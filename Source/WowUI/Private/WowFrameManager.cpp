#include "WowFrameManager.h"
#include "WowEventSystem.h"
#include "WowFontManager.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/ProgressBar.h"
#include "Components/Slider.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Slate/SlateBrushAsset.h"
#include "Mpq/MpqManager.h"
#include "WowAssetCache.h"
#include "Formats/BlpParser.h"
#include "WowTextureFactory.h"
#include "Styling/CoreStyle.h"
#include "Slate/WidgetTransform.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/UserInterfaceSettings.h"
DEFINE_LOG_CATEGORY_STATIC(LogWowFrame, Log, All);

namespace
{
FString BuildParentFrameAliasName(const FString& ParentName, const FString& ChildName)
{
	if (!ParentName.EndsWith(TEXT("Frame")) || ChildName.StartsWith(ParentName) || ParentName.Len() <= 5)
	{
		return FString();
	}

	const FString ParentBase = ParentName.LeftChop(5);
	if (ParentBase.IsEmpty() || !ChildName.StartsWith(ParentBase))
	{
		return FString();
	}

	const FString Suffix = ChildName.Mid(ParentBase.Len());
	if (Suffix.IsEmpty())
	{
		return FString();
	}

	return ParentName + Suffix;
}
}

FWowFrameManager::FWowFrameManager()
{
}

void FWowFrameManager::Initialize(UCanvasPanel* InRootCanvas)
{
	RootCanvas = InRootCanvas;

	if (InRootCanvas)
	{
		// WoW's base UI height (the reference for scaling)
		const float WowBaseHeight = 768.0f;

		// Get actual viewport size
		FVector2D ViewportSize = FVector2D(1024.0f, 768.0f); // Default fallback

		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->GetViewportSize(ViewportSize);
		}

		// Guard against viewport not ready (returns 0,0 early in startup)
		if (ViewportSize.X < 100.f || ViewportSize.Y < 100.f)
		{
			ViewportSize = FVector2D(1920.0f, 1080.0f); // Sensible default
			UE_LOG(LogWowFrame, Warning, TEXT("Viewport size not available yet, defaulting to %.0fx%.0f"), ViewportSize.X, ViewportSize.Y);
		}

		// UMG applies its own DPI scaling curve (Project Settings → Engine → User Interface).
		// At 720p the default curve yields ~0.67, so a widget at pixel 670 renders at
		// 670×0.67 ≈ 450 — nowhere near the bottom. We must divide our scale by the
		// UMG viewport scale so positions come out correct after DPI scaling.
		// Get the effective DPI scale that UMG applies to viewport widgets.
		// UGameViewportClient::GetDPIScale() may return 1.0 even when UMG
		// is applying a scale internally via the DPI curve.
		float ViewportDPIScale = 1.0f;
		if (GEngine && GEngine->GameViewport)
		{
			// Try multiple APIs to find the actual scale
			float Method1 = GEngine->GameViewport->GetDPIScale();

			// The viewport overlay uses SDPIScaler whose scale comes from
			// UUserInterfaceSettings::GetDPIScaleBasedOnSize
			FIntPoint ViewSize((int32)ViewportSize.X, (int32)ViewportSize.Y);
			float Method2 = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(ViewSize);

			ViewportDPIScale = Method2; // This is what actually gets applied
			if (ViewportDPIScale <= 0.f) ViewportDPIScale = 1.0f;

			UE_LOG(LogWowFrame, Warning, TEXT("DPI detection: GetDPIScale=%.3f, UISettings.GetDPIScaleBasedOnSize=%.3f, using=%.3f"),
				Method1, Method2, ViewportDPIScale);
		}

		// WoW scales by screen HEIGHT only — UIParent stretches to fill the full
		// viewport width.  This means on a 1920×1080 screen the UI scale is
		// 1080/768 = 1.40625 and UIParent is 1365×768 in WoW-coordinate units.
		// Divide by DPI scale so that WoW-coord → pixel conversion accounts for
		// the additional DPI transform UMG applies.
		RawViewportScale = ViewportSize.Y / WowBaseHeight;
		UIScale = RawViewportScale / ViewportDPIScale;

		// UIParent dimensions in WoW coordinate units (covers the full viewport)
		// UIScale already accounts for DPI, so ViewportSize / UIScale gives correct WoW coords.
		float UIParentW = ViewportSize.X / (ViewportSize.Y / WowBaseHeight); // Use raw viewport ratio
		float UIParentH = WowBaseHeight;

		UE_LOG(LogWowFrame, Log, TEXT("UI Scale: Viewport (%.0fx%.0f) -> UIParent %.1fx%.1f, scale %.3f"),
			ViewportSize.X, ViewportSize.Y, UIParentW, UIParentH, UIScale);

		// Initialize UIParent as a virtual frame that represents the screen
		// UIParent doesn't get a widget - it's just a coordinate reference point
		FWowFrameDef UIParentDef;
		UIParentDef.Name = TEXT("UIParent");
		UIParentDef.Type = EWowFrameType::Frame;
		UIParentDef.Width = UIParentW;   // Full viewport width in WoW units
		UIParentDef.Height = UIParentH;  // Always 768 WoW units tall
		UIParentDef.bVirtual = true; // Don't create a widget for this
		UIParentDef.Strata = EWowFrameStrata::MEDIUM;
		UIParentDef.FrameLevel = 0;

		// Register UIParent as a virtual frame and store its rect
		RegisterTemplate(TEXT("UIParent"), UIParentDef);

		// Store the rect so anchor lookups use the correct full-screen dimensions
		FrameRects.Add(TEXT("UIParent"), FFrameRect(0.0f, 0.0f, UIParentW, UIParentH));

		// Create a handle for UIParent but without a widget
		int64 UIParentHandle = NextHandle++;
		FFrameEntry UIParentEntry;
		UIParentEntry.Def = UIParentDef;
		UIParentEntry.Widget = nullptr; // No widget for virtual frames
		UIParentEntry.ParentHandle = -1;

		Frames.Add(UIParentHandle, MoveTemp(UIParentEntry));
		NameToHandle.Add(TEXT("UIParent"), UIParentHandle);

		// Check DPI / application scale — on Retina displays, Slate operates
		// in physical pixels while GetViewportSize returns logical pixels.
		float DPIScale = 1.0f;
		if (GEngine && GEngine->GameViewport && GEngine->GameViewport->GetWindow())
		{
			DPIScale = GEngine->GameViewport->GetWindow()->GetNativeWindow()->GetDPIScaleFactor();
		}

		UE_LOG(LogWowFrame, Warning, TEXT("Initialized UIParent virtual frame (%.1fx%.1f WoW coordinates, viewport %.0fx%.0f, UIScale %.3f, DPIScale=%.3f, viewportDPI=%.3f)"),
			UIParentW, UIParentH, ViewportSize.X, ViewportSize.Y, UIScale, DPIScale, ViewportDPIScale);
	}

	// Register built-in virtual templates that WoW defines in Lua but not in XML.
	// These templates are referenced by inherits="..." but their Frame definitions
	// aren't in the MPQ's XML files (they're defined implicitly by FrameXML Lua code).
	{
		// FadingFrame template — used by ZoneTextFrame, SubZoneTextFrame etc.
		// Frames inheriting this should start hidden; OnUpdate runs FadingFrame_OnUpdate.
		FWowFrameDef FadingFrameDef;
		FadingFrameDef.Name = TEXT("FadingFrame");
		FadingFrameDef.Type = EWowFrameType::Frame;
		FadingFrameDef.bVirtual = true;
		FadingFrameDef.bHidden = true;
		FadingFrameDef.Width = 128.0f;
		FadingFrameDef.Height = 128.0f;
		FWowScriptHandler FadingOnLoad;
		FadingOnLoad.Event = TEXT("OnLoad");
		FadingOnLoad.Code = TEXT("FadingFrame_OnLoad(self);");
		FadingFrameDef.Scripts.Add(FadingOnLoad);
		FWowScriptHandler FadingOnUpdate;
		FadingOnUpdate.Event = TEXT("OnUpdate");
		FadingOnUpdate.Code = TEXT("FadingFrame_OnUpdate(self, elapsed);");
		FadingFrameDef.Scripts.Add(FadingOnUpdate);
		RegisterTemplate(TEXT("FadingFrame"), FadingFrameDef);
	}

	UE_LOG(LogWowFrame, Warning, TEXT("Frame manager initialized - WoW anchor positioning system ready"));
}

void FWowFrameManager::RegisterTemplate(const FString& Name, const FWowFrameDef& Def)
{
	Templates.Add(Name, Def);
	UE_LOG(LogWowFrame, Verbose, TEXT("Registered template: %s"), *Name);
}

int64 FWowFrameManager::FindFrame(const FString& Name) const
{
	const int64* Handle = NameToHandle.Find(Name);
	return Handle ? *Handle : -1;
}

FString FWowFrameManager::GetFrameName(int64 Handle) const
{
	const FFrameEntry* Entry = Frames.Find(Handle);
	return Entry ? Entry->Def.Name : FString();
}

const FWowFrameDef* FWowFrameManager::GetFrameDef(int64 Handle) const
{
	const FFrameEntry* Entry = Frames.Find(Handle);
	return Entry ? &Entry->Def : nullptr;
}

void FWowFrameManager::SetFrameVisible(int64 Handle, bool bVisible)
{
	FFrameEntry* Entry = Frames.Find(Handle);
	if (!Entry) return;

	Entry->Def.bHidden = !bVisible;

	if (Entry->Widget.IsValid())
	{
		if (bVisible)
		{
			Entry->Widget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else
		{
			Entry->Widget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Propagate visibility recursively to ALL descendant frames
	TArray<int64> ToPropagate;
	ToPropagate.Add(Handle);
	while (ToPropagate.Num() > 0)
	{
		int64 Current = ToPropagate.Pop();
		for (auto& Pair : Frames)
		{
			if (Pair.Value.ParentHandle == Current && Pair.Value.Widget.IsValid())
			{
				if (!bVisible)
				{
					Pair.Value.Widget->SetVisibility(ESlateVisibility::Collapsed);
				}
				else if (!Pair.Value.Def.bHidden)
				{
					Pair.Value.Widget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
				}
				ToPropagate.Add(Pair.Key);
			}
		}
	}
}

bool FWowFrameManager::IsFrameVisible(int64 Handle) const
{
	const FFrameEntry* Entry = Frames.Find(Handle);
	return Entry ? !Entry->Def.bHidden : false;
}

FWowFrameDef* FWowFrameManager::GetMutableFrameDef(int64 Handle)
{
	FFrameEntry* Entry = Frames.Find(Handle);
	return Entry ? &Entry->Def : nullptr;
}

void FWowFrameManager::SetFrameSize(int64 Handle, float W, float H)
{
	FFrameEntry* Entry = Frames.Find(Handle);
	if (!Entry) return;

	Entry->Def.Width = W;
	Entry->Def.Height = H;

	// Clear cached frame rect since size changed
	if (!Entry->Def.Name.IsEmpty())
	{
		FrameRects.Remove(Entry->Def.Name);
	}

	if (Entry->Widget.IsValid())
	{
		// Re-apply anchors to recalculate position with new size
		ApplyAnchors(Entry->Widget.Get(), Entry->Def);
	}
}

void FWowFrameManager::SetFrameAnchors(int64 Handle, const TArray<FWowAnchor>& NewAnchors)
{
	FFrameEntry* Entry = Frames.Find(Handle);
	if (!Entry) return;

	Entry->Def.Anchors = NewAnchors;
	Entry->Def.bSetAllPoints = false;

	// Clear cached frame rect since position will change
	if (!Entry->Def.Name.IsEmpty())
	{
		FrameRects.Remove(Entry->Def.Name);
	}

	if (Entry->Widget.IsValid())
	{
		ApplyAnchors(Entry->Widget.Get(), Entry->Def);
	}
}

void FWowFrameManager::ClearFrameAnchors(int64 Handle)
{
	FFrameEntry* Entry = Frames.Find(Handle);
	if (!Entry) return;

	Entry->Def.Anchors.Empty();
	Entry->Def.bSetAllPoints = false;
}

void FWowFrameManager::SetFrameAlpha(int64 Handle, float Alpha)
{
	FFrameEntry* Entry = Frames.Find(Handle);
	if (!Entry) return;

	if (Entry->Widget.IsValid())
	{
		Entry->Widget->SetRenderOpacity(Alpha);
	}
}

void FWowFrameManager::SetFrameStrata(int64 Handle, EWowFrameStrata Strata)
{
	FFrameEntry* Entry = Frames.Find(Handle);
	if (!Entry) return;

	Entry->Def.Strata = Strata;

	if (Entry->Widget.IsValid())
	{
		if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Entry->Widget->Slot))
		{
			Slot->SetZOrder(static_cast<int32>(Strata) * 1000 + Entry->Def.FrameLevel);
		}
	}
}

void FWowFrameManager::SetFrameLevel(int64 Handle, int32 Level)
{
	FFrameEntry* Entry = Frames.Find(Handle);
	if (!Entry) return;

	Entry->Def.FrameLevel = Level;

	if (Entry->Widget.IsValid())
	{
		if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Entry->Widget->Slot))
		{
			Slot->SetZOrder(static_cast<int32>(Entry->Def.Strata) * 1000 + Level);
		}
	}
}

