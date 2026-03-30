#pragma once
#include "CoreMinimal.h"
#include "WowFrameTypes.h"

class UCanvasPanel;
class UWidget;
class UUserWidget;
class UImage;
class UButton;
class UEditableTextBox;
class UProgressBar;
class USlider;
class UTextBlock;
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

	/** Get the UButton child widget for a frame (nullptr if not found or invalid) */
	UButton* GetButtonWidget(int64 Handle) const;

	/** Get the UEditableTextBox child widget for a frame (nullptr if not found or invalid) */
	UEditableTextBox* GetEditBoxWidget(int64 Handle) const;

	/** Get the USlider child widget for a frame (nullptr if not found or invalid) */
	USlider* GetSliderWidget(int64 Handle) const;

	/** Get the primary text widget associated with a frame (button label, etc.) */
	UTextBlock* GetPrimaryTextWidget(int64 Handle) const;

	/** Get a named FontString's UTextBlock widget */
	UTextBlock* GetNamedFontString(const FString& Name) const;

	/** Runtime input flags used by the UI hit-test and controller bridge */
	void SetFrameMouseEnabled(int64 Handle, bool bEnabled);
	void SetFrameKeyboardEnabled(int64 Handle, bool bEnabled);
	void SetFrameMouseWheelEnabled(int64 Handle, bool bEnabled);
	void SetFrameRegisteredClicks(int64 Handle, const TArray<FString>& ClickTypes);
	bool IsFrameMouseEnabled(int64 Handle) const;
	bool IsFrameKeyboardEnabled(int64 Handle) const;
	bool IsFrameMouseWheelEnabled(int64 Handle) const;
	bool IsFrameClickRegistered(int64 Handle, const FString& Button, bool bMouseDown) const;

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
	bool IsMouseOverFrame(int64 Handle, float TopOffset = 0.f, float BottomOffset = 0.f, float LeftOffset = 0.f, float RightOffset = 0.f) const;

	/** Dispatch a mouse click event to a frame's Lua scripts.
	 *  Button: "LeftButton", "RightButton", "MiddleButton" */
	void DispatchMouseDown(int64 Handle, const FString& Button);
	void DispatchMouseUp(int64 Handle, const FString& Button);
	void DispatchClick(int64 Handle, const FString& Button, bool bMouseDown);
	bool DispatchReceiveDrag(int64 Handle);

	/** Update mouse hover state — call once per frame from Tick.
	 *  Dispatches OnEnter/OnLeave events to Lua scripts. */
	void UpdateMouseHover(float ScreenX, float ScreenY);

	/** Poll EditBox/Slider widgets for changes and dispatch Lua events.
	 *  Call once per frame from Tick. */
	void TickWidgetEvents();

	/** Dispatch OnEnterPressed to the currently focused EditBox (if any).
	 *  Returns true if an EditBox was focused and received the event. */
	bool DispatchEditBoxEnterPressed();

	/** Dispatch OnEscapePressed to the currently focused EditBox (if any).
	 *  Returns true if an EditBox was focused and received the event. */
	bool DispatchEditBoxEscapePressed();

	/** Set the UI scale factor (typically calculated from viewport size vs WoW's base resolution) */
	void SetUIScale(float InScale) { UIScale = InScale; }

private:
	struct FFrameEntry
	{
		FWowFrameDef Def;
		TWeakObjectPtr<UWidget> Widget;
		int64 ParentHandle = -1;
		bool bMouseEnabled = false;
		bool bKeyboardEnabled = false;
		bool bMouseWheelEnabled = false;
		bool bHasExplicitClickRegistration = false;
		TSet<FString> RegisteredClicks;
	};

	/** Handle of the frame currently under the mouse cursor (-1 if none) */
	int64 HoverFrameHandle = -1;

	/** Last-known text for each EditBox, used to detect changes for OnTextChanged dispatch */
	TMap<int64, FString> EditBoxLastText;

	/** Last-known value for each Slider, used to detect changes for OnValueChanged dispatch */
	TMap<int64, float> SliderLastValue;

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

	/** Named fontstrings (from XML FontString name="Foo") → UTextBlock widget */
	TMap<FString, TWeakObjectPtr<UTextBlock>> NamedFontStringWidgets;

	/** StatusBar frames → UProgressBar child widgets */
	TMap<int64, TWeakObjectPtr<UProgressBar>> StatusBarWidgets;

	/** Interactive control widgets owned by frame containers */
	TMap<int64, TWeakObjectPtr<UButton>> ButtonWidgets;
	TMap<int64, TWeakObjectPtr<UEditableTextBox>> EditBoxWidgets;
	TMap<int64, TWeakObjectPtr<USlider>> SliderWidgets;
	TMap<int64, TWeakObjectPtr<UTextBlock>> PrimaryTextWidgets;

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
	void CreateLayerContent(UCanvasPanel* Container, const FWowFrameDef& Def, int64 OwnerHandle);

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
