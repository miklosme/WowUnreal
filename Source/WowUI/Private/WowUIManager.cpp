#include "WowUIManager.h"
#include "WowLuaVM.h"
#include "WowFrameManager.h"
#include "WowEventSystem.h"
#include "WowFontManager.h"
#include "WowFrameXmlParser.h"
#include "WowAddonLoader.h"
#include "LuaApi/LuaApiRegistry.h"
#include "Mpq/MpqManager.h"
#include "Components/CanvasPanel.h"
#include "WowEntity.h"
#include "WowEntityManager.h"
#include "WowConnectionManager.h"
#include "Widgets/SOverlay.h"
#include "Framework/Application/SlateApplication.h"

#if __has_include("lua.h")
extern "C" {
#include "lua.h"
#include "lauxlib.h"
}
#define HAS_LUA 1
#else
#define HAS_LUA 0
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWowUIManager, Log, All);

namespace WowLuaApi
{
	void SetAddonLoaderContext(FMpqManager* Mpq, FWowLuaVM* LuaVM, FWowFrameManager* FrameManager, FWowEventSystem* EventSystem);
}

namespace
{
FString ResolveFrameXmlAssetPath(const FString& InPath)
{
	FString ResolvedPath = InPath;
	ResolvedPath.ReplaceInline(TEXT("/"), TEXT("\\"));

	if (!ResolvedPath.StartsWith(TEXT("Interface\\"), ESearchCase::IgnoreCase))
	{
		ResolvedPath = TEXT("Interface\\FrameXML\\") + ResolvedPath;
	}

	return ResolvedPath;
}

bool TryReadUiAssetFile(FMpqManager* Mpq, const FString& InPath, TArray<uint8>& OutData)
{
	if (!Mpq)
	{
		return false;
	}

	FString NormalizedPath = InPath;
	NormalizedPath.ReplaceInline(TEXT("/"), TEXT("\\"));

	TArray<FString> Candidates;
	Candidates.AddUnique(NormalizedPath);
	Candidates.AddUnique(NormalizedPath.ToLower());
	Candidates.AddUnique(NormalizedPath.Replace(TEXT("\\"), TEXT("/")));
	Candidates.AddUnique(NormalizedPath.ToLower().Replace(TEXT("\\"), TEXT("/")));

	for (const FString& Candidate : Candidates)
	{
		if (Mpq->ReadFile(Candidate, OutData))
		{
			return true;
		}
	}

	return false;
}
}

UWowUIManager::UWowUIManager()
{
}

void UWowUIManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Create core UI systems
	LuaVM = MakeUnique<FWowLuaVM>();
	FrameManager = MakeUnique<FWowFrameManager>();
	EventSystem = MakeUnique<FWowEventSystem>();
	FontManager = MakeUnique<FWowFontManager>();
	InventoryManager = MakeShareable(new FWowInventoryManager());

	// Initialize the Lua VM
	if (!LuaVM->Initialize())
	{
		UE_LOG(LogWowUIManager, Error, TEXT("Failed to initialize Lua VM"));
		return;
	}

	// Wire the systems together
	EventSystem->SetLuaVM(LuaVM.Get());
	EventSystem->SetFrameManager(FrameManager.Get());
	FrameManager->SetEventSystem(EventSystem.Get());

	// Set up the Lua context so API functions can access game systems
	// EntityManager and ConnectionManager will be set later when networking is ready
	UIContext = new FWowLuaContext();
	UIContext->EventSystem = EventSystem.Get();
	UIContext->FrameManager = FrameManager.Get();
	WowLuaApi::SetContext(LuaVM->GetState(), UIContext);

	UE_LOG(LogWowUIManager, Log, TEXT("WoW UI Manager initialized (Lua VM + FrameManager + EventSystem)"));
}

void UWowUIManager::Deinitialize()
{
	if (LuaVM && LuaVM->GetState())
	{
		WowLuaApi::ClearContext(LuaVM->GetState());
	}

	delete UIContext;
	UIContext = nullptr;

	if (LuaVM)
	{
		LuaVM->Shutdown();
	}

	EventSystem.Reset();
	FrameManager.Reset();
	FontManager.Reset();
	LuaVM.Reset();

	bUILoaded = false;

	UE_LOG(LogWowUIManager, Log, TEXT("WoW UI Manager deinitialized"));

	Super::Deinitialize();
}


