#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WowLuaVM.h"
#include "WowFrameManager.h"
#include "WowEventSystem.h"
#include "WowUIManager.generated.h"

class FMpqManager;
class UCanvasPanel;

/**
 * GameInstanceSubsystem that owns the WoW UI systems (Lua VM, FrameManager, EventSystem).
 * Initialize() creates the Lua VM and wires the systems.
 * LoadUI() loads FrameXML and addons from MPQ once the data is available.
 */
UCLASS()
class WOWUI_API UWowUIManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	UWowUIManager();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Load FrameXML and addons from MPQ — call once MpqManager is available */
	void LoadUI(FMpqManager* Mpq);

	/** Set the root UMG canvas for frame rendering */
	void SetRootCanvas(UCanvasPanel* Canvas);

	/** Accessors */
	FWowLuaVM* GetLuaVM() const { return LuaVM.Get(); }
	FWowFrameManager* GetFrameManager() const { return FrameManager.Get(); }
	FWowEventSystem* GetEventSystem() const { return EventSystem.Get(); }

	bool IsUILoaded() const { return bUILoaded; }

private:
	TUniquePtr<FWowLuaVM> LuaVM;
	TUniquePtr<FWowFrameManager> FrameManager;
	TUniquePtr<FWowEventSystem> EventSystem;

	bool bUILoaded = false;
};