int64 FWowFrameManager::GetParentHandle(int64 Handle) const
{
	const FFrameEntry* Entry = Frames.Find(Handle);
	return Entry ? Entry->ParentHandle : -1;
}

TArray<int64> FWowFrameManager::GetChildHandles(int64 Handle) const
{
	TArray<int64> Children;
	for (const auto& Pair : Frames)
	{
		if (Pair.Value.ParentHandle == Handle)
		{
			Children.Add(Pair.Key);
		}
	}
	return Children;
}

UWidget* FWowFrameManager::GetWidgetForHandle(int64 Handle) const
{
	const FFrameEntry* Entry = Frames.Find(Handle);
	return Entry && Entry->Widget.IsValid() ? Entry->Widget.Get() : nullptr;
}

UButton* FWowFrameManager::GetButtonWidget(int64 Handle) const
{
	const TWeakObjectPtr<UButton>* Found = ButtonWidgets.Find(Handle);
	return (Found && Found->IsValid()) ? Found->Get() : nullptr;
}

UEditableTextBox* FWowFrameManager::GetEditBoxWidget(int64 Handle) const
{
	const TWeakObjectPtr<UEditableTextBox>* Found = EditBoxWidgets.Find(Handle);
	return (Found && Found->IsValid()) ? Found->Get() : nullptr;
}

USlider* FWowFrameManager::GetSliderWidget(int64 Handle) const
{
	const TWeakObjectPtr<USlider>* Found = SliderWidgets.Find(Handle);
	return (Found && Found->IsValid()) ? Found->Get() : nullptr;
}

UTextBlock* FWowFrameManager::GetPrimaryTextWidget(int64 Handle) const
{
	const TWeakObjectPtr<UTextBlock>* Found = PrimaryTextWidgets.Find(Handle);
	return (Found && Found->IsValid()) ? Found->Get() : nullptr;
}

UTextBlock* FWowFrameManager::GetNamedFontString(const FString& Name) const
{
	const TWeakObjectPtr<UTextBlock>* Found = NamedFontStringWidgets.Find(Name);
	return (Found && Found->IsValid()) ? Found->Get() : nullptr;
}

void FWowFrameManager::SetFrameMouseEnabled(int64 Handle, bool bEnabled)
{
	if (FFrameEntry* Entry = Frames.Find(Handle))
	{
		Entry->bMouseEnabled = bEnabled;
	}
}

void FWowFrameManager::SetFrameKeyboardEnabled(int64 Handle, bool bEnabled)
{
	if (FFrameEntry* Entry = Frames.Find(Handle))
	{
		Entry->bKeyboardEnabled = bEnabled;
	}
}

void FWowFrameManager::SetFrameMouseWheelEnabled(int64 Handle, bool bEnabled)
{
	if (FFrameEntry* Entry = Frames.Find(Handle))
	{
		Entry->bMouseWheelEnabled = bEnabled;
	}
}

bool FWowFrameManager::IsFrameMouseEnabled(int64 Handle) const
{
	const FFrameEntry* Entry = Frames.Find(Handle);
	return Entry ? Entry->bMouseEnabled : false;
}

bool FWowFrameManager::IsFrameKeyboardEnabled(int64 Handle) const
{
	const FFrameEntry* Entry = Frames.Find(Handle);
	return Entry ? Entry->bKeyboardEnabled : false;
}

bool FWowFrameManager::IsFrameMouseWheelEnabled(int64 Handle) const
{
	const FFrameEntry* Entry = Frames.Find(Handle);
	return Entry ? Entry->bMouseWheelEnabled : false;
}

// ── Template Inheritance ──────────────────────────────────────────────────────

