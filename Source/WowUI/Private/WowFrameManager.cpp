#include "WowFrameManager.h"
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

void FWowFrameManager::SetFrameVisible(int64 Handle, bool bVisible)
{
	FFrameEntry* Entry = Frames.Find(Handle);
	if (!Entry) return;

	if (Entry->Widget.IsValid())
	{
		Entry->Widget->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

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
		if (Def.MinValue != 0.f || Def.MaxValue != 1.f)
		{
			// USlider is 0-1, store range for mapping later
		}
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

	// Add to root canvas
	UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget);
	if (Slot)
	{
		if (Def.Width > 0.f && Def.Height > 0.f)
		{
			Slot->SetSize(FVector2D(Def.Width, Def.Height));
		}
		else
		{
			Slot->SetAutoSize(true);
		}
	}

	return Widget;
}

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

	int64 Handle = NextHandle++;

	FFrameEntry Entry;
	Entry.Def = Def;

	// Resolve parent handle
	if (!Def.Parent.IsEmpty())
	{
		Entry.ParentHandle = FindFrame(Def.Parent);
	}

	// Create the widget
	Entry.Widget = CreateWidgetForFrame(Def);

	Frames.Add(Handle, MoveTemp(Entry));

	if (!Def.Name.IsEmpty())
	{
		NameToHandle.Add(Def.Name, Handle);
	}

	// Create child frames recursively
	for (const FWowFrameDef& ChildDef : Def.Children)
	{
		CreateFrame(ChildDef);
	}

	UE_LOG(LogWowFrame, Verbose, TEXT("Created frame [%lld] %s (type %d)"), Handle, *Def.Name, (int32)Def.Type);
	return Handle;
}
