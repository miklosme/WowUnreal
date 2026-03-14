#include "WowFrameManager.h"
#include "WowEventSystem.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/ProgressBar.h"
#include "Components/Slider.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowFrame, Log, All);

FWowFrameManager::FWowFrameManager()
{
}

void FWowFrameManager::Initialize(UCanvasPanel* InRootCanvas)
{
	RootCanvas = InRootCanvas;
	UE_LOG(LogWowFrame, Log, TEXT("Frame manager initialized"));
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
		Entry->Widget->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
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

	if (Entry->Widget.IsValid())
	{
		if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Entry->Widget->Slot))
		{
			Slot->SetSize(FVector2D(W, H));
		}
	}
}

void FWowFrameManager::SetFrameAnchors(int64 Handle, const TArray<FWowAnchor>& NewAnchors)
{
	FFrameEntry* Entry = Frames.Find(Handle);
	if (!Entry) return;

	Entry->Def.Anchors = NewAnchors;
	Entry->Def.bSetAllPoints = false;

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
	if (Target.NormalTexture.IsEmpty()) Target.NormalTexture = Template.NormalTexture;
	if (Target.PushedTexture.IsEmpty()) Target.PushedTexture = Template.PushedTexture;
	if (Target.HighlightTexture.IsEmpty()) Target.HighlightTexture = Template.HighlightTexture;
	if (Target.DisabledTexture.IsEmpty()) Target.DisabledTexture = Template.DisabledTexture;
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

static FVector2D AnchorPointToAlignment(EWowAnchorPoint Point)
{
	switch (Point)
	{
	case EWowAnchorPoint::TOPLEFT:     return FVector2D(0.0, 0.0);
	case EWowAnchorPoint::TOP:         return FVector2D(0.5, 0.0);
	case EWowAnchorPoint::TOPRIGHT:    return FVector2D(1.0, 0.0);
	case EWowAnchorPoint::LEFT:        return FVector2D(0.0, 0.5);
	case EWowAnchorPoint::CENTER:      return FVector2D(0.5, 0.5);
	case EWowAnchorPoint::RIGHT:       return FVector2D(1.0, 0.5);
	case EWowAnchorPoint::BOTTOMLEFT:  return FVector2D(0.0, 1.0);
	case EWowAnchorPoint::BOTTOM:      return FVector2D(0.5, 1.0);
	case EWowAnchorPoint::BOTTOMRIGHT: return FVector2D(1.0, 1.0);
	default: return FVector2D(0.5, 0.5);
	}
}

void FWowFrameManager::ApplyAnchors(UWidget* Widget, const FWowFrameDef& Def)
{
	if (!Widget) return;

	UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Widget->Slot);
	if (!Slot) return;

	if (Def.bSetAllPoints)
	{
		// Fill parent
		Slot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		Slot->SetOffsets(FMargin(0));
		return;
	}

	if (Def.Anchors.IsEmpty()) return;

	// Use first anchor for positioning
	const FWowAnchor& Anchor = Def.Anchors[0];

	// Map WoW anchor point to UMG anchor
	FVector2D UmgAnchor = AnchorPointToAlignment(Anchor.RelativePoint);
	Slot->SetAnchors(FAnchors(UmgAnchor.X, UmgAnchor.Y));

	// Widget alignment from the point
	FVector2D Alignment = AnchorPointToAlignment(Anchor.Point);
	Slot->SetAlignment(Alignment);

	// Position with offsets
	Slot->SetPosition(FVector2D(Anchor.OffsetX, Anchor.OffsetY));

	// Size
	if (Def.Width > 0.f && Def.Height > 0.f)
	{
		Slot->SetSize(FVector2D(Def.Width, Def.Height));
	}
	else
	{
		Slot->SetAutoSize(true);
	}

	// If two anchors, use them to define stretch
	if (Def.Anchors.Num() >= 2)
	{
		const FWowAnchor& A2 = Def.Anchors[1];
		FVector2D SecondAnchor = AnchorPointToAlignment(A2.RelativePoint);

		// Two-anchor stretch
		Slot->SetAnchors(FAnchors(UmgAnchor.X, UmgAnchor.Y, SecondAnchor.X, SecondAnchor.Y));
		Slot->SetOffsets(FMargin(
			Anchor.OffsetX, Anchor.OffsetY,
			-A2.OffsetX, -A2.OffsetY
		));
	}
}

// ── Layer Content (Textures & FontStrings) ───────────────────────────────────

void FWowFrameManager::ApplyElementAnchors(UWidget* Widget, UCanvasPanel* Parent,
	const TArray<FWowAnchor>& Anchors, float Width, float Height)
{
	UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Widget->Slot);
	if (!Slot) return;

	if (Anchors.IsEmpty())
	{
		// Default: fill parent
		Slot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		Slot->SetOffsets(FMargin(0));
		return;
	}

	const FWowAnchor& Anchor = Anchors[0];
	FVector2D UmgAnchor = AnchorPointToAlignment(Anchor.RelativePoint);
	FVector2D Alignment = AnchorPointToAlignment(Anchor.Point);

	Slot->SetAnchors(FAnchors(UmgAnchor.X, UmgAnchor.Y));
	Slot->SetAlignment(Alignment);
	Slot->SetPosition(FVector2D(Anchor.OffsetX, Anchor.OffsetY));

	if (Width > 0.f && Height > 0.f)
	{
		Slot->SetSize(FVector2D(Width, Height));
	}
	else
	{
		Slot->SetAutoSize(true);
	}

	if (Anchors.Num() >= 2)
	{
		const FWowAnchor& A2 = Anchors[1];
		FVector2D SecondAnchor = AnchorPointToAlignment(A2.RelativePoint);
		Slot->SetAnchors(FAnchors(UmgAnchor.X, UmgAnchor.Y, SecondAnchor.X, SecondAnchor.Y));
		Slot->SetOffsets(FMargin(Anchor.OffsetX, Anchor.OffsetY, -A2.OffsetX, -A2.OffsetY));
	}
}