void FWowFrameManager::MergeTemplate(FWowFrameDef& Target, const FWowFrameDef& Template)
{
	// Type — template sets the base type if target doesn't override
	// (typically the inheriting frame specifies its own type, so only fallback)

	// Size — use template size if target doesn't specify
	if (Target.Width == 0.f && Template.Width > 0.f) Target.Width = Template.Width;
	if (Target.Height == 0.f && Template.Height > 0.f) Target.Height = Template.Height;

	// Strata/level — use template if target is at default
	if (Target.Strata == EWowFrameStrata::MEDIUM && Template.Strata != EWowFrameStrata::MEDIUM)
		Target.Strata = Template.Strata;
	if (Target.FrameLevel == 0 && Template.FrameLevel != 0)
		Target.FrameLevel = Template.FrameLevel;

	// Hidden — template hidden overrides unless target explicitly sets visible
	if (Template.bHidden && !Target.bHidden) Target.bHidden = Template.bHidden;
	if (Target.ParentKey.IsEmpty()) Target.ParentKey = Template.ParentKey;

	// EnableMouse / EnableKeyboard — inherit from template
	if (Template.bEnableMouse && !Target.bEnableMouse) Target.bEnableMouse = true;
	if (Template.bEnableKeyboard && !Target.bEnableKeyboard) Target.bEnableKeyboard = true;

	// SetAllPoints
	if (Template.bSetAllPoints && !Target.bSetAllPoints) Target.bSetAllPoints = Template.bSetAllPoints;

	// Anchors — template anchors are used if target has none
	if (Target.Anchors.IsEmpty() && !Template.Anchors.IsEmpty())
	{
		Target.Anchors = Template.Anchors;
	}

	// Layers — merge: template layers first, then target layers (target overrides by draw layer level)
	for (const FWowLayer& TplLayer : Template.Layers)
	{
		bool bFound = false;
		for (FWowLayer& TargetLayer : Target.Layers)
		{
			if (TargetLayer.Level == TplLayer.Level)
			{
				// Target already has this layer — prepend template textures/fontstrings
				TArray<FWowTextureElement> Merged = TplLayer.Textures;
				Merged.Append(TargetLayer.Textures);
				TargetLayer.Textures = MoveTemp(Merged);

				TArray<FWowFontStringElement> MergedFS = TplLayer.FontStrings;
				MergedFS.Append(TargetLayer.FontStrings);
				TargetLayer.FontStrings = MoveTemp(MergedFS);
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			Target.Layers.Add(TplLayer);
		}
	}

	// Scripts — concatenate: template scripts run first
	TArray<FWowScriptHandler> MergedScripts = Template.Scripts;
	MergedScripts.Append(Target.Scripts);
	Target.Scripts = MoveTemp(MergedScripts);

	for (const FString& NamedObject : Template.NamedObjectGlobals)
	{
		Target.NamedObjectGlobals.AddUnique(NamedObject);
	}

	// Children — template children are prepended
	TArray<FWowFrameDef> MergedChildren = Template.Children;
	MergedChildren.Append(Target.Children);
	Target.Children = MoveTemp(MergedChildren);

	// Backdrop — use template if target doesn't have one
	if (!Target.Backdrop.IsSet() && Template.Backdrop.IsSet())
	{
		Target.Backdrop = Template.Backdrop;
	}

	// Type-specific properties — use template values if target is empty
	if (Target.ButtonText.IsEmpty()) Target.ButtonText = Template.ButtonText;
	if (Target.NormalTexture.IsEmpty()) { Target.NormalTexture = Template.NormalTexture; Target.NormalTextureName = Template.NormalTextureName; Target.NormalTextureParentKey = Template.NormalTextureParentKey; }
	if (Target.PushedTexture.IsEmpty()) { Target.PushedTexture = Template.PushedTexture; Target.PushedTextureName = Template.PushedTextureName; Target.PushedTextureParentKey = Template.PushedTextureParentKey; }
	if (Target.HighlightTexture.IsEmpty()) { Target.HighlightTexture = Template.HighlightTexture; Target.HighlightTextureName = Template.HighlightTextureName; Target.HighlightTextureParentKey = Template.HighlightTextureParentKey; }
	if (Target.DisabledTexture.IsEmpty()) { Target.DisabledTexture = Template.DisabledTexture; Target.DisabledTextureName = Template.DisabledTextureName; Target.DisabledTextureParentKey = Template.DisabledTextureParentKey; }
	if (Target.Orientation.IsEmpty()) Target.Orientation = Template.Orientation;
}

FWowFrameDef FWowFrameManager::ResolveInherits(const FWowFrameDef& Def) const
{
	if (Def.Inherits.IsEmpty()) return Def;

	FWowFrameDef Resolved = Def;

	// Parse comma-separated template names
	TArray<FString> TemplateNames;
	Def.Inherits.ParseIntoArray(TemplateNames, TEXT(","), true);

	// Apply templates left-to-right (later templates override earlier ones)
	// Build a merged template first, then merge into the frame def
	for (FString& TplName : TemplateNames)
	{
		TplName.TrimStartAndEndInline();
		const FWowFrameDef* Tpl = Templates.Find(TplName);
		if (Tpl)
		{
			// Recursively resolve the template's own inheritance
			FWowFrameDef ResolvedTpl = ResolveInherits(*Tpl);
			MergeTemplate(Resolved, ResolvedTpl);
			UE_LOG(LogWowFrame, Verbose, TEXT("Applied template '%s' to frame '%s'"), *TplName, *Def.Name);
		}
		else
		{
			UE_LOG(LogWowFrame, Warning, TEXT("Template '%s' not found for frame '%s'"), *TplName, *Def.Name);
		}
	}

	// Clear the inherits field so we don't re-resolve
	Resolved.Inherits.Empty();

	return Resolved;
}

// ── Anchor Positioning ────────────────────────────────────────────────────────

/** Get the screen position offset of an anchor point on a frame */
FVector2D FWowFrameManager::GetAnchorPointOffset(EWowAnchorPoint Point, float Width, float Height)
{
	switch (Point)
	{
	case EWowAnchorPoint::TOPLEFT:     return FVector2D(0.0f, 0.0f);
	case EWowAnchorPoint::TOP:         return FVector2D(Width * 0.5f, 0.0f);
	case EWowAnchorPoint::TOPRIGHT:    return FVector2D(Width, 0.0f);
	case EWowAnchorPoint::LEFT:        return FVector2D(0.0f, Height * 0.5f);
	case EWowAnchorPoint::CENTER:      return FVector2D(Width * 0.5f, Height * 0.5f);
	case EWowAnchorPoint::RIGHT:       return FVector2D(Width, Height * 0.5f);
	case EWowAnchorPoint::BOTTOMLEFT:  return FVector2D(0.0f, Height);
	case EWowAnchorPoint::BOTTOM:      return FVector2D(Width * 0.5f, Height);
	case EWowAnchorPoint::BOTTOMRIGHT: return FVector2D(Width, Height);
	default: return FVector2D(Width * 0.5f, Height * 0.5f);
	}
}

/** Get the frame rect for a named frame, or UIParent default */
FWowFrameManager::FFrameRect FWowFrameManager::GetFrameRect(const FString& FrameName)
{
	if (FrameName.IsEmpty() || FrameName == TEXT("UIParent"))
	{
		// UIParent covers the entire screen in WoW coordinates
		if (FrameRects.Contains(TEXT("UIParent")))
		{
			return FrameRects[TEXT("UIParent")];
		}

		// Initialize UIParent rect if not set — use actual viewport-based dimensions
		float FallbackW = 1024.0f;
		float FallbackH = 768.0f;
		if (UIScale > 0.f && GEngine && GEngine->GameViewport)
		{
			FVector2D VP;
			GEngine->GameViewport->GetViewportSize(VP);
			FallbackW = VP.X / UIScale;
			FallbackH = 768.0f;
		}
		FFrameRect UIParentRect(0.0f, 0.0f, FallbackW, FallbackH);
		FrameRects.Add(TEXT("UIParent"), UIParentRect);
		return UIParentRect;
	}

	if (FrameRects.Contains(FrameName))
	{
		return FrameRects[FrameName];
	}

	// Try to look up the frame and calculate its rect
	int64 Handle = FindFrame(FrameName);
	if (Handle != -1)
	{
		const FWowFrameDef* Def = GetFrameDef(Handle);
		if (Def)
		{
			// Calculate rect based on frame's definition
			FFrameRect Rect(0.0f, 0.0f, Def->Width, Def->Height);

			if (!Def->Anchors.IsEmpty())
			{
				const FWowAnchor& Anchor = Def->Anchors[0];
				FFrameRect ParentRect = GetFrameRect(Anchor.RelativeTo.IsEmpty() ? Def->Parent : Anchor.RelativeTo);
				FVector2D ParentAnchorPos = FVector2D(ParentRect.X, ParentRect.Y) +
					GetAnchorPointOffset(Anchor.RelativePoint, ParentRect.W, ParentRect.H);
				FVector2D FrameAnchorOffset = GetAnchorPointOffset(Anchor.Point, Rect.W, Rect.H);

				Rect.X = ParentAnchorPos.X + Anchor.OffsetX - FrameAnchorOffset.X;
				// WoW convention: positive Y = UP, but our rect system Y increases downward
				Rect.Y = ParentAnchorPos.Y - Anchor.OffsetY - FrameAnchorOffset.Y;
			}

			// Cache the calculated rect
			FrameRects.Add(FrameName, Rect);
			return Rect;
		}
	}

	// Fallback to center of screen
	UE_LOG(LogWowFrame, Warning, TEXT("Frame '%s' not found, using default rect"), *FrameName);
	FFrameRect DefaultRect(400.0f, 300.0f, 200.0f, 100.0f);
	FrameRects.Add(FrameName, DefaultRect);
	return DefaultRect;
}

void FWowFrameManager::ApplyAnchors(UWidget* Widget, const FWowFrameDef& Def)
{
	if (!Widget) return;

	UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Widget->Slot);
	if (!Slot)
	{
		UE_LOG(LogWowFrame, Error, TEXT("  ApplyAnchors: No canvas slot for %s"), *Def.Name);
		return;
	}

	if (Def.bSetAllPoints)
	{
		// Fill parent completely
		Slot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		Slot->SetOffsets(FMargin(0));

		// Store frame rect for children to reference
		FFrameRect ParentRect = GetFrameRect(Def.Parent);
		FrameRects.Add(Def.Name, ParentRect);

		UE_LOG(LogWowFrame, Log, TEXT("  %s: SetAllPoints (fill parent %.1fx%.1f)"), *Def.Name, ParentRect.W, ParentRect.H);
		return;
	}

	float FrameWidth = Def.Width;
	float FrameHeight = Def.Height;

	// Get the canvas owner's absolute position — we need to convert absolute WoW coords
	// to relative coords within the parent canvas. In WoW, all positions are absolute
	// screen coords, but UMG canvas slots use parent-relative positioning.
	FFrameRect CanvasOwnerRect = GetFrameRect(Def.Parent);
	float CanvasOffX = CanvasOwnerRect.X;
	float CanvasOffY = CanvasOwnerRect.Y;

	if (Def.Anchors.IsEmpty())
	{
		// Default positioning - center of parent with explicit size
		float PosX = (CanvasOwnerRect.W * 0.5f) - (FrameWidth * 0.5f);
		float PosY = (CanvasOwnerRect.H * 0.5f) - (FrameHeight * 0.5f);

		// Store ABSOLUTE rect for child lookups
		float AbsX = CanvasOffX + PosX;
		float AbsY = CanvasOffY + PosY;

		Slot->SetAnchors(FAnchors(0.0f, 0.0f));
		Slot->SetAlignment(FVector2D(0.0f, 0.0f));

		if (FrameWidth > 0.f && FrameHeight > 0.f)
		{
			Slot->SetPosition(FVector2D(PosX * UIScale, PosY * UIScale));
			Slot->SetSize(FVector2D(FrameWidth * UIScale, FrameHeight * UIScale));
			FrameRects.Add(Def.Name, FFrameRect(AbsX, AbsY, FrameWidth, FrameHeight));
		}
		else
		{
			Slot->SetAutoSize(true);
		}
		return;
	}

	// Handle single anchor positioning
	const FWowAnchor& Anchor = Def.Anchors[0];

	// Get anchor reference frame rect (absolute coords)
	FString AnchorParentName = Anchor.RelativeTo.IsEmpty() ? Def.Parent : Anchor.RelativeTo;
	FFrameRect AnchorParentRect = GetFrameRect(AnchorParentName);

	// Calculate anchor position in absolute WoW coordinates
	FVector2D ParentAnchorPos = FVector2D(AnchorParentRect.X, AnchorParentRect.Y) +
		GetAnchorPointOffset(Anchor.RelativePoint, AnchorParentRect.W, AnchorParentRect.H);

	FVector2D FrameAnchorOffset = GetAnchorPointOffset(Anchor.Point, FrameWidth, FrameHeight);

	// WoW offset Y is inverted: positive Y = UP in WoW, but DOWN in canvas
	float AbsX = ParentAnchorPos.X + Anchor.OffsetX - FrameAnchorOffset.X;
	float AbsY = ParentAnchorPos.Y - Anchor.OffsetY - FrameAnchorOffset.Y;

	// Convert absolute position to relative (within parent canvas)
	float RelX = AbsX - CanvasOffX;
	float RelY = AbsY - CanvasOffY;

	// Debug log for key frames
	if (Def.Name.Contains(TEXT("Party")) || Def.Name.Contains(TEXT("Target")) || Def.Name.Contains(TEXT("Pet"))
		|| Def.Name.Contains(TEXT("Player")) || Def.Name.Contains(TEXT("MainMenu")) || Def.Name.Contains(TEXT("Minimap")))
	{
		UE_LOG(LogWowFrame, Warning, TEXT("ANCHOR %s: point=%d relPoint=%d relTo='%s' offset=(%.1f,%.1f) -> abs=(%.1f,%.1f) rel=(%.1f,%.1f) frameSize=(%.1f,%.1f) parentRect=(%.1f,%.1f,%.1f,%.1f)"),
			*Def.Name, (int)Anchor.Point, (int)Anchor.RelativePoint, *AnchorParentName,
			Anchor.OffsetX, Anchor.OffsetY, AbsX, AbsY, RelX, RelY,
			FrameWidth, FrameHeight, AnchorParentRect.X, AnchorParentRect.Y, AnchorParentRect.W, AnchorParentRect.H);
	}

	// Handle two-anchor stretching
	if (Def.Anchors.Num() >= 2)
	{
		const FWowAnchor& Anchor2 = Def.Anchors[1];
		FFrameRect Parent2Rect = GetFrameRect(Anchor2.RelativeTo.IsEmpty() ? Def.Parent : Anchor2.RelativeTo);

		FVector2D Parent2AnchorPos = FVector2D(Parent2Rect.X, Parent2Rect.Y) +
			GetAnchorPointOffset(Anchor2.RelativePoint, Parent2Rect.W, Parent2Rect.H);
		FVector2D Frame2AnchorOffset = GetAnchorPointOffset(Anchor2.Point, FrameWidth, FrameHeight);

		float Abs2X = Parent2AnchorPos.X + Anchor2.OffsetX - Frame2AnchorOffset.X;
		float Abs2Y = Parent2AnchorPos.Y - Anchor2.OffsetY - Frame2AnchorOffset.Y;

		float StretchWidth = Abs2X - AbsX + Frame2AnchorOffset.X - FrameAnchorOffset.X;
		float StretchHeight = Abs2Y - AbsY + Frame2AnchorOffset.Y - FrameAnchorOffset.Y;

		Slot->SetAnchors(FAnchors(0.0f, 0.0f));
		Slot->SetAlignment(FVector2D(0.0f, 0.0f));
		Slot->SetPosition(FVector2D(RelX * UIScale, RelY * UIScale));
		Slot->SetSize(FVector2D(StretchWidth * UIScale, StretchHeight * UIScale));

		// Store ABSOLUTE rect for other frames to anchor to
		FrameRects.Add(Def.Name, FFrameRect(AbsX, AbsY, StretchWidth, StretchHeight));
	}
	else
	{
		// Single anchor positioning
		Slot->SetAnchors(FAnchors(0.0f, 0.0f));
		Slot->SetAlignment(FVector2D(0.0f, 0.0f));
		Slot->SetPosition(FVector2D(RelX * UIScale, RelY * UIScale));

		if (FrameWidth > 0.f && FrameHeight > 0.f)
		{
			Slot->SetSize(FVector2D(FrameWidth * UIScale, FrameHeight * UIScale));
		}
		else
		{
			Slot->SetAutoSize(true);
		}

		// Store ABSOLUTE rect for other frames to anchor to
		FrameRects.Add(Def.Name, FFrameRect(AbsX, AbsY, FrameWidth, FrameHeight));
	}
}

// ── Layer Content (Textures & FontStrings) ───────────────────────────────────

void FWowFrameManager::ApplyElementAnchors(UWidget* Widget, UCanvasPanel* Parent,
	const TArray<FWowAnchor>& Anchors, float Width, float Height, bool bSetAllPoints,
	float ParentFrameW, float ParentFrameH)
{
	UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Widget->Slot);
	if (!Slot) return;

	// Use passed-in parent dimensions (WoW coords) — GetDesiredSize() returns 0 at creation time
	float ParentW = ParentFrameW;
	float ParentH = ParentFrameH;

	if (bSetAllPoints)
	{
		// SetAllPoints=true: explicitly fill parent
		Slot->SetAnchors(FAnchors(0.0f, 0.0f));
		Slot->SetAlignment(FVector2D(0.0f, 0.0f));
		Slot->SetPosition(FVector2D(0.0f, 0.0f));
		if (ParentW > 0.f && ParentH > 0.f)
		{
			Slot->SetSize(FVector2D(ParentW * UIScale, ParentH * UIScale));
		}
		else
		{
			Slot->SetAutoSize(true);
		}
		return;
	}

	if (Anchors.IsEmpty())
	{
		// No anchors and no SetAllPoints: position at (0,0), use explicit size
		// or fall back to the widget's native image dimensions.
		// Do NOT fill parent — that causes small decorative textures to cover the screen.
		Slot->SetAnchors(FAnchors(0.0f, 0.0f));
		Slot->SetAlignment(FVector2D(0.0f, 0.0f));
		Slot->SetPosition(FVector2D(0.0f, 0.0f));

		if (Width > 0.f && Height > 0.f)
		{
			Slot->SetSize(FVector2D(Width * UIScale, Height * UIScale));
		}
		else
		{
			// Use native image size if available
			UImage* ImgWidget = Cast<UImage>(Widget);
			if (ImgWidget)
			{
				FSlateBrush Brush = ImgWidget->GetBrush();
				if (Brush.ImageSize.X > 0.f && Brush.ImageSize.Y > 0.f)
				{
					Slot->SetSize(FVector2D(Brush.ImageSize.X * UIScale, Brush.ImageSize.Y * UIScale));
				}
				else
				{
					Slot->SetAutoSize(true);
				}
			}
			else
			{
				Slot->SetAutoSize(true);
			}
		}
		return;
	}

	// For layer elements, positioning is relative to the parent frame (which is a canvas)
	const FWowAnchor& Anchor = Anchors[0];

	// Calculate parent anchor position (relative to parent's top-left)
	FVector2D ParentAnchorPos = GetAnchorPointOffset(Anchor.RelativePoint, ParentW, ParentH);

	// Calculate element anchor offset
	FVector2D ElementAnchorOffset = GetAnchorPointOffset(Anchor.Point, Width, Height);

	// Calculate final element position: parent_anchor + wow_offset - element_anchor_offset
	// WoW convention: positive Y = UP, but UMG canvas Y increases downward, so negate OffsetY
	float FinalX = ParentAnchorPos.X + Anchor.OffsetX - ElementAnchorOffset.X;
	float FinalY = ParentAnchorPos.Y - Anchor.OffsetY - ElementAnchorOffset.Y;

	// Apply positioning
	Slot->SetAnchors(FAnchors(0.0f, 0.0f));
	Slot->SetAlignment(FVector2D(0.0f, 0.0f));
	Slot->SetPosition(FVector2D(FinalX * UIScale, FinalY * UIScale));

	if (Width > 0.f && Height > 0.f)
	{
		FVector2D NewSize(Width * UIScale, Height * UIScale);
		Slot->SetSize(NewSize);
		UE_LOG(LogWowFrame, Verbose, TEXT("    ApplyAnchors: pos=(%.0f,%.0f) size=(%.0f,%.0f) [from Width=%.0f Height=%.0f scale=%.3f]"),
			FinalX * UIScale, FinalY * UIScale, NewSize.X, NewSize.Y, Width, Height, UIScale);
	}
	else
	{
		// No explicit size — use the widget's brush texture dimensions if available,
		// otherwise auto-size. Auto-size doesn't always work at creation time in UMG.
		UImage* ImgCheck = Cast<UImage>(Widget);
		if (ImgCheck)
		{
			FSlateBrush Brush = ImgCheck->GetBrush();
			if (Brush.ImageSize.X > 0.f && Brush.ImageSize.Y > 0.f)
			{
				Slot->SetSize(FVector2D(Brush.ImageSize.X * UIScale, Brush.ImageSize.Y * UIScale));
			}
			else
			{
				Slot->SetAutoSize(true);
			}
		}
		else
		{
			Slot->SetAutoSize(true);
		}
	}

	// Handle two-anchor stretching for layer elements
	if (Anchors.Num() >= 2)
	{
		const FWowAnchor& Anchor2 = Anchors[1];

		// Calculate second anchor positions
		FVector2D Parent2AnchorPos = GetAnchorPointOffset(Anchor2.RelativePoint, ParentW, ParentH);
		FVector2D Element2AnchorOffset = GetAnchorPointOffset(Anchor2.Point, Width, Height);

		float Final2X = Parent2AnchorPos.X + Anchor2.OffsetX - Element2AnchorOffset.X;
		float Final2Y = Parent2AnchorPos.Y - Anchor2.OffsetY - Element2AnchorOffset.Y;

		// Element stretches between the two positions
		float StretchWidth = Final2X - FinalX + Element2AnchorOffset.X - ElementAnchorOffset.X;
		float StretchHeight = Final2Y - FinalY + Element2AnchorOffset.Y - ElementAnchorOffset.Y;

		Slot->SetPosition(FVector2D(FinalX * UIScale, FinalY * UIScale));
		Slot->SetSize(FVector2D(StretchWidth * UIScale, StretchHeight * UIScale));
	}
}

