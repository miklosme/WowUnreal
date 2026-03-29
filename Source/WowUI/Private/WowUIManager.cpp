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

DEFINE_LOG_CATEGORY_STATIC(LogWowUIManager, Log, All);

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

	// 1. Load FrameXML (Interface/FrameXML/FrameXML.toc) — the core UI system
	TArray<FWowXmlDirective> FrameXmlDirectives = FWowFrameXmlParser::LoadFrameXml(Mpq);
	UE_LOG(LogWowUIManager, Warning, TEXT("LoadFrameXml returned %d directives"), FrameXmlDirectives.Num());

	int32 FrameCount = 0;
	for (const FWowXmlDirective& Dir : FrameXmlDirectives)
	{
		switch (Dir.Type)
		{
		case FWowXmlDirective::EType::Script:
		{
			if (!Dir.FilePath.IsEmpty())
			{
				FString ScriptPath = TEXT("Interface\\FrameXML\\") + Dir.FilePath;
				TArray<uint8> ScriptData;
				if (Mpq->ReadFile(ScriptPath, ScriptData))
				{
					LuaVM->ExecuteBuffer(ScriptData, ScriptPath);
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
		{
			if (!Dir.FilePath.IsEmpty())
			{
				FString IncPath = TEXT("Interface\\FrameXML\\") + Dir.FilePath;
				TArray<uint8> IncData;
				if (Mpq->ReadFile(IncPath, IncData))
				{
					TArray<FWowXmlDirective> IncDirs = FWowFrameXmlParser::ParseXml(IncData, IncPath);
					for (const FWowXmlDirective& IncDir : IncDirs)
					{
						if (IncDir.Type == FWowXmlDirective::EType::Script && !IncDir.FilePath.IsEmpty())
						{
							FString IncScriptPath = TEXT("Interface\\FrameXML\\") + IncDir.FilePath;
							TArray<uint8> ScriptData;
							if (Mpq->ReadFile(IncScriptPath, ScriptData))
							{
								LuaVM->ExecuteBuffer(ScriptData, IncScriptPath);
							}
						}
						else if (IncDir.Type == FWowXmlDirective::EType::Frame && FrameManager)
						{
							FrameManager->CreateFrame(IncDir.FrameDef);
							FrameCount++;
						}
					}
				}
			}
			break;
		}
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

	UE_LOG(LogWowUIManager, Log, TEXT("FrameXML: processed %d directives, created %d frames"),
		FrameXmlDirectives.Num(), FrameCount);

	// 2. Fire VARIABLES_LOADED before addons load (WoW 3.3.5 boot order)
	EventSystem->FireEvent(TEXT("VARIABLES_LOADED"));

	// 3. Load addons (Blizzard + user addons) in dependency order
	// Each addon fires ADDON_LOADED(addonName) after loading
	FWowAddonLoader::LoadAllAddons(Mpq, LuaVM.Get(), FrameManager.Get(), EventSystem.Get());

	// 4. Fire login events in correct WoW order
	EventSystem->FireEvent(TEXT("PLAYER_LOGIN"));
	EventSystem->FireEvent(TEXT("PLAYER_ENTERING_WORLD"));

	bUILoaded = true;

	UE_LOG(LogWowUIManager, Log, TEXT("WoW UI loaded successfully (%d frames total)"),
		FrameManager ? FrameManager->GetFrameCount() : 0);

	// Dump key frame layout for debugging
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

void UWowUIManager::UpdateInventory()
{
	if (!InventoryManager.IsValid())
		return;

	// For demo purposes, create a dummy player entity and entity manager
	FWowPlayerEntity DummyPlayer;
	FWowEntityManager DummyEntityManager;

	InventoryManager->UpdateFromPlayerEntity(DummyPlayer, DummyEntityManager);
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

	// Character panel connection manager is set when the panel is created
	// in WowGameplayController, not here.
}