void FWowFrameManager::CreateLayerContent(UCanvasPanel* Container, const FWowFrameDef& Def)
{
	if (!Container) return;

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
			ImgWidget->SetColorAndOpacity(Tex.VertexColor);

			UCanvasPanelSlot* Slot = Container->AddChildToCanvas(ImgWidget);
			if (Slot)
			{
				Slot->SetZOrder(ZOrder++);
			}

			ApplyElementAnchors(ImgWidget, Container, Tex.Anchors, Tex.Width, Tex.Height);

			// Register the texture element as a named sub-object
			if (!Tex.Name.IsEmpty())
			{
				// Store texture widget for Lua access — texture names are frame-scoped
				UE_LOG(LogWowFrame, Verbose, TEXT("  Layer texture: %s (file: %s)"), *Tex.Name, *Tex.File);
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

			// Apply font size
			FSlateFontInfo FontInfo = TextWidget->GetFont();
			FontInfo.Size = static_cast<int32>(FS.FontHeight);
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

			ApplyElementAnchors(TextWidget, Container, FS.Anchors, FS.Width, FS.Height);

			if (!FS.Name.IsEmpty())
			{
				UE_LOG(LogWowFrame, Verbose, TEXT("  Layer fontstring: %s (text: %s)"), *FS.Name, *FS.Text);
			}
		}
	}
}

// ── Widget Creation ──────────────────────────────────────────────────────────

UWidget* FWowFrameManager::CreateWidgetForFrame(const FWowFrameDef& Def)
{
	UCanvasPanel* Canvas = RootCanvas.Get();
	if (!Canvas)
	{
		UE_LOG(LogWowFrame, Warning, TEXT("No root canvas for frame creation"));
		return nullptr;
	}

	UObject* Outer = Canvas->GetOuter();
	if (!Outer) Outer = GetTransientPackage();

	UWidget* Widget = nullptr;

	switch (Def.Type)
	{
	case EWowFrameType::Button:
	case EWowFrameType::CheckButton:
	{
		UButton* Btn = NewObject<UButton>(Outer);
		Widget = Btn;
		break;
	}
	case EWowFrameType::EditBox:
	{
		UEditableTextBox* Edit = NewObject<UEditableTextBox>(Outer);
		Widget = Edit;
		break;
	}
	case EWowFrameType::StatusBar:
	{
		UProgressBar* Bar = NewObject<UProgressBar>(Outer);
		Widget = Bar;
		break;
	}
	case EWowFrameType::Slider:
	{
		USlider* Sldr = NewObject<USlider>(Outer);
		Widget = Sldr;
		break;
	}
	default:
	{
		// Frame and other types: use a canvas panel as a generic container
		UCanvasPanel* Container = NewObject<UCanvasPanel>(Outer);
		Widget = Container;
		break;
	}
	}

	if (!Widget)
	{
		return nullptr;
	}

	// Set visibility
	if (Def.bHidden)
	{
		Widget->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Add to root canvas (all top-level frames go to root)
	UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget);

	// Apply strata z-ordering (1000 per strata level + frame level offset)
	if (Slot)
	{
		int32 ZOrder = static_cast<int32>(Def.Strata) * 1000 + Def.FrameLevel;
		Slot->SetZOrder(ZOrder);
	}

	// Apply anchor positioning
	ApplyAnchors(Widget, Def);

	// Create layer content (textures and fontstrings) inside the frame
	if (UCanvasPanel* Container = Cast<UCanvasPanel>(Widget))
	{
		CreateLayerContent(Container, Def);
	}

	return Widget;
}

// ── Frame Creation ───────────────────────────────────────────────────────────

int64 FWowFrameManager::CreateFrame(const FWowFrameDef& Def)
{
	// Skip virtual frames - register as templates only
	if (Def.bVirtual)
	{
		if (!Def.Name.IsEmpty())
		{
			RegisterTemplate(Def.Name, Def);
		}
		return -1;
	}

	// Resolve template inheritance
	FWowFrameDef Resolved = ResolveInherits(Def);

	int64 Handle = NextHandle++;

	FFrameEntry Entry;
	Entry.Def = Resolved;

	// Resolve parent handle
	if (!Resolved.Parent.IsEmpty())
	{
		Entry.ParentHandle = FindFrame(Resolved.Parent);
	}

	// Create the widget
	Entry.Widget = CreateWidgetForFrame(Resolved);

	Frames.Add(Handle, MoveTemp(Entry));

	if (!Resolved.Name.IsEmpty())
	{
		NameToHandle.Add(Resolved.Name, Handle);
	}

	// Compile script handlers and create Lua frame object
	if (EventSystem && Resolved.Scripts.Num() > 0)
	{
		EventSystem->CompileFrameScripts(Handle, Resolved);
	}
	else if (EventSystem && !Resolved.Name.IsEmpty())
	{
		// Even without scripts, create the Lua frame object for named frames
		EventSystem->CreateFrameObject(Handle, Resolved.Name);
	}

	// Create child frames recursively
	for (const FWowFrameDef& ChildDef : Resolved.Children)
	{
		CreateFrame(ChildDef);
	}

	UE_LOG(LogWowFrame, Verbose, TEXT("Created frame [%lld] %s (type %d, %d layers, %d children)"),
		Handle, *Resolved.Name, (int32)Resolved.Type, Resolved.Layers.Num(), Resolved.Children.Num());
	return Handle;
}