void FWowFrameManager::CreateLayerContent(UCanvasPanel* Container, const FWowFrameDef& Def, int64 OwnerHandle)
{
	if (!Container) return;

	// Resolve effective parent dimensions for element anchoring.
	// Frames with SetAllPoints=true or no explicit size have Width/Height=0 in the def,
	// but their actual size is computed during ApplyAnchors and cached in FrameRects.
	float EffectiveParentW = Def.Width;
	float EffectiveParentH = Def.Height;
	if (EffectiveParentW <= 0.f || EffectiveParentH <= 0.f)
	{
		if (FrameRects.Contains(Def.Name))
		{
			const FFrameRect& Rect = FrameRects[Def.Name];
			if (Rect.W > 0.f) EffectiveParentW = Rect.W;
			if (Rect.H > 0.f) EffectiveParentH = Rect.H;
		}
		if (EffectiveParentW <= 0.f || EffectiveParentH <= 0.f)
		{
			UE_LOG(LogWowFrame, Warning, TEXT("CreateLayerContent: %s has zero effective size (def=%.0fx%.0f rect=%.0fx%.0f) — element anchors will collapse"),
				*Def.Name, Def.Width, Def.Height, EffectiveParentW, EffectiveParentH);
		}
	}

	int32 TotalTextures = 0, TotalFontStrings = 0, NamedTextures = 0;
	for (const FWowLayer& L : Def.Layers) { TotalTextures += L.Textures.Num(); TotalFontStrings += L.FontStrings.Num(); for (const auto& T : L.Textures) { if (!T.Name.IsEmpty()) NamedTextures++; } }
	if (TotalTextures > 0 || TotalFontStrings > 0)
	{
		UE_LOG(LogWowFrame, Log, TEXT("CreateLayerContent: %s — %d layers, %d textures (%d named), %d fontstrings, effectiveSize=%.0fx%.0f"),
			*Def.Name, Def.Layers.Num(), TotalTextures, NamedTextures, TotalFontStrings, EffectiveParentW, EffectiveParentH);
	}

	UObject* Outer = Container->GetOuter();
	if (!Outer) Outer = GetTransientPackage();

	// Layer draw order: BACKGROUND(0), BORDER(1), ARTWORK(2), OVERLAY(3), HIGHLIGHT(4)
	int32 LayerZBase = 0;

	for (const FWowLayer& Layer : Def.Layers)
	{
		int32 ZOrder = static_cast<int32>(Layer.Level) * 100;

		// Create UImage widgets for textures
		for (const FWowTextureElement& Tex : Layer.Textures)
		{
			UImage* ImgWidget = NewObject<UImage>(Outer);
			// Start with transparent brush — prevents default UE white/grey square
			// from showing before the BLP texture is loaded (or for empty textures).
			FSlateBrush TransparentBrush;
			TransparentBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
			ImgWidget->SetBrush(TransparentBrush);
			ImgWidget->SetColorAndOpacity(Tex.VertexColor);
			if (Tex.bHidden)
			{
				ImgWidget->SetVisibility(ESlateVisibility::Collapsed);
			}

			// Register named texture regions for Lua access + create Lua global
			if (!Tex.Name.IsEmpty())
			{
				NamedTextureWidgets.Add(Tex.Name, ImgWidget);

				// Create a Lua global table for this texture region so scripts can call
				// MainMenuBarLeftEndCap:SetTexture("Interface\\...")
				if (EventSystem)
				{
					EventSystem->CreateTextureRegionGlobal(Tex.Name, OwnerHandle, TEXT("Texture"));
					UE_LOG(LogWowFrame, Verbose, TEXT("    Registered texture region global: %s"), *Tex.Name);
				}
				else
				{
					UE_LOG(LogWowFrame, Warning, TEXT("    NO EventSystem for texture region: %s"), *Tex.Name);
				}
			}

			// If no file specified, hide until Lua sets a texture via SetTexture()
			if (Tex.File.IsEmpty())
			{
				ImgWidget->SetRenderOpacity(0.0f);
			}

			// Load the BLP texture if specified
			if (!Tex.File.IsEmpty())
			{
				UTexture2D* LoadedTexture = LoadUITexture(Tex.File);
				if (LoadedTexture)
				{
					// Create slate brush with texture and UV coordinates
					FSlateBrush SlateBrush;
					SlateBrush.SetResourceObject(LoadedTexture);
					SlateBrush.DrawAs = ESlateBrushDrawType::Image;
					float FullW = (float)LoadedTexture->GetSizeX();
					float FullH = (float)LoadedTexture->GetSizeY();
					SlateBrush.Tiling = ESlateBrushTileType::NoTile;

					// Apply texture coordinates if specified (default is 0,0,1,1 for full texture)
					bool bHasSubRegion = (Tex.Left != 0.0f || Tex.Right != 1.0f || Tex.Top != 0.0f || Tex.Bottom != 1.0f);
					if (bHasSubRegion)
					{
						// WoW uses Left,Right,Top,Bottom UV coordinates.
						// FBox2D expects Min < Max, but WoW uses Left > Right for horizontal flip
						// and Top > Bottom for vertical flip.  Normalize the UVs and apply
						// a RenderTransform to handle flipping.
						float UVLeft  = FMath::Min(Tex.Left, Tex.Right);
						float UVRight = FMath::Max(Tex.Left, Tex.Right);
						float UVTop   = FMath::Min(Tex.Top, Tex.Bottom);
						float UVBot   = FMath::Max(Tex.Top, Tex.Bottom);

						FBox2D UVRegion(FVector2D(UVLeft, UVTop), FVector2D(UVRight, UVBot));
						SlateBrush.SetUVRegion(UVRegion);

						// ImageSize = visible sub-region size (not full atlas)
						// so fallback sizing uses correct dimensions
						SlateBrush.ImageSize = FVector2D(
							FullW * (UVRight - UVLeft),
							FullH * (UVBot - UVTop));

						// Apply render transform for flipped texcoords
						bool bFlipH = Tex.Left > Tex.Right;
						bool bFlipV = Tex.Top > Tex.Bottom;
						if (bFlipH || bFlipV)
						{
							FVector2D Scale(bFlipH ? -1.0f : 1.0f, bFlipV ? -1.0f : 1.0f);
							ImgWidget->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
							ImgWidget->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, Scale, FVector2D::ZeroVector, 0.0f));
						}
					}
					else
					{
						SlateBrush.ImageSize = FVector2D(FullW, FullH);
					}

					ImgWidget->SetBrush(SlateBrush);
					if (Tex.Left > Tex.Right || Tex.Top > Tex.Bottom)
					{
						UE_LOG(LogWowFrame, Warning, TEXT("  FLIPPED texture: %s in frame %s (UV: L=%.3f R=%.3f T=%.3f B=%.3f) flipH=%d flipV=%d"),
							*Tex.File, *Def.Name, Tex.Left, Tex.Right, Tex.Top, Tex.Bottom,
							Tex.Left > Tex.Right ? 1 : 0, Tex.Top > Tex.Bottom ? 1 : 0);
					}
					UE_LOG(LogWowFrame, Verbose, TEXT("  Applied texture: %s (UV: %.3f,%.3f to %.3f,%.3f)"),
						*Tex.File, Tex.Left, Tex.Top, Tex.Right, Tex.Bottom);
				}
				else
				{
					UE_LOG(LogWowFrame, Warning, TEXT("  Failed to load texture: %s for frame %s"), *Tex.File, *Def.Name);
				}
				// Log texture with position info
				UCanvasPanelSlot* DbgSlot = Cast<UCanvasPanelSlot>(ImgWidget->Slot);
				FVector2D DbgPos = DbgSlot ? DbgSlot->GetPosition() : FVector2D::ZeroVector;
				FVector2D DbgSize = DbgSlot ? DbgSlot->GetSize() : FVector2D::ZeroVector;
				UE_LOG(LogWowFrame, Log, TEXT("  TEX: %s -> %s (%s) xmlSize=%.0fx%.0f slot=(%.0f,%.0f) slotSz=(%.0f,%.0f) anchors=%d setAll=%d"),
					*Tex.File, LoadedTexture ? TEXT("OK") : TEXT("FAIL"), *Def.Name, Tex.Width, Tex.Height,
					DbgPos.X, DbgPos.Y, DbgSize.X, DbgSize.Y, Tex.Anchors.Num(), Tex.bSetAllPoints ? 1 : 0);
			}

			UCanvasPanelSlot* Slot = Container->AddChildToCanvas(ImgWidget);
			if (Slot)
			{
				Slot->SetZOrder(ZOrder++);
			}

			ApplyElementAnchors(ImgWidget, Container, Tex.Anchors, Tex.Width, Tex.Height, Tex.bSetAllPoints, EffectiveParentW, EffectiveParentH);

			// For textures with anchors but no explicit size that auto-sized,
			// override with texture native size × UIScale (prevents oversized rendering)
			if (Tex.Width <= 0.f && Tex.Height <= 0.f && !Tex.bSetAllPoints && Tex.Anchors.Num() > 0)
			{
				UCanvasPanelSlot* TexSlot = Cast<UCanvasPanelSlot>(ImgWidget->Slot);
				if (TexSlot)
				{
					FSlateBrush CurrentBrush = ImgWidget->GetBrush();
					FVector2D ImgSize = CurrentBrush.ImageSize;
					if (ImgSize.X > 0.f && ImgSize.Y > 0.f)
					{
						TexSlot->SetAutoSize(false);
						TexSlot->SetSize(FVector2D(ImgSize.X * UIScale, ImgSize.Y * UIScale));
					}
				}
			}

			// Register named texture elements so other frames can anchor to them
			if (!Tex.Name.IsEmpty())
			{
				// Calculate and store absolute rect for this texture element.
				// WoW XML allows frames to anchor relativeTo a named texture/fontstring.
				UCanvasPanelSlot* TexSlot2 = Cast<UCanvasPanelSlot>(ImgWidget->Slot);
				if (TexSlot2)
				{
					// Get the parent frame's absolute position
					FFrameRect ParentAbsRect = FrameRects.Contains(Def.Name)
						? FrameRects[Def.Name]
						: FFrameRect(0.f, 0.f, EffectiveParentW, EffectiveParentH);

					// Element position within parent canvas (in WoW coords, pre-scale)
					FVector2D SlotPos = TexSlot2->GetPosition();
					FVector2D SlotSize = TexSlot2->GetSize();
					float ElemX = ParentAbsRect.X + SlotPos.X / UIScale;
					float ElemY = ParentAbsRect.Y + SlotPos.Y / UIScale;
					float ElemW = SlotSize.X / UIScale;
					float ElemH = SlotSize.Y / UIScale;

					FrameRects.Add(Tex.Name, FFrameRect(ElemX, ElemY, ElemW, ElemH));
					UE_LOG(LogWowFrame, Log, TEXT("  Registered texture rect: %s abs=(%.1f,%.1f) sz=(%.1f,%.1f)"),
						*Tex.Name, ElemX, ElemY, ElemW, ElemH);
				}
			}
		}

		// Create UTextBlock widgets for fontstrings
		for (const FWowFontStringElement& FS : Layer.FontStrings)
		{
			UTextBlock* TextWidget = NewObject<UTextBlock>(Outer);

			if (!FS.Text.IsEmpty())
			{
				TextWidget->SetText(FText::FromString(FS.Text));
			}

			// Apply color
			TextWidget->SetColorAndOpacity(FSlateColor(FS.Color));

			// Apply font from font manager or fallback to default
			FSlateFontInfo FontInfo;
			if (FontManager && FontManager->IsInitialized())
			{
				// Try to get the font based on inheritance or use default
				FString FontName = FS.Inherits.IsEmpty() ? TEXT("GameFontNormal") : FS.Inherits;
				// Scale font size with UI scale for proper sizing
				int32 ScaledFontSize = FMath::RoundToInt(FS.FontHeight * UIScale);
				FontInfo = FontManager->GetFont(FontName, ScaledFontSize);
			}
			else
			{
				// Fallback to default UE font (also scale the size)
				FontInfo = TextWidget->GetFont();
				FontInfo.Size = FMath::RoundToInt(FS.FontHeight * UIScale);
			}
			TextWidget->SetFont(FontInfo);

			// Apply justification
			if (FS.JustifyH == TEXT("LEFT"))
				TextWidget->SetJustification(ETextJustify::Left);
			else if (FS.JustifyH == TEXT("RIGHT"))
				TextWidget->SetJustification(ETextJustify::Right);
			else
				TextWidget->SetJustification(ETextJustify::Center);

			UCanvasPanelSlot* Slot = Container->AddChildToCanvas(TextWidget);
			if (Slot)
			{
				Slot->SetZOrder(ZOrder++);
			}

			ApplyElementAnchors(TextWidget, Container, FS.Anchors, FS.Width, FS.Height, false, EffectiveParentW, EffectiveParentH);

			if (!FS.Name.IsEmpty())
			{
				// Create a Lua global for this fontstring so scripts can access it
				if (EventSystem)
				{
					EventSystem->CreateTextureRegionGlobal(FS.Name, OwnerHandle, TEXT("FontString"));
				}
				NamedFontStringWidgets.Add(FS.Name, TextWidget);
				// Warn if fontstring name still contains unresolved $parent
				if (FS.Name.Contains(TEXT("$parent")))
				{
					UE_LOG(LogWowFrame, Warning, TEXT("  UNRESOLVED $parent in fontstring: %s (frame: %s)"), *FS.Name, *Def.Name);
				}

				if (!Def.Name.IsEmpty() && FS.Name == (Def.Name + TEXT("Text")))
				{
					PrimaryTextWidgets.Add(OwnerHandle, TextWidget);
				}

				// Register fontstring rect so other frames can anchor to it
				UCanvasPanelSlot* FSSlot = Cast<UCanvasPanelSlot>(TextWidget->Slot);
				if (FSSlot)
				{
					FFrameRect ParentAbsRect = FrameRects.Contains(Def.Name)
						? FrameRects[Def.Name]
						: FFrameRect(0.f, 0.f, EffectiveParentW, EffectiveParentH);
					FVector2D SlotPos = FSSlot->GetPosition();
					FVector2D SlotSize = FSSlot->GetSize();
					float ElemX = ParentAbsRect.X + SlotPos.X / UIScale;
					float ElemY = ParentAbsRect.Y + SlotPos.Y / UIScale;
					float ElemW = SlotSize.X / UIScale;
					float ElemH = SlotSize.Y / UIScale;
					FrameRects.Add(FS.Name, FFrameRect(ElemX, ElemY, ElemW, ElemH));
				}
			}
		}
	}
}

