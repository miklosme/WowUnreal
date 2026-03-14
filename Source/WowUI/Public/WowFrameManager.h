#pragma once
#include "CoreMinimal.h"
#include "WowFrameTypes.h"

class UCanvasPanel;
class UWidget;
class UUserWidget;
class FWowEventSystem;

/**
 * Manages WoW UI frames, mapping them to UMG widgets.
 */
class WOWUI_API FWowFrameManager
{
public:
	FWowFrameManager();

	/** Initialize with a root canvas panel to add widgets to */
	void Initialize(UCanvasPanel* RootCanvas);

	/** Set the event system for script compilation on frame creation */
	void SetEventSystem(FWowEventSystem* InEventSystem) { EventSystem = InEventSystem; }

	/** Create a frame from a parsed definition */
	int64 CreateFrame(const FWowFrameDef& Def);

	/** Register a template for inheritance */
	void RegisterTemplate(const FString& Name, const FWowFrameDef& Def);

	/** Find a frame by name */
	int64 FindFrame(const FString& Name) const;

	/** Get frame name by handle */
	FString GetFrameName(int64 Handle) const;

	/** Get the frame definition for a handle (nullptr if not found) */
	const FWowFrameDef* GetFrameDef(int64 Handle) const;

	/** Get mutable frame definition for a handle (nullptr if not found) */
	FWowFrameDef* GetMutableFrameDef(int64 Handle);

	/** Show/hide a frame */
	void SetFrameVisible(int64 Handle, bool bVisible);

	/** Check if a frame is currently visible */
	bool IsFrameVisible(int64 Handle) const;

	/** Set frame size and update widget */
	void SetFrameSize(int64 Handle, float W, float H);

	/** Set frame anchors and reposition widget */
	void SetFrameAnchors(int64 Handle, const TArray<FWowAnchor>& NewAnchors);

	/** Clear all anchors on a frame */
	void ClearFrameAnchors(int64 Handle);

	/** Set frame alpha */
	void SetFrameAlpha(int64 Handle, float Alpha);

	/** Set frame strata and level */
	void SetFrameStrata(int64 Handle, EWowFrameStrata Strata);
	void SetFrameLevel(int64 Handle, int32 Level);

	/** Get the parent handle of a frame (-1 if none) */
	int64 GetParentHandle(int64 Handle) const;

	/** Get child handles of a frame */
	TArray<int64> GetChildHandles(int64 Handle) const;

	/** Get frame count */
	int32 GetFrameCount() const { return Frames.Num(); }

private:
	struct FFrameEntry
	{
		FWowFrameDef Def;
		TWeakObjectPtr<UWidget> Widget;
		int64 ParentHandle = -1;
	};

	TMap<int64, FFrameEntry> Frames;
	TMap<FString, int64> NameToHandle;
	TMap<FString, FWowFrameDef> Templates;

	FWowEventSystem* EventSystem = nullptr;
	TWeakObjectPtr<UCanvasPanel> RootCanvas;
	int64 NextHandle = 1;

	UWidget* CreateWidgetForFrame(const FWowFrameDef& Def);

	/** Create layer content (textures and fontstrings) inside a frame's canvas */
	void CreateLayerContent(UCanvasPanel* Container, const FWowFrameDef& Def);

	/** Resolve template inheritance for a frame definition */
	FWowFrameDef ResolveInherits(const FWowFrameDef& Def) const;

	/** Merge template attributes into a target definition (template first, then target overrides) */
	static void MergeTemplate(FWowFrameDef& Target, const FWowFrameDef& Template);

	/** Apply anchor positioning to a widget's canvas slot */
	void ApplyAnchors(UWidget* Widget, const FWowFrameDef& Def);

	/** Apply anchor positioning to a widget within a parent canvas */
	static void ApplyElementAnchors(UWidget* Widget, UCanvasPanel* Parent, const TArray<FWowAnchor>& Anchors, float Width, float Height);
};
