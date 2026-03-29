#pragma once
#include "CoreMinimal.h"
#include "WowFrameTypes.h"

class UCanvasPanel;
class UWidget;
class UUserWidget;
class UImage;
class UProgressBar;
class FWowEventSystem;
class FWowFontManager;
class FMpqManager;
class FWowAssetCache;
class UTexture2D;

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

	/** Set the font manager for font rendering */
	void SetFontManager(FWowFontManager* InFontManager) { FontManager = InFontManager; }

	/** Set MPQ manager for loading texture files */
	void SetMpqManager(FMpqManager* InMpqManager) { MpqManager = InMpqManager; }

	/** Set asset cache for texture caching */
	void SetAssetCache(FWowAssetCache* InAssetCache) { AssetCache = InAssetCache; }

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

	/** Get the UWidget for a frame handle (nullptr if not found or invalid) */
	UWidget* GetWidgetForHandle(int64 Handle) const;

	/** Load a BLP texture from MPQ and cache it */
	UTexture2D* LoadUITexture(const FString& TexturePath);

	/** Create a simple test frame for debugging */
	int64 CreateDebugFrame(const FString& Name, float Width, float Height, float X = 0.f, float Y = 0.f);

	/** Get the current UI scale factor */
	float GetUIScale() const { return UIScale; }

	/** Debug: dump key frame layout info to log */
	void DebugDumpLayout() const;

	/** Post-load pass: hide children of hidden parents (catches frames made visible
	 *  before their parent's OnLoad script hid the parent) */
	void SyncChildVisibility();

	/** Find the topmost interactive frame under a screen position (pixels).
	 *  Returns frame handle or -1 if no interactive frame found. */
	int64 HitTestFrames(float ScreenX, float ScreenY) const;

	/** Dispatch a mouse click event to a frame's Lua scripts.
	 *  Button: "LeftButton", "RightButton", "MiddleButton" */
	void DispatchMouseDown(int64 Handle, const FString& Button);
	void DispatchMouseUp(int64 Handle, const FString& Button);
	void DispatchClick(int64 Handle, const FString& Button);

	/** Set the UI scale factor (typically calculated from viewport size vs WoW's base resolution) */
	void SetUIScale(float InScale) { UIScale = InScale; }

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
	FWowFontManager* FontManager = nullptr;
	FMpqManager* MpqManager = nullptr;
	FWowAssetCache* AssetCache = nullptr;
	TWeakObjectPtr<UCanvasPanel> RootCanvas;
	int64 NextHandle = 1;

	/** UI scale factor to convert WoW coordinates to UMG design pixels (includes DPI adjustment) */
	float UIScale = 1.0f;

	/** Raw viewport-to-WoW scale: viewportHeight / 768. Used for converting
	 *  viewport pixel positions (e.g. mouse cursor) to WoW coordinates. */
	float RawViewportScale = 1.0f;

	/** Cache for loaded UI textures to avoid loading the same BLP twice */
	TMap<FString, TWeakObjectPtr<UTexture2D>> TextureCache;

	/** Named texture regions (from XML Texture name="Foo") → UImage widget */
	TMap<FString, TWeakObjectPtr<UImage>> NamedTextureWidgets;

	/** StatusBar frames → UProgressBar child widgets */
	TMap<int64, TWeakObjectPtr<UProgressBar>> StatusBarWidgets;

public:
	/** Get named texture region's UImage widget */
	UImage* GetNamedTexture(const FString& Name) const
	{
		const TWeakObjectPtr<UImage>* Found = NamedTextureWidgets.Find(Name);
		return (Found && Found->IsValid()) ? Found->Get() : nullptr;
	}

	/** Get the UProgressBar widget for a StatusBar frame (nullptr if not found or invalid) */
	UProgressBar* GetStatusBarWidget(int64 Handle) const
	{
		const TWeakObjectPtr<UProgressBar>* Found = StatusBarWidgets.Find(Handle);
		return (Found && Found->IsValid()) ? Found->Get() : nullptr;
	}

private:

	/** Frame position/size tracking for anchor resolution */
	struct FFrameRect
	{
		float X, Y, W, H;
		FFrameRect(float InX = 0.f, float InY = 0.f, float InW = 0.f, float InH = 0.f)
			: X(InX), Y(InY), W(InW), H(InH) {}
	};
	TMap<FString, FFrameRect> FrameRects;

	UWidget* CreateWidgetForFrame(const FWowFrameDef& Def, int64 Handle, int64 ParentHandle = -1);

	/** Create layer content (textures and fontstrings) inside a frame's canvas */
	void CreateLayerContent(UCanvasPanel* Container, const FWowFrameDef& Def);

	/** Resolve template inheritance for a frame definition */
	FWowFrameDef ResolveInherits(const FWowFrameDef& Def) const;

	/** Resolve $parent references in frame names and anchor RelativeTo fields */
	void ResolveParentReferences(FWowFrameDef& Def, const FString& ParentName);

	/** Merge template attributes into a target definition (template first, then target overrides) */
	static void MergeTemplate(FWowFrameDef& Target, const FWowFrameDef& Template);

	/** Apply anchor positioning to a widget's canvas slot */
	void ApplyAnchors(UWidget* Widget, const FWowFrameDef& Def);

	/** Apply anchor positioning to a widget within a parent canvas.
	 *  ParentFrameW/H are the parent frame's dimensions in WoW coords (needed because GetDesiredSize returns 0 at creation time). */
	void ApplyElementAnchors(UWidget* Widget, UCanvasPanel* Parent, const TArray<FWowAnchor>& Anchors, float Width, float Height, bool bSetAllPoints = false, float ParentFrameW = 0.f, float ParentFrameH = 0.f);

	/** Get the screen position offset of an anchor point on a frame */
	static FVector2D GetAnchorPointOffset(EWowAnchorPoint Point, float Width, float Height);

	/** Get the frame rect for a named frame, or UIParent default */
	FFrameRect GetFrameRect(const FString& FrameName);

	/** Create backdrop (9-slice background) for a frame */
	void CreateBackdrop(UCanvasPanel* Container, const FWowBackdrop& Backdrop);
};