// ── Backdrop Creation ────────────────────────────────────────────────────────

void FWowFrameManager::CreateBackdrop(UCanvasPanel* Container, const FWowBackdrop& Backdrop)
{
	if (!Container) return;

	UObject* Outer = Container->GetOuter();
	if (!Outer) Outer = GetTransientPackage();

	// For now, implement a simple backdrop that just renders the background texture
	// A full 9-slice implementation would be more complex
	if (!Backdrop.BgFile.IsEmpty())
	{
		UTexture2D* BgTexture = LoadUITexture(Backdrop.BgFile);
		if (BgTexture)
		{
			UImage* BgImage = NewObject<UImage>(Outer);

			FSlateBrush SlateBrush;
			SlateBrush.SetResourceObject(BgTexture);
			SlateBrush.DrawAs = ESlateBrushDrawType::Image;
			BgImage->SetBrush(SlateBrush);

			// Add as first child (lowest Z order) so it appears behind other content
			UCanvasPanelSlot* Slot = Container->AddChildToCanvas(BgImage);
			if (Slot)
			{
				Slot->SetZOrder(-1000); // Behind all other elements
				// Fill the entire frame (insets are in WoW coords, scale them)
				Slot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
				Slot->SetOffsets(FMargin(
					Backdrop.InsetLeft * UIScale,
					Backdrop.InsetTop * UIScale,
					-Backdrop.InsetRight * UIScale,
					-Backdrop.InsetBottom * UIScale
				));
			}

			UE_LOG(LogWowFrame, Verbose, TEXT("Created backdrop with background: %s"), *Backdrop.BgFile);
		}
	}

	// TODO: Implement edge textures for proper 9-slice rendering
	// For now, we just render the background texture
}

// ── Widget Creation ──────────────────────────────────────────────────────────

