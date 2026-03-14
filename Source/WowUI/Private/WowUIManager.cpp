#include "WowUIManager.h"
#include "WowLuaVM.h"
#include "WowFrameManager.h"
#include "WowEventSystem.h"
#include "WowFrameXmlParser.h"
#include "WowAddonLoader.h"
#include "LuaApi/LuaApiRegistry.h"
#include "Mpq/MpqManager.h"
#include "Components/CanvasPanel.h"

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
	static FWowLuaContext UIContext;
	UIContext.EventSystem = EventSystem.Get();
	UIContext.FrameManager = FrameManager.Get();
	WowLuaApi::SetContext(LuaVM->GetState(), &UIContext);

	UE_LOG(LogWowUIManager, Log, TEXT("WoW UI Manager initialized (Lua VM + FrameManager + EventSystem)"));
}

void UWowUIManager::Deinitialize()
{
	if (LuaVM)
	{
		LuaVM->Shutdown();
	}

	EventSystem.Reset();
	FrameManager.Reset();
	LuaVM.Reset();

	bUILoaded = false;

	UE_LOG(LogWowUIManager, Log, TEXT("WoW UI Manager deinitialized"));

	Super::Deinitialize();
}

void UWowUIManager::SetRootCanvas(UCanvasPanel* Canvas)
{
	if (FrameManager && Canvas)
	{
		FrameManager->Initialize(Canvas);
		UE_LOG(LogWowUIManager, Log, TEXT("Root canvas set for FrameManager"));
	}
}

void UWowUIManager::LoadUI(FMpqManager* Mpq)
{
	if (!Mpq || !LuaVM || !LuaVM->IsInitialized())
	{
		UE_LOG(LogWowUIManager, Error, TEXT("Cannot load UI: null MPQ or Lua VM not initialized"));
		return;
	}

	if (bUILoaded)
	{
		UE_LOG(LogWowUIManager, Warning, TEXT("UI already loaded, skipping"));
		return;
	}

	UE_LOG(LogWowUIManager, Log, TEXT("Loading WoW UI from MPQ..."));

	// 1. Load FrameXML (Interface/FrameXML/FrameXML.toc) — the core UI system
	TArray<FWowXmlDirective> FrameXmlDirectives = FWowFrameXmlParser::LoadFrameXml(Mpq);

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
			// Includes within FrameXML should already be resolved by the parser
			break;
		}
		case FWowXmlDirective::EType::Font:
		{
			UE_LOG(LogWowUIManager, Verbose, TEXT("FrameXML Font: %s"), *Dir.FontName);
			break;
		}
		}
	}

	UE_LOG(LogWowUIManager, Log, TEXT("FrameXML: processed %d directives, created %d frames"),
		FrameXmlDirectives.Num(), FrameCount);

	// 2. Load addons (Blizzard + user addons) in dependency order
	FWowAddonLoader::LoadAllAddons(Mpq, LuaVM.Get(), FrameManager.Get(), EventSystem.Get());

	// 3. Fire PLAYER_ENTERING_WORLD-like init event so frames can set up
	EventSystem->FireEvent(TEXT("ADDON_LOADED"));
	EventSystem->FireEvent(TEXT("PLAYER_LOGIN"));

	bUILoaded = true;

	UE_LOG(LogWowUIManager, Log, TEXT("WoW UI loaded successfully (%d frames total)"),
		FrameManager ? FrameManager->GetFrameCount() : 0);
}