void UWowUIManager::LoadUI(FMpqManager* Mpq, FWowAssetCache* AssetCache)
{
	if (!Mpq || !AssetCache || !LuaVM || !LuaVM->IsInitialized())
	{
		UE_LOG(LogWowUIManager, Error, TEXT("Cannot load UI: null MPQ/AssetCache or Lua VM not initialized"));
		return;
	}

	if (bUILoaded)
	{
		UE_LOG(LogWowUIManager, Warning, TEXT("UI already loaded, skipping"));
		return;
	}

	UE_LOG(LogWowUIManager, Log, TEXT("Loading WoW UI from MPQ..."));

	// 0. Initialize font manager first (required for FrameXML font rendering)
	if (!FontManager->Initialize(Mpq))
	{
		UE_LOG(LogWowUIManager, Error, TEXT("Failed to initialize font manager"));
		return;
	}

	// Wire dependencies to frame manager
	FrameManager->SetFontManager(FontManager.Get());
	FrameManager->SetMpqManager(Mpq);
	FrameManager->SetAssetCache(AssetCache);

	if (FrameManager->FindFrame(TEXT("UIParent")) == -1)
	{
		UE_LOG(LogWowUIManager, Error, TEXT("Cannot load UI before SetRootCanvas initializes the FrameManager/UIParent"));
		return;
	}

	WowLuaApi::SetAddonLoaderContext(Mpq, LuaVM.Get(), FrameManager.Get(), EventSystem.Get());

	// 1. Load FrameXML (Interface/FrameXML/FrameXML.toc) — the core UI system
	TArray<FWowXmlDirective> FrameXmlDirectives = FWowFrameXmlParser::LoadFrameXml(Mpq);
	UE_LOG(LogWowUIManager, Log, TEXT("LoadFrameXml returned %d directives"), FrameXmlDirectives.Num());

	int32 FrameCount = 0;
	if (EventSystem)
	{
		EventSystem->BeginOnLoadBatch();
	}

	for (const FWowXmlDirective& Dir : FrameXmlDirectives)
	{
		switch (Dir.Type)
		{
		case FWowXmlDirective::EType::Script:
		{
			if (!Dir.FilePath.IsEmpty())
			{
				const FString ScriptPath = ResolveFrameXmlAssetPath(Dir.FilePath);
				TArray<uint8> ScriptData;
				if (TryReadUiAssetFile(Mpq, ScriptPath, ScriptData))
				{
					LuaVM->ExecuteBuffer(ScriptData, ScriptPath);
				}
				else
				{
					UE_LOG(LogWowUIManager, Warning, TEXT("Failed to load FrameXML script: %s"), *ScriptPath);
				}
			}
			break;
		}
		case FWowXmlDirective::EType::Frame:
		{
			if (FrameManager)
			{
				FrameManager->CreateFrame(Dir.FrameDef);
				FrameCount++;
			}
			break;
		}
		case FWowXmlDirective::EType::Include:
			// LoadFrameXml now expands includes recursively; keep this as a no-op guard.
			break;
		case FWowXmlDirective::EType::Font:
		{
			if (FontManager && !Dir.FontName.IsEmpty())
			{
				FontManager->RegisterFontMapping(Dir.FontName, Dir.FontInherits, static_cast<int32>(Dir.FontHeight));
				UE_LOG(LogWowUIManager, Verbose, TEXT("Registered FrameXML Font: %s"), *Dir.FontName);
			}
			break;
		}
		}
	}

	if (EventSystem)
	{
		EventSystem->EndOnLoadBatch();
	}

	UE_LOG(LogWowUIManager, Log, TEXT("FrameXML: processed %d directives, created %d frames"),
		FrameXmlDirectives.Num(), FrameCount);

	// 2. Load startup addons (Blizzard + user addons) in dependency order.
	// Each addon fires ADDON_LOADED(addonName) as it finishes loading.
	// Each addon fires ADDON_LOADED(addonName) after loading
	FWowAddonLoader::LoadAllAddons(Mpq, LuaVM.Get(), FrameManager.Get(), EventSystem.Get());

	if (FrameManager)
	{
		FrameManager->SyncChildVisibility();
	}

	// 2.5. Post-load Lua fixups: initialize fields that OnLoad should have set
	// but didn't (e.g., frame templates missing from MPQ XML, OnLoad not reaching
	// child frames, etc.). This runs after all frames and addons are loaded.
#if HAS_LUA
	if (LuaVM && LuaVM->IsInitialized())
	{
		const char* PostLoadFixup = R"LUA(
			-- BuffFrame: ensure OnLoad fields exist (OnLoad may have run on wrong self)
			if BuffFrame and BuffFrame.BuffFrameUpdateTime == nil then
				BuffFrame.BuffFrameUpdateTime = 0
				BuffFrame.BuffFrameFlashTime = 0
				BuffFrame.BuffFrameFlashState = 1
				BuffFrame.BuffAlphaValue = 1
				BuffFrame.numEnchants = 0
				BuffFrame.numConsolidated = 0
			end
			-- TemporaryEnchantFrame: needs TempEnchant button references
			if TemporaryEnchantFrame and TemporaryEnchantFrame.TemporaryEnchantFrameUpdateTime == nil then
				TemporaryEnchantFrame.TemporaryEnchantFrameUpdateTime = 0
			end
			-- CombatFeedback: initialize feedbackText directly on frames that have
			-- CombatFeedback_OnUpdate but never had CombatFeedback_Initialize called.
			-- The OnUpdate runs on these frames, not their child HitIndicator frames.
			local function EnsureCombatFeedback(frameName)
				local f = _G[frameName]
				if f and not f.feedbackText then
					-- Create a stub FontString that safely no-ops all method calls
					local noop = function() end
					local fs = setmetatable({ __objectType = "FontString" }, { __index = function(_, key)
						if key == "IsShown" or key == "IsVisible" then return function() return false end end
						return noop
					end })
					f.feedbackText = fs
					f.feedbackFontHeight = 20
				end
			end
			EnsureCombatFeedback("PlayerFrame")
			EnsureCombatFeedback("TargetFrame")
			EnsureCombatFeedback("PetFrame")
		)LUA";

		lua_State* L = LuaVM->GetState();
		if (L && luaL_dostring(L, PostLoadFixup) != 0)
		{
			UE_LOG(LogWowUIManager, Warning, TEXT("Post-load fixup error: %hs"), lua_tostring(L, -1));
			lua_pop(L, 1);
		}
	}