UWidget* FWowFrameManager::CreateWidgetForFrame(const FWowFrameDef& Def, int64 Handle, int64 ParentHandle)
{
	UCanvasPanel* Canvas = RootCanvas.Get();
	if (!Canvas)
	{
		UE_LOG(LogWowFrame, Error, TEXT("No root canvas for frame creation"));
		return nullptr;
	}

	bool bForceHidden = false; // Set to true for orphaned frames whose parent doesn't exist

	// Determine parent canvas: use parent frame's canvas if it exists, otherwise root canvas
	UCanvasPanel* ParentCanvas = Canvas; // Default to root canvas

	if (ParentHandle != -1)
	{
		UWidget* ParentWidget = GetWidgetForHandle(ParentHandle);
		if (ParentWidget)
		{
			if (UCanvasPanel* ParentCanvasPanel = Cast<UCanvasPanel>(ParentWidget))
			{
				ParentCanvas = ParentCanvasPanel;
				UE_LOG(LogWowFrame, Verbose, TEXT("  %s: Using parent canvas from frame %lld"), *Def.Name, ParentHandle);
			}
			else
			{
				UE_LOG(LogWowFrame, Warning, TEXT("  %s: Parent frame %lld is not a canvas, using root canvas"), *Def.Name, ParentHandle);
			}
		}
		else
		{
			// Parent frame exists but has no widget (like UIParent or other virtual frames)
			// Use root canvas
			const FFrameEntry* ParentEntry = Frames.Find(ParentHandle);
			if (ParentEntry)
			{
				UE_LOG(LogWowFrame, Verbose, TEXT("  %s: Parent frame %lld (%s) is virtual, using root canvas"),
					*Def.Name, ParentHandle, *ParentEntry->Def.Name);
			}
		}
	}
	else if (!Def.Parent.IsEmpty())
	{
		UE_LOG(LogWowFrame, Warning, TEXT("  %s: Parent '%s' not found, using root canvas (hidden)"), *Def.Name, *Def.Parent);
		// Force-hide orphaned frames — they belong inside a container that doesn't exist yet
		bForceHidden = true;
	}

	UE_LOG(LogWowFrame, Log, TEXT("CreateFrame: %s type=%d canvas=%p parent=%p hidden=%d"),
		*Def.Name, (int32)Def.Type, Canvas, ParentCanvas, Def.bHidden);

	UObject* Outer = ParentCanvas->GetOuter();
	if (!Outer) Outer = GetTransientPackage();

	UWidget* Widget = nullptr;

	// WorldFrame is the 3D world viewport — skip widget creation (it's handled by UE5's viewport)
	if (Def.Type == EWowFrameType::WorldFrame)
	{
		UE_LOG(LogWowFrame, Log, TEXT("  %s: WorldFrame type — skipping widget (3D viewport)"), *Def.Name);
		return nullptr;
	}

	// ALL frame types use UCanvasPanel so they can contain children (WoW allows children on any frame type).
	// Button/EditBox/StatusBar/Slider behavior is handled through Lua scripts and event routing,
	// not UMG widget types. Using non-canvas widgets (UButton etc.) breaks the parent-child hierarchy
	// because UButton can't contain arbitrary positioned children.
	{
		UCanvasPanel* Container = NewObject<UCanvasPanel>(Outer);
		// WoW frames do NOT clip their layer content by default.
		// Layer textures (portraits, borders, decorations) regularly extend
		// beyond the frame bounds. Only clip if XML explicitly sets clipsChildren.
		Container->SetClipping(EWidgetClipping::Inherit);
		Widget = Container;
	}

	if (!Widget)
	{
		return nullptr;
	}

	UCanvasPanel* ContainerWidget = Cast<UCanvasPanel>(Widget);
	if (ContainerWidget)
	{
		auto AddFillChild = [this, ContainerWidget](UWidget* Child, int32 ZOrder, ESlateVisibility Visibility)
		{
			if (!Child) return;
			Child->SetVisibility(Visibility);
			if (UCanvasPanelSlot* ChildSlot = ContainerWidget->AddChildToCanvas(Child))
			{
				ChildSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
				ChildSlot->SetOffsets(FMargin(0.f));
				ChildSlot->SetZOrder(ZOrder);
			}
		};

		if (Def.Type == EWowFrameType::Button || Def.Type == EWowFrameType::CheckButton)
		{
			UButton* Button = NewObject<UButton>(ContainerWidget);
			// Make button transparent — WoW buttons have no default background.
			// Visual appearance comes from NormalTexture/PushedTexture/HighlightTexture.
			FButtonStyle TransparentStyle;
			FSlateBrush EmptyBrush;
			EmptyBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
			TransparentStyle.SetNormal(EmptyBrush);
			TransparentStyle.SetHovered(EmptyBrush);
			TransparentStyle.SetPressed(EmptyBrush);
			TransparentStyle.SetDisabled(EmptyBrush);
			TransparentStyle.SetNormalPadding(FMargin(0));
			TransparentStyle.SetPressedPadding(FMargin(0));
			Button->SetStyle(TransparentStyle);
			Button->SetBackgroundColor(FLinearColor::Transparent);
			// HitTestInvisible — clicks are routed via our custom HitTestFrames(),
			// not UMG's Slate hit-testing.  This prevents the transparent UButton
			// from consuming mouse events before the PlayerController sees them.
			AddFillChild(Button, -100, ESlateVisibility::HitTestInvisible);
			ButtonWidgets.Add(Handle, Button);

			if (!Def.ButtonText.IsEmpty())
			{
				UTextBlock* ButtonText = NewObject<UTextBlock>(ContainerWidget);
				ButtonText->SetText(FText::FromString(Def.ButtonText));
				ButtonText->SetJustification(ETextJustify::Center);
				FSlateFontInfo FontInfo = ButtonText->GetFont();
				FontInfo.Size = FMath::RoundToInt(12.f * UIScale);
				ButtonText->SetFont(FontInfo);
				AddFillChild(ButtonText, 250, ESlateVisibility::HitTestInvisible);
				PrimaryTextWidgets.Add(Handle, ButtonText);

				if (!Def.Name.IsEmpty())
				{
					const FString ButtonTextName = Def.Name + TEXT("Text");
					NamedFontStringWidgets.Add(ButtonTextName, ButtonText);
					if (EventSystem)
					{
						EventSystem->CreateTextureRegionGlobal(ButtonTextName, Handle, TEXT("FontString"));
					}
				}
			}
		}
		else if (Def.Type == EWowFrameType::EditBox)
		{
			UEditableTextBox* EditBox = NewObject<UEditableTextBox>(ContainerWidget);
			// Make edit box background transparent — WoW edit boxes use XML textures for appearance
			FEditableTextBoxStyle BoxStyle = EditBox->GetWidgetStyle();
			FSlateBrush EmptyBrush;
			EmptyBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
			BoxStyle.SetBackgroundImageNormal(EmptyBrush);
			BoxStyle.SetBackgroundImageHovered(EmptyBrush);
			BoxStyle.SetBackgroundImageFocused(EmptyBrush);
			BoxStyle.SetBackgroundImageReadOnly(EmptyBrush);
			BoxStyle.SetPadding(FMargin(2.f));
			EditBox->SetWidgetStyle(BoxStyle);
			AddFillChild(EditBox, 50, ESlateVisibility::Visible);
			EditBoxWidgets.Add(Handle, EditBox);
		}
		else if (Def.Type == EWowFrameType::Slider)
		{
			USlider* Slider = NewObject<USlider>(ContainerWidget);
			if (Def.Orientation.Equals(TEXT("VERTICAL"), ESearchCase::IgnoreCase))
			{
				Slider->SetOrientation(EOrientation::Orient_Vertical);
			}
			// Use transparent bar — WoW sliders use XML textures
			Slider->SetSliderBarColor(FLinearColor::Transparent);
			AddFillChild(Slider, 50, ESlateVisibility::Visible);
			SliderWidgets.Add(Handle, Slider);
		}
	}

	// Walk the full parent chain to check if ANY ancestor is hidden
	bool bParentHidden = false;
	{
		int64 CheckHandle = ParentHandle;
		int32 MaxDepth = 20; // prevent infinite loops
		while (CheckHandle >= 0 && MaxDepth-- > 0)
		{
			const FFrameEntry* AncestorEntry = Frames.Find(CheckHandle);
			if (!AncestorEntry) break;
			if (AncestorEntry->Def.bHidden || (AncestorEntry->Widget.IsValid() &&
				AncestorEntry->Widget->GetVisibility() == ESlateVisibility::Collapsed))
			{
				bParentHidden = true;
				break;
			}
			CheckHandle = AncestorEntry->ParentHandle;
		}
	}

	// Frames that WoW's Lua hides at startup but aren't marked hidden="true" in XML.
	// These are typically voice chat, quest info sub-frames, and other panels that
	// are shown conditionally by scripts we haven't fully implemented.
	static const TSet<FString> LuaHiddenFrames = {
		TEXT("LoopbackVUMeter"),
		TEXT("QuestInfoRequiredMoneyFrame"),
		TEXT("QuestInfoRequiredMoneyDisplay"),
		TEXT("WatchFrame"),  // Objectives tracker — shown by WatchFrame_Update Lua
	};
	bool bLuaHidden = LuaHiddenFrames.Contains(Def.Name);

	// Frames that WoW's Lua SHOWS at startup (via OnLoad scripts) but our FrameXML
	// scripts fail to execute. Force these visible so the UI is usable.
	// ActionButton1-12, their icons, and micro buttons are key examples.
	bool bForceVisible = false;
	if (Def.bHidden && !bForceHidden && !bParentHidden)
	{
		// ActionButton1-12, MultiBarBottomLeftButton1-12, etc.
		if (Def.Name.StartsWith(TEXT("ActionButton")) ||
			Def.Name.StartsWith(TEXT("MultiBarBottomLeftButton")) ||
			Def.Name.StartsWith(TEXT("MultiBarBottomRightButton")) ||
			Def.Name.StartsWith(TEXT("MultiBarRightButton")) ||
			Def.Name.StartsWith(TEXT("MultiBarLeftButton")))
		{
			bForceVisible = true;
		}
	}

	if ((Def.bHidden && !bForceVisible) || bForceHidden || bParentHidden || bLuaHidden)
	{
		Widget->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		// All frame containers use SelfHitTestInvisible — clicks pass through to the
		// 3D world and are routed to Lua via our custom HitTestFrames() in the
		// PlayerController.  EditBox and Slider children that need UMG interaction
		// set their own visibility to Visible.
		Widget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	// Add to parent canvas (may be root canvas or a parent frame's canvas)
	UCanvasPanelSlot* Slot = ParentCanvas->AddChildToCanvas(Widget);

	// Apply strata z-ordering (1000 per strata level + frame level offset)
	if (Slot)
	{
		int32 ZOrder = static_cast<int32>(Def.Strata) * 1000 + Def.FrameLevel;
		Slot->SetZOrder(ZOrder);
		UE_LOG(LogWowFrame, Verbose, TEXT("  Added to canvas: slot=%p ZOrder=%d"), Slot, ZOrder);
	}
	else
	{
		UE_LOG(LogWowFrame, Error, TEXT("  Failed to add widget to canvas!"));
	}

	// Apply anchor positioning
	ApplyAnchors(Widget, Def);

	// Ensure frame has minimum size if not auto-sizing
	if (Slot && Def.Width > 0.f && Def.Height > 0.f)
	{
		// Double-check that size was set (in case ApplyAnchors didn't handle it)
		FVector2D CurrentSize = Slot->GetSize();
		if (CurrentSize.X == 0.f && CurrentSize.Y == 0.f)
		{
			Slot->SetSize(FVector2D(Def.Width * UIScale, Def.Height * UIScale));
			UE_LOG(LogWowFrame, Warning, TEXT("  %s: Fallback size set to %.1fx%.1f (scaled)"), *Def.Name, Def.Width * UIScale, Def.Height * UIScale);
		}
	}

	// Create UProgressBar child for StatusBar frames
	if (Def.Type == EWowFrameType::StatusBar)
	{
		UProgressBar* Bar = NewObject<UProgressBar>(Widget);
		// Transparent style by default — Lua will set fill color via :SetStatusBarColor()
		// Remove the default UE grey background/border
		FSlateBrush EmptyBrush;
		EmptyBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
		FProgressBarStyle BarStyle;
		BarStyle.SetBackgroundImage(EmptyBrush);
		Bar->SetWidgetStyle(BarStyle);
		Bar->SetFillColorAndOpacity(FLinearColor::Green); // default, Lua will override
		Bar->SetPercent(0.0f);
		UCanvasPanel* CanvasPanel = Cast<UCanvasPanel>(Widget);
		UCanvasPanelSlot* BarSlot = CanvasPanel ? CanvasPanel->AddChildToCanvas(Bar) : nullptr;
		if (BarSlot)
		{
			BarSlot->SetAnchors(FAnchors(0, 0));
			BarSlot->SetPosition(FVector2D(0, 0));
			BarSlot->SetSize(FVector2D(Def.Width * UIScale, Def.Height * UIScale));
		}
		StatusBarWidgets.Add(Handle, Bar);
	}

	// Create backdrop (9-slice background) if specified
	if (UCanvasPanel* BackdropCanvas = Cast<UCanvasPanel>(Widget); Def.Backdrop.IsSet() && BackdropCanvas)
	{
		CreateBackdrop(BackdropCanvas, Def.Backdrop.GetValue());
	}

	// Create layer content (textures and fontstrings) inside the frame
	if (Def.Layers.Num() > 0)
	{
		UCanvasPanel* Container = Cast<UCanvasPanel>(Widget);
		// Only create layers for canvas-type frames; skip for buttons/statusbars/etc.
		// to avoid dumping textures onto the root canvas where they'd fill the screen
		if (Container)
		{
			CreateLayerContent(Container, Def, Handle);
		}
	}

	return Widget;
}

// ── $parent Resolution ───────────────────────────────────────────────────────

void FWowFrameManager::ResolveParentReferences(FWowFrameDef& Def, const FString& ParentName)
{
	// Resolve $parent in the frame name itself
	if (Def.Name.StartsWith(TEXT("$parent")) && !ParentName.IsEmpty())
	{
		FString ResolvedName = Def.Name;
		ResolvedName.ReplaceInline(TEXT("$parent"), *ParentName);
		Def.Name = ResolvedName;

		UE_LOG(LogWowFrame, Verbose, TEXT("Resolved frame name: $parent -> %s"), *Def.Name);
	}

	// Resolve $parent in anchor RelativeTo fields
	for (FWowAnchor& Anchor : Def.Anchors)
	{
		if (Anchor.RelativeTo.StartsWith(TEXT("$parent")) && !ParentName.IsEmpty())
		{
			FString ResolvedRelativeTo = Anchor.RelativeTo;
			ResolvedRelativeTo.ReplaceInline(TEXT("$parent"), *ParentName);
			Anchor.RelativeTo = ResolvedRelativeTo;

			UE_LOG(LogWowFrame, Verbose, TEXT("Resolved anchor RelativeTo: $parent -> %s"), *Anchor.RelativeTo);
		}
	}

	// Resolve $parent in layer content (textures and fontstrings)
	// $parent in layer content refers to the FRAME itself. For anonymous frames
	// (no name), fall back to using ParentName (the frame's parent).
	const FString& FrameName = Def.Name.IsEmpty() ? ParentName : Def.Name;
	for (FWowLayer& Layer : Def.Layers)
	{
		// Resolve in texture elements
		for (FWowTextureElement& Texture : Layer.Textures)
		{
			// Resolve texture name: $parentBorder → ActionButton1Border
			if (Texture.Name.Contains(TEXT("$parent")) && !FrameName.IsEmpty())
			{
				Texture.Name.ReplaceInline(TEXT("$parent"), *FrameName);
			}

			// Resolve texture anchor RelativeTo fields
			for (FWowAnchor& TexAnchor : Texture.Anchors)
			{
				if (TexAnchor.RelativeTo.Contains(TEXT("$parent")) && !FrameName.IsEmpty())
				{
					TexAnchor.RelativeTo.ReplaceInline(TEXT("$parent"), *FrameName);
				}
			}
		}

		// Resolve in fontstring elements
		for (FWowFontStringElement& FontString : Layer.FontStrings)
		{
			// Resolve fontstring name: $parentName → PlayerFrameName
			if (FontString.Name.Contains(TEXT("$parent")) && !FrameName.IsEmpty())
			{
				FontString.Name.ReplaceInline(TEXT("$parent"), *FrameName);
			}

			// Resolve fontstring anchor RelativeTo fields
			for (FWowAnchor& FSAnchor : FontString.Anchors)
			{
				if (FSAnchor.RelativeTo.Contains(TEXT("$parent")) && !FrameName.IsEmpty())
				{
					FSAnchor.RelativeTo.ReplaceInline(TEXT("$parent"), *FrameName);
				}
			}
		}
	}

	// Resolve $parent in button texture names (NormalTexture, PushedTexture, etc.)
	if (!FrameName.IsEmpty())
	{
		if (Def.NormalTextureName.Contains(TEXT("$parent")))
			Def.NormalTextureName.ReplaceInline(TEXT("$parent"), *FrameName);
		if (Def.PushedTextureName.Contains(TEXT("$parent")))
			Def.PushedTextureName.ReplaceInline(TEXT("$parent"), *FrameName);
		if (Def.HighlightTextureName.Contains(TEXT("$parent")))
			Def.HighlightTextureName.ReplaceInline(TEXT("$parent"), *FrameName);
		if (Def.DisabledTextureName.Contains(TEXT("$parent")))
			Def.DisabledTextureName.ReplaceInline(TEXT("$parent"), *FrameName);
	}

	for (FString& NamedObject : Def.NamedObjectGlobals)
	{
		if (NamedObject.Contains(TEXT("$parent")) && !FrameName.IsEmpty())
		{
			NamedObject.ReplaceInline(TEXT("$parent"), *FrameName);
		}
	}

	// Recursively resolve $parent in child frames.
	// For anonymous frames (empty name), pass the effective name (ParentName)
	// so grandchildren can still resolve $parent.
	const FString& EffectiveName = Def.Name.IsEmpty() ? ParentName : Def.Name;
	for (FWowFrameDef& Child : Def.Children)
	{
		ResolveParentReferences(Child, EffectiveName);
	}
}

// ── Frame Creation ───────────────────────────────────────────────────────────

int64 FWowFrameManager::CreateFrame(const FWowFrameDef& Def)
{
	// Resolve template inheritance first
	FWowFrameDef Resolved = ResolveInherits(Def);

	// Resolve $parent references in frame names and anchors.
	// SKIP for virtual frames — they are templates whose children should keep
	// $parent unresolved so it gets resolved during instantiation.
	if (!Resolved.bVirtual)
	{
		// Always resolve, even if Parent is empty — layer content ($parent in
		// texture/fontstring names) uses the frame's OWN name, not the parent.
		FString ParentForResolution = Resolved.Parent.IsEmpty() ? Resolved.Name : Resolved.Parent;
		ResolveParentReferences(Resolved, ParentForResolution);
	}

	int64 Handle = NextHandle++;
	FFrameEntry Entry;
	Entry.Def = Resolved;
	Entry.bMouseEnabled = Resolved.bEnableMouse ||
		Resolved.Type == EWowFrameType::Button ||
		Resolved.Type == EWowFrameType::CheckButton ||
		Resolved.Type == EWowFrameType::EditBox ||
		Resolved.Type == EWowFrameType::Slider;
	Entry.bKeyboardEnabled = Resolved.bEnableKeyboard ||
		Resolved.Type == EWowFrameType::EditBox;
	Entry.bMouseWheelEnabled = false;

	// Resolve parent handle
	if (!Resolved.Parent.IsEmpty())
	{
		Entry.ParentHandle = FindFrame(Resolved.Parent);
		if (Entry.ParentHandle == -1)
		{
			UE_LOG(LogWowFrame, Warning, TEXT("Parent frame '%s' not found for frame '%s'"), *Resolved.Parent, *Resolved.Name);
		}
	}

	// Handle virtual frames (like templates and UIParent)
	if (Resolved.bVirtual)
	{
		// Virtual frames don't get widgets but are stored for reference/inheritance
		Entry.Widget = nullptr;
		Frames.Add(Handle, MoveTemp(Entry));

		if (!Resolved.Name.IsEmpty())
		{
			NameToHandle.Add(Resolved.Name, Handle);
			RegisterTemplate(Resolved.Name, Resolved);
		}

		UE_LOG(LogWowFrame, Log, TEXT("Created virtual frame [%lld] %s"), Handle, *Resolved.Name);
		return Handle;
	}

	// Create the widget for non-virtual frames
	Entry.Widget = CreateWidgetForFrame(Resolved, Handle, Entry.ParentHandle);

	Frames.Add(Handle, MoveTemp(Entry));

	if (!Resolved.Name.IsEmpty())
	{
		NameToHandle.Add(Resolved.Name, Handle);
	}

	// Register frame as Lua global FIRST (so children and scripts can find it)
	if (EventSystem && !Resolved.Name.IsEmpty())
	{
		EventSystem->CreateFrameObject(Handle, Resolved.Name, Resolved.FrameID);

		if (Entry.ParentHandle != -1)
		{
			const FString ParentName = GetFrameName(Entry.ParentHandle);
			const FString AliasName = BuildParentFrameAliasName(ParentName, Resolved.Name);
			if (!AliasName.IsEmpty())
			{
				EventSystem->AliasGlobalObject(Resolved.Name, AliasName);
			}
		}

		// Create Lua globals for button texture names (e.g., ActionButton1NormalTexture)
		// These are needed by GetNormalTexture() etc.
		if (!Resolved.NormalTextureName.IsEmpty())
			EventSystem->CreateTextureRegionGlobal(Resolved.NormalTextureName, Handle, TEXT("Texture"));
		if (!Resolved.PushedTextureName.IsEmpty())
			EventSystem->CreateTextureRegionGlobal(Resolved.PushedTextureName, Handle, TEXT("Texture"));
		if (!Resolved.HighlightTextureName.IsEmpty())
			EventSystem->CreateTextureRegionGlobal(Resolved.HighlightTextureName, Handle, TEXT("Texture"));
		if (!Resolved.DisabledTextureName.IsEmpty())
			EventSystem->CreateTextureRegionGlobal(Resolved.DisabledTextureName, Handle, TEXT("Texture"));

		for (const FString& NamedObject : Resolved.NamedObjectGlobals)
		{
			if (!NamedObject.IsEmpty())
			{
				EventSystem->CreateTextureRegionGlobal(NamedObject, Handle, TEXT("Animation"));
			}
		}
	}

	// Create child frames BEFORE firing OnLoad (WoW creates children first,
	// then fires parent OnLoad — scripts like ActionButton_OnLoad expect
	// _G["ActionButton1Cooldown"] to exist when OnLoad runs)
	for (const FWowFrameDef& ChildDef : Resolved.Children)
	{
		FWowFrameDef ChildWithParent = ChildDef;
		if (ChildWithParent.Parent.IsEmpty() && !Resolved.Name.IsEmpty())
		{
			ChildWithParent.Parent = Resolved.Name;
		}
		CreateFrame(ChildWithParent);
	}

	// NOW compile and fire OnLoad — all children exist
	// Always call CompileFrameScripts so child-field mapping and self.name are set,
	// even for frames without scripts (they still need fields for parent scripts to access)
	if (EventSystem)
	{
		EventSystem->CompileFrameScripts(Handle, Resolved);
	}

	UE_LOG(LogWowFrame, Log, TEXT("Created frame [%lld] %s (type %d, %d layers, %d children, widget=%p)"),
		Handle, *Resolved.Name, (int32)Resolved.Type, Resolved.Layers.Num(), Resolved.Children.Num(), Entry.Widget.Get());
	return Handle;
}

// ── Texture Loading ──────────────────────────────────────────────────────

UTexture2D* FWowFrameManager::LoadUITexture(const FString& TexturePath)
{
	if (TexturePath.IsEmpty() || !MpqManager || !AssetCache)
	{
		return nullptr;
	}

	// Check cache first
	if (TWeakObjectPtr<UTexture2D>* CachedPtr = TextureCache.Find(TexturePath))
	{
		if (CachedPtr->IsValid())
		{
			return CachedPtr->Get();
		}
		else
		{
			// Remove stale entry
			TextureCache.Remove(TexturePath);
		}
	}

	// Check asset cache
	UTexture2D* ExistingTexture = AssetCache->FindTexture(TexturePath);
	if (ExistingTexture)
	{
		TextureCache.Add(TexturePath, ExistingTexture);
		return ExistingTexture;
	}

	// Convert interface path to BLP file path
	FString BlpPath = TexturePath;
	// Replace known extensions with .blp
	if (BlpPath.EndsWith(TEXT(".tga"), ESearchCase::IgnoreCase))
	{
		BlpPath = BlpPath.Left(BlpPath.Len() - 4) + TEXT(".blp");
	}
	else if (BlpPath.EndsWith(TEXT(".png"), ESearchCase::IgnoreCase))
	{
		BlpPath = BlpPath.Left(BlpPath.Len() - 4) + TEXT(".blp");
	}
	else if (!BlpPath.EndsWith(TEXT(".blp"), ESearchCase::IgnoreCase))
	{
		BlpPath += TEXT(".blp");
	}

	// Read BLP file from MPQ
	TArray<uint8> BlpData;
	if (!MpqManager->ReadFile(BlpPath, BlpData))
	{
		UE_LOG(LogWowFrame, Warning, TEXT("Failed to read UI texture: %s"), *BlpPath);
		return nullptr;
	}

	// Parse BLP
	FBlpTexture BlpTexture = FBlpParser::Parse(BlpData);
	if (!BlpTexture.bIsValid)
	{
		UE_LOG(LogWowFrame, Warning, TEXT("Failed to parse UI texture: %s"), *BlpPath);
		return nullptr;
	}

	// Create UTexture2D
	UTexture2D* Texture = FWowTextureFactory::CreateTexture(BlpTexture, TexturePath);
	if (!Texture)
	{
		UE_LOG(LogWowFrame, Warning, TEXT("Failed to create UI texture: %s"), *BlpPath);
		return nullptr;
	}

	// Cache the texture
	AssetCache->CacheTexture(TexturePath, Texture);
	TextureCache.Add(TexturePath, Texture);

	UE_LOG(LogWowFrame, Verbose, TEXT("Loaded UI texture: %s (%dx%d)"),
		*TexturePath, Texture->GetSizeX(), Texture->GetSizeY());

	return Texture;
}

int64 FWowFrameManager::CreateDebugFrame(const FString& Name, float Width, float Height, float X, float Y)
{
	// Create a simple debug frame with a background and text
	FWowFrameDef DebugDef;
	DebugDef.Name = Name;
	DebugDef.Type = EWowFrameType::Frame;
	DebugDef.Width = Width;
	DebugDef.Height = Height;
	DebugDef.Strata = EWowFrameStrata::DIALOG;
	DebugDef.FrameLevel = 10;

	// Add anchor to position it
	FWowAnchor Anchor;
	Anchor.Point = EWowAnchorPoint::TOPLEFT;
	Anchor.RelativePoint = EWowAnchorPoint::TOPLEFT;
	Anchor.OffsetX = X;
	Anchor.OffsetY = Y;
	DebugDef.Anchors.Add(Anchor);

	// Add a backdrop for visual feedback
	FWowBackdrop Backdrop;
	// For now, create a debug frame without backdrop since we need texture files
	// A real implementation would load Interface/DialogFrame/UI-DialogBox-Background.blp

	// Add a text layer to show the frame name
	FWowLayer TextLayer;
	TextLayer.Level = EWowDrawLayer::OVERLAY;

	FWowFontStringElement DebugText;
	DebugText.Name = Name + TEXT("_Text");
	DebugText.Text = Name + FString::Printf(TEXT(" (%.0fx%.0f)"), Width, Height);
	DebugText.FontHeight = 12.f;
	DebugText.Color = FLinearColor::Yellow;
	DebugText.JustifyH = TEXT("CENTER");

	// Center the text in the frame
	FWowAnchor TextAnchor;
	TextAnchor.Point = EWowAnchorPoint::CENTER;
	TextAnchor.RelativePoint = EWowAnchorPoint::CENTER;
	DebugText.Anchors.Add(TextAnchor);

	TextLayer.FontStrings.Add(DebugText);
	DebugDef.Layers.Add(TextLayer);

	int64 Handle = CreateFrame(DebugDef);
	UE_LOG(LogWowFrame, Warning, TEXT("Created debug frame '%s' [%lld] at (%.0f,%.0f) size %.0fx%.0f"),
		*Name, Handle, X, Y, Width, Height);

	return Handle;
}

void FWowFrameManager::SyncChildVisibility()
{
	// After all OnLoad scripts fire, some parents got hidden but their children
	// were already created visible. Walk the tree and hide children of hidden ancestors.
	int32 HiddenCount = 0;
	for (auto& Pair : Frames)
	{
		if (!Pair.Value.Widget.IsValid()) continue;
		if (Pair.Value.Def.bHidden) continue; // Already hidden by itself

		// Walk parent chain
		int64 CheckHandle = Pair.Value.ParentHandle;
		int32 MaxDepth = 20;
		bool bAncestorHidden = false;
		while (CheckHandle >= 0 && MaxDepth-- > 0)
		{
			const FFrameEntry* Ancestor = Frames.Find(CheckHandle);
			if (!Ancestor) break;
			if (Ancestor->Def.bHidden || (Ancestor->Widget.IsValid() &&
				Ancestor->Widget->GetVisibility() == ESlateVisibility::Collapsed))
			{
				bAncestorHidden = true;
				break;
			}
			CheckHandle = Ancestor->ParentHandle;
		}

		if (bAncestorHidden && Pair.Value.Widget->GetVisibility() != ESlateVisibility::Collapsed)
		{
			Pair.Value.Widget->SetVisibility(ESlateVisibility::Collapsed);
			HiddenCount++;
		}
	}

	if (HiddenCount > 0)
	{
		UE_LOG(LogWowFrame, Log, TEXT("SyncChildVisibility: hid %d orphaned visible children of hidden parents"), HiddenCount);
	}

	// Force-show frames that WoW's Lua would show at startup but our FrameXML
	// scripts can't execute (missing API stubs cause OnLoad to fail/hide them).
	int32 ForceShownCount = 0;
	for (auto& Pair : Frames)
	{
		if (!Pair.Value.Widget.IsValid()) continue;
		const FString& Name = Pair.Value.Def.Name;

		bool bShouldForceShow = false;

		// ActionButton1-12 (main action bar buttons)
		if (Name.StartsWith(TEXT("ActionButton")) && Name.Len() <= 14)
			bShouldForceShow = true;
		// BonusActionButton1-12
		if (Name.StartsWith(TEXT("BonusActionButton")))
			bShouldForceShow = true;

		if (bShouldForceShow && Pair.Value.Widget->GetVisibility() == ESlateVisibility::Collapsed)
		{
			// Check parent is actually visible first
			bool bParentVis = true;
			int64 PH = Pair.Value.ParentHandle;
			if (PH >= 0)
			{
				const FFrameEntry* PE = Frames.Find(PH);
				if (PE && PE->Widget.IsValid() && PE->Widget->GetVisibility() == ESlateVisibility::Collapsed)
					bParentVis = false;
			}
			if (bParentVis)
			{
				bool bInteractive = (Pair.Value.Def.Type == EWowFrameType::Button ||
					Pair.Value.Def.Type == EWowFrameType::CheckButton);
				Pair.Value.Widget->SetVisibility(bInteractive ? ESlateVisibility::Visible : ESlateVisibility::SelfHitTestInvisible);
				ForceShownCount++;
			}
		}
	}
	if (ForceShownCount > 0)
	{
		UE_LOG(LogWowFrame, Log, TEXT("SyncChildVisibility: force-showed %d action bar frames"), ForceShownCount);
	}

	// Force-hide frames that WoW expects to start hidden but our XML/template
	// system didn't mark as hidden (e.g., FadingFrame templates not in MPQ XML).
	static const TCHAR* ForceHideFrames[] = {
		TEXT("ZoneTextFrame"),
		TEXT("SubZoneTextFrame"),
		TEXT("AutoFollowStatusText"),
		TEXT("PVPInfoTextString"),
		TEXT("ReadyCheckListenerFrame"),
		TEXT("ReadyCheckFrame"),
	};
	int32 ForceHiddenCount = 0;
	for (const TCHAR* FrameName : ForceHideFrames)
	{
		int64 H = FindFrame(FrameName);
		if (H >= 0)
		{
			SetFrameVisible(H, false);
			ForceHiddenCount++;
		}
	}
	if (ForceHiddenCount > 0)
	{
		UE_LOG(LogWowFrame, Log, TEXT("SyncChildVisibility: force-hid %d frames (FadingFrame pattern)"), ForceHiddenCount);
	}

	// Dump all visible frames with their positions for debugging
	UE_LOG(LogWowFrame, Warning, TEXT("=== VISIBLE FRAMES (post-sync) ==="));
	for (const auto& Pair : Frames)
	{
		if (!Pair.Value.Widget.IsValid()) continue;
		if (Pair.Value.Widget->GetVisibility() == ESlateVisibility::Collapsed) continue;

		UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Pair.Value.Widget->Slot);
		if (!Slot) continue;
		FVector2D Pos = Slot->GetPosition();
		FVector2D Sz = Slot->GetSize();
		// Only log frames with significant area (skip zero-size and tiny frames)
		if (Sz.X * Sz.Y > 100.f)
		{
			UE_LOG(LogWowFrame, Warning, TEXT("  VISIBLE: %-40s pos=(%.0f,%.0f) sz=(%.0f,%.0f) parent='%s'"),
				*Pair.Value.Def.Name, Pos.X, Pos.Y, Sz.X, Sz.Y, *Pair.Value.Def.Parent);
		}
	}
}

void FWowFrameManager::DebugDumpLayout() const
{
	// Dump key WoW UI frames with expected vs actual sizes
	static const TCHAR* KeyFrames[] = {
		TEXT("UIParent"), TEXT("PlayerFrame"), TEXT("TargetFrame"),
		TEXT("MainMenuBar"), TEXT("MainMenuBarArtFrame"), TEXT("MinimapCluster"),
		TEXT("Minimap"), TEXT("ActionButton1"), TEXT("MultiBarBottomLeft"),
		TEXT("ChatFrame1"), TEXT("BuffFrame"), TEXT("CastingBarFrame"),
	};

	UE_LOG(LogWowFrame, Warning, TEXT("═══════════ LAYOUT DEBUG DUMP ═══════════"));
	UE_LOG(LogWowFrame, Warning, TEXT("UIScale=%.3f  RootCanvas=%p  TotalFrames=%d"),
		UIScale, RootCanvas.Get(), Frames.Num());

	for (const TCHAR* FrameName : KeyFrames)
	{
		const int64* HandlePtr = NameToHandle.Find(FrameName);
		if (!HandlePtr) {
			UE_LOG(LogWowFrame, Warning, TEXT("  %-25s  NOT FOUND"), FrameName);
			continue;
		}

		const FFrameEntry* Entry = Frames.Find(*HandlePtr);
		if (!Entry) continue;

		const FWowFrameDef& Def = Entry->Def;
		UWidget* W = Entry->Widget.IsValid() ? Entry->Widget.Get() : nullptr;

		// Get stored rect
		const FFrameRect* Rect = FrameRects.Find(FrameName);
		float RX = Rect ? Rect->X : -1, RY = Rect ? Rect->Y : -1;
		float RW = Rect ? Rect->W : -1, RH = Rect ? Rect->H : -1;

		// Get actual slot info
		FString SlotInfo = TEXT("no widget");
		if (W)
		{
			UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(W->Slot);
			if (Slot)
			{
				FVector2D Pos = Slot->GetPosition();
				FVector2D Size = Slot->GetSize();
				FAnchors Anch = Slot->GetAnchors();
				SlotInfo = FString::Printf(TEXT("slot=(%.0f,%.0f) sz=(%.0f,%.0f) anch=(%.1f,%.1f,%.1f,%.1f) vis=%d clip=%d"),
					Pos.X, Pos.Y, Size.X, Size.Y,
					Anch.Minimum.X, Anch.Minimum.Y, Anch.Maximum.X, Anch.Maximum.Y,
					(int32)W->GetVisibility(), (int32)W->GetClipping());
			}
			else
			{
				SlotInfo = TEXT("no slot");
			}
		}

		UE_LOG(LogWowFrame, Warning, TEXT("  %-25s  rect=(%.0f,%.0f,%.0f,%.0f) hidden=%d setAll=%d parent='%s' %s"),
			FrameName, RX, RY, RW, RH, Def.bHidden, Def.bSetAllPoints, *Def.Parent, *SlotInfo);
	}

	UE_LOG(LogWowFrame, Warning, TEXT("═══════════ END LAYOUT DUMP ═══════════"));
}