#endif

	// 3. Fire post-addon startup events in WoW order.
	EventSystem->FireEvent(TEXT("VARIABLES_LOADED"));
	EventSystem->FireEvent(TEXT("PLAYER_LOGIN"));
	EventSystem->FireEvent(TEXT("PLAYER_ENTERING_WORLD"));

	// Second visibility pass — catches addon frames (GMChatUI etc.) created after first SyncChildVisibility
	if (FrameManager)
	{
		FrameManager->SyncChildVisibility();
	}

	bUILoaded = true;

	UE_LOG(LogWowUIManager, Log, TEXT("WoW UI loaded successfully (%d frames total)"),
		FrameManager ? FrameManager->GetFrameCount() : 0);

	if (FrameManager)
	{
		FrameManager->DebugDumpLayout();
	}
}

void UWowUIManager::SetRootCanvas(UCanvasPanel* Canvas)
{
	// First, initialize the frame manager with the canvas if provided
	if (FrameManager && Canvas)
	{
		FrameManager->Initialize(Canvas);
		UE_LOG(LogWowUIManager, Log, TEXT("Root canvas set for FrameManager"));
	}

	// Create the UI overlay for Slate widgets
	if (FSlateApplication::IsInitialized())
	{
		UIOverlay = SNew(SOverlay)
			.Visibility(EVisibility::SelfHitTestInvisible); // Pass clicks through to 3D world

		// Create bag window (right side, padded from edge)
		BagWindow = SNew(SWowBagWindow, InventoryManager);
		UIOverlay->AddSlot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.Padding(0, 0, 20, 0)  // 20px from right edge
		[
			BagWindow.ToSharedRef()
		];

		BankWindow = SNew(SWowBankWindow, InventoryManager);
		UIOverlay->AddSlot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.Padding(0, 0, 220, 0)
		[
			BankWindow.ToSharedRef()
		];

		// Create character panel (left side, padded from edge)
		// ConnectionManager is set later via SetConnectionManager, for now pass nullptr
		CharacterPanel = SNew(SWowCharacterPanel, InventoryManager, nullptr);
		UIOverlay->AddSlot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(20, 0, 0, 0)  // 20px from left edge
		[
			CharacterPanel.ToSharedRef()
		];

		// Add the overlay to the viewport
		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->AddViewportWidgetContent(UIOverlay.ToSharedRef(), 80); // Z-order above main UI
		}

		UE_LOG(LogWowUIManager, Log, TEXT("Created bag and character panel UI"));
	}
}

void UWowUIManager::ToggleBagWindow()
{
	if (BagWindow.IsValid())
	{
		BagWindow->ToggleVisibility();
	}
}

void UWowUIManager::ToggleCharacterPanel()
{
	if (CharacterPanel.IsValid())
	{
		CharacterPanel->ToggleVisibility();
	}
}

void UWowUIManager::ShowBankWindow()
{
	UpdateInventory();

	if (BankWindow.IsValid())
	{
		BankWindow->Show();
	}
}

void UWowUIManager::HideBankWindow()
{
	if (BankWindow.IsValid())
	{
		BankWindow->Hide();
	}
}

void UWowUIManager::UpdateInventory()
{
	if (!InventoryManager.IsValid() || !ConnectionManager)
		return;

	FWowPlayerEntity* LocalPlayer = ConnectionManager->PacketHandler.EntityManager.GetLocalPlayer();
	if (!LocalPlayer)
	{
		UE_LOG(LogWowUIManager, Verbose, TEXT("UpdateInventory skipped: local player entity not ready"));
		return;
	}

	InventoryManager->UpdateFromPlayerEntity(*LocalPlayer, ConnectionManager->PacketHandler.EntityManager);
}

void UWowUIManager::SetConnectionManager(UWowConnectionManager* InConnectionManager)
{
	ConnectionManager = InConnectionManager;

	// Update the Lua context with connection manager and entity manager
	if (UIContext && ConnectionManager)
	{
		UIContext->ConnectionManager = ConnectionManager;
		UIContext->EntityManager = &ConnectionManager->PacketHandler.EntityManager;

		UE_LOG(LogWowUIManager, Log, TEXT("Updated Lua context with ConnectionManager and EntityManager"));
	}

	if (InventoryManager.IsValid())
	{
		InventoryManager->SetConnectionManager(ConnectionManager);
	}

	if (CharacterPanel.IsValid())
	{
		CharacterPanel->SetConnectionManager(ConnectionManager);
	}
}