// ── Mouse Hit-Testing & Click Dispatch ───────────────────────────────────────

int64 FWowFrameManager::HitTestFrames(float ScreenX, float ScreenY) const
{
	// Convert viewport pixels to WoW coordinates using the RAW viewport scale
	// (not UIScale, which includes DPI adjustment for UMG positioning).
	// GetMousePosition() returns logical viewport coords; dividing by
	// RawViewportScale gives WoW coords that match our FrameRects.
	float WowX = ScreenX / RawViewportScale;
	float WowY = ScreenY / RawViewportScale;

	// Find the topmost interactive frame under the cursor.
	// Walk all frames and pick the one with highest z-order that contains the point.
	int64 BestHandle = -1;
	int32 BestZOrder = -999999;

	for (const auto& Pair : Frames)
	{
		const FFrameEntry& Entry = Pair.Value;
		if (!Entry.Widget.IsValid()) continue;
		if (Entry.Widget->GetVisibility() == ESlateVisibility::Collapsed) continue;

		if (!Entry.bMouseEnabled)
		{
			continue;
		}

		// Look up cached frame rect (absolute WoW coords)
		const FFrameRect* Rect = FrameRects.Find(Entry.Def.Name);
		if (!Rect) continue;
		if (Rect->W <= 0.f || Rect->H <= 0.f) continue;

		// Point-in-rect test
		if (WowX >= Rect->X && WowX <= Rect->X + Rect->W &&
			WowY >= Rect->Y && WowY <= Rect->Y + Rect->H)
		{
			// Z-order = strata * 1000 + frame level
			int32 ZOrder = static_cast<int32>(Entry.Def.Strata) * 1000 + Entry.Def.FrameLevel;
			if (ZOrder > BestZOrder)
			{
				BestZOrder = ZOrder;
				BestHandle = Pair.Key;
			}
		}
	}

	if (BestHandle >= 0)
	{
		const FFrameEntry* HitEntry = Frames.Find(BestHandle);
		UE_LOG(LogWowFrame, Log, TEXT("HitTest: screen=(%.0f,%.0f) wow=(%.1f,%.1f) -> %s [%lld] z=%d"),
			ScreenX, ScreenY, WowX, WowY,
			HitEntry ? *HitEntry->Def.Name : TEXT("?"), BestHandle, BestZOrder);
	}

	return BestHandle;
}

bool FWowFrameManager::IsMouseOverFrame(int64 Handle, float TopOffset, float BottomOffset, float LeftOffset, float RightOffset) const
{
	const FFrameEntry* Entry = Frames.Find(Handle);
	if (!Entry || !Entry->Widget.IsValid())
	{
		return false;
	}

	if (Entry->Widget->GetVisibility() == ESlateVisibility::Collapsed)
	{
		return false;
	}

	const FFrameRect* Rect = FrameRects.Find(Entry->Def.Name);
	if (!Rect || Rect->W <= 0.f || Rect->H <= 0.f || RawViewportScale <= 0.f)
	{
		return false;
	}

	if (!FSlateApplication::IsInitialized())
	{
		return false;
	}

	const FVector2D CursorPos = FSlateApplication::Get().GetCursorPos();
	const float WowX = CursorPos.X / RawViewportScale;
	const float WowY = CursorPos.Y / RawViewportScale;

	const float Left = Rect->X + LeftOffset;
	const float Right = Rect->X + Rect->W + RightOffset;
	const float Top = Rect->Y - TopOffset;
	const float Bottom = Rect->Y + Rect->H - BottomOffset;
	return WowX >= Left && WowX <= Right && WowY >= Top && WowY <= Bottom;
}

void FWowFrameManager::TickWidgetEvents()
{
	// Poll EditBox widgets for text changes
	for (auto& Pair : EditBoxWidgets)
	{
		if (!Pair.Value.IsValid()) continue;
		FString CurrentText = Pair.Value->GetText().ToString();
		FString* LastText = EditBoxLastText.Find(Pair.Key);
		if (!LastText)
		{
			EditBoxLastText.Add(Pair.Key, CurrentText);
		}
		else if (*LastText != CurrentText)
		{
			*LastText = CurrentText;
			if (EventSystem)
			{
				EventSystem->RunFrameScript(Pair.Key, TEXT("OnTextChanged"));
			}
		}
	}

	// Poll Slider widgets for value changes
	for (auto& Pair : SliderWidgets)
	{
		if (!Pair.Value.IsValid()) continue;
		float CurrentValue = Pair.Value->GetValue();
		float* LastValue = SliderLastValue.Find(Pair.Key);
		if (!LastValue)
		{
			SliderLastValue.Add(Pair.Key, CurrentValue);
		}
		else if (!FMath::IsNearlyEqual(*LastValue, CurrentValue))
		{
			*LastValue = CurrentValue;
			if (EventSystem)
			{
				EventSystem->RunFrameScript(Pair.Key, TEXT("OnValueChanged"),
					{FString::SanitizeFloat(CurrentValue)});
			}
		}
	}
}

bool FWowFrameManager::DispatchEditBoxEnterPressed()
{
	for (const auto& Pair : EditBoxWidgets)
	{
		if (!Pair.Value.IsValid()) continue;
		if (Pair.Value->HasKeyboardFocus())
		{
			if (EventSystem)
			{
				EventSystem->RunFrameScript(Pair.Key, TEXT("OnEnterPressed"));
			}
			return true;
		}
	}
	return false;
}

bool FWowFrameManager::DispatchEditBoxEscapePressed()
{
	for (const auto& Pair : EditBoxWidgets)
	{
		if (!Pair.Value.IsValid()) continue;
		if (Pair.Value->HasKeyboardFocus())
		{
			if (EventSystem)
			{
				EventSystem->RunFrameScript(Pair.Key, TEXT("OnEscapePressed"));
			}
			return true;
		}
	}
	return false;
}

void FWowFrameManager::UpdateMouseHover(float ScreenX, float ScreenY)
{
	int64 NewHover = HitTestFrames(ScreenX, ScreenY);

	if (NewHover != HoverFrameHandle)
	{
		// Dispatch OnLeave for the old frame
		if (HoverFrameHandle >= 0 && EventSystem)
		{
			EventSystem->RunFrameScript(HoverFrameHandle, TEXT("OnLeave"), {TEXT("0")});
		}

		HoverFrameHandle = NewHover;

		// Dispatch OnEnter for the new frame
		if (HoverFrameHandle >= 0 && EventSystem)
		{
			EventSystem->RunFrameScript(HoverFrameHandle, TEXT("OnEnter"), {TEXT("0")});
		}
	}
}

void FWowFrameManager::DispatchMouseDown(int64 Handle, const FString& Button)
{
	if (!EventSystem || Handle < 0) return;
	EventSystem->RunFrameScript(Handle, TEXT("OnMouseDown"), {Button});
}

void FWowFrameManager::DispatchMouseUp(int64 Handle, const FString& Button)
{
	if (!EventSystem || Handle < 0) return;
	EventSystem->RunFrameScript(Handle, TEXT("OnMouseUp"), {Button});
}

void FWowFrameManager::DispatchClick(int64 Handle, const FString& Button)
{
	if (!EventSystem || Handle < 0) return;
	// WoW OnClick signature: function(self, button, down)
	EventSystem->RunFrameScript(Handle, TEXT("OnClick"), {Button, TEXT("false")});
}

bool FWowFrameManager::DispatchReceiveDrag(int64 Handle)
{
	if (!EventSystem || Handle < 0)
	{
		return false;
	}

	return EventSystem->RunFrameScript(Handle, TEXT("OnReceiveDrag"));
}
