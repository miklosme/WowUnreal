#include "SWowMinimap.h"
#include "WowEntityManager.h"
#include "WowEntity.h"
#include "WowWorldManager.h"
#include "WowUpdateFields.h"
#include "Coord/WowCoordinate.h"
#include "Formats/Dbc/DbcStore.h"
#include "Formats/Dbc/AreaTableDbc.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Fonts/FontMeasure.h"
#include "Styling/CoreStyle.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowMinimap, Log, All);

void SWowMinimap::Construct(const FArguments& InArgs)
{
    EntityManager = InArgs._EntityManager;
    WorldManager = InArgs._WorldManager;

    ChildSlot
    [
        SNew(SBox)
        .WidthOverride(MinimapSize + 40.0f) // Extra space for border and zone name
        .HeightOverride(MinimapSize + 60.0f) // Extra space for zone name below
        [
            SNew(SVerticalBox)

            // Minimap circle area
            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Center)
            [
                SNew(SBox)
                .WidthOverride(MinimapSize + 20.0f)
                .HeightOverride(MinimapSize + 20.0f)
                [
                    SNullWidget::NullWidget // The actual minimap is drawn via OnPaint
                ]
            ]

            // Zone name
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 5.0f, 0.0f, 0.0f)
            .HAlign(HAlign_Center)
            [
                SAssignNew(ZoneNameText, STextBlock)
                .Text(FText::FromString(TEXT("Unknown Zone")))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
                .ColorAndOpacity(FLinearColor::White)
                .Justification(ETextJustify::Center)
            ]
        ]
    ];
}

void SWowMinimap::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

    // Update zone name periodically
    if (InCurrentTime - LastUpdateTime >= UpdateInterval)
    {
        LastUpdateTime = InCurrentTime;

        // Update zone name
        FString ZoneName = GetCurrentZoneName();
        if (ZoneNameText.IsValid())
        {
            ZoneNameText->SetText(FText::FromString(ZoneName));
        }
    }
}

void SWowMinimap::SetMinimapVisibility(EVisibility InVisibility)
{
    SetVisibility(InVisibility);
}

FReply SWowMinimap::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
    // Check if mouse is over the minimap circle
    FVector2D LocalPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
    FVector2D Center = FVector2D(MinimapSize * 0.5f + 10.0f, MinimapSize * 0.5f + 10.0f);
    float Radius = MinimapSize * 0.5f;

    if (IsWithinMinimapCircle(Center, Radius, LocalPos))
    {
        float WheelDelta = MouseEvent.GetWheelDelta();
        float ZoomMultiplier = WheelDelta > 0 ? 0.8f : 1.25f; // Zoom in/out

        ZoomYards = FMath::Clamp(ZoomYards * ZoomMultiplier, MinZoomYards, MaxZoomYards);

        UE_LOG(LogWowMinimap, Log, TEXT("Minimap zoom changed to %f yards"), ZoomYards);
        return FReply::Handled();
    }

    return FReply::Unhandled();
}

void SWowMinimap::ToggleFullScreenMap()
{
    bIsFullScreen = !bIsFullScreen;
    // For now, just show a larger version of the same minimap
    // TODO: Implement proper full-screen world map
    UE_LOG(LogWowMinimap, Log, TEXT("Full-screen map toggled: %s"), bIsFullScreen ? TEXT("ON") : TEXT("OFF"));
}

int32 SWowMinimap::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
                          const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
                          int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    // Calculate minimap center and radius
    FVector2D Center = FVector2D(MinimapSize * 0.5f + 10.0f, MinimapSize * 0.5f + 10.0f);
    float Radius = MinimapSize * 0.5f;

    // Paint background
    PaintMinimapBackground(AllottedGeometry, OutDrawElements, LayerId, Center, Radius);

    // Paint entity dots
    PaintEntityDots(AllottedGeometry, OutDrawElements, LayerId + 1, Center, Radius);

    // Paint player arrow (always on top)
    PaintPlayerArrow(AllottedGeometry, OutDrawElements, LayerId + 2, Center);

    // Paint border (always on top)
    PaintCircularBorder(AllottedGeometry, OutDrawElements, LayerId + 3, Center, Radius);

    return LayerId + 4;
}

void SWowMinimap::PaintMinimapBackground(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements,
                                        int32 LayerId, const FVector2D& Center, float Radius) const
{
    // Paint dark green/brown circular background
    FLinearColor BackgroundColor = FLinearColor(0.2f, 0.3f, 0.1f, 0.9f); // Dark green

    // Create circle geometry
    TArray<FSlateVertex> Vertices;
    TArray<SlateIndex> Indices;

    const int32 NumSegments = 32;
    const float AngleStep = 2.0f * PI / NumSegments;

    // Center vertex
    Vertices.Add(FSlateVertex::Make(FSlateRenderTransform(), Center, FVector2D::ZeroVector, BackgroundColor));

    // Circle vertices
    for (int32 i = 0; i <= NumSegments; ++i)
    {
        float Angle = i * AngleStep;
        FVector2D Pos = Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
        Vertices.Add(FSlateVertex::Make(FSlateRenderTransform(), Pos, FVector2D::ZeroVector, BackgroundColor));
    }

    // Triangle indices for filled circle
    for (int32 i = 0; i < NumSegments; ++i)
    {
        Indices.Add(0); // Center
        Indices.Add(i + 1);
        Indices.Add(i + 2);
    }

    FSlateDrawElement::MakeCustomVerts(
        OutDrawElements,
        LayerId,
        FSlateResourceHandle(),
        Vertices,
        Indices,
        nullptr,
        0,
        0,
        ESlateDrawEffect::None
    );
}

void SWowMinimap::PaintEntityDots(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements,
                                 int32 LayerId, const FVector2D& Center, float Radius) const
{
    if (!EntityManager)
    {
        return;
    }

    FVector PlayerPos;
    float PlayerYaw;
    if (!GetPlayerPositionAndRotation(PlayerPos, PlayerYaw))
    {
        return;
    }

    const TMap<uint64, TSharedPtr<FWowEntity>>& AllEntities = EntityManager->GetAll();

    for (const auto& Pair : AllEntities)
    {
        const FWowEntity& Entity = *Pair.Value;

        // Skip local player
        if (Entity.Guid == EntityManager->LocalPlayerGuid)
        {
            continue;
        }

        // Only show units (NPCs and other players)
        if (!Entity.IsUnit())
        {
            continue;
        }

        FVector EntityPos = Entity.Movement.Position;

        // Check if entity is within radius (convert zoom yards to UE units)
        float MaxDistanceUE = ZoomYards * 100.0f; // 1 yard = 100 UE units
        float DistanceUE = FVector::Dist(PlayerPos, EntityPos);
        if (DistanceUE > MaxDistanceUE)
        {
            continue;
        }

        // Convert to minimap coordinates
        FVector2D MinimapPos = WorldToMinimapCoords(EntityPos, PlayerPos, Center, Radius, PlayerYaw);

        // Check if position is within the minimap circle
        if (!IsWithinMinimapCircle(Center, Radius, MinimapPos))
        {
            continue;
        }

        // Get entity color
        FLinearColor DotColor = GetEntityColor(Entity);

        // Draw entity dot
        const float DotRadius = 3.0f;
        FSlateDrawElement::MakeBox(
            OutDrawElements,
            LayerId,
            AllottedGeometry.ToPaintGeometry(
                FVector2D(DotRadius * 2, DotRadius * 2),
                FSlateLayoutTransform(MinimapPos - FVector2D(DotRadius, DotRadius))
            ),
            FCoreStyle::Get().GetBrush("WhiteBrush"),
            ESlateDrawEffect::None,
            DotColor
        );
    }
}

void SWowMinimap::PaintPlayerArrow(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements,
                                  int32 LayerId, const FVector2D& Center) const
{
    FVector PlayerPos;
    float PlayerYaw;
    if (!GetPlayerPositionAndRotation(PlayerPos, PlayerYaw))
    {
        return;
    }

    // Draw yellow arrow pointing in player's direction
    FLinearColor ArrowColor = FLinearColor::Yellow;
    const float ArrowSize = 8.0f;

    // Create arrow vertices (triangle pointing up, then rotated)
    TArray<FSlateVertex> Vertices;
    TArray<SlateIndex> Indices;

    // Triangle vertices in local space (pointing up)
    FVector2D ArrowVerts[3] = {
        FVector2D(0, -ArrowSize),     // Top
        FVector2D(-ArrowSize/2, ArrowSize/2), // Bottom left
        FVector2D(ArrowSize/2, ArrowSize/2)   // Bottom right
    };

    // Rotate by player yaw and translate to center
    float CosYaw = FMath::Cos(PlayerYaw);
    float SinYaw = FMath::Sin(PlayerYaw);

    for (int32 i = 0; i < 3; ++i)
    {
        FVector2D Rotated = FVector2D(
            ArrowVerts[i].X * CosYaw - ArrowVerts[i].Y * SinYaw,
            ArrowVerts[i].X * SinYaw + ArrowVerts[i].Y * CosYaw
        );
        FVector2D WorldPos = Center + Rotated;
        Vertices.Add(FSlateVertex::Make(FSlateRenderTransform(), WorldPos, FVector2D::ZeroVector, ArrowColor));
    }

    // Triangle indices
    Indices.Add(0);
    Indices.Add(1);
    Indices.Add(2);

    FSlateDrawElement::MakeCustomVerts(
        OutDrawElements,
        LayerId,
        FSlateResourceHandle(),
        Vertices,
        Indices,
        nullptr,
        0,
        0,
        ESlateDrawEffect::None
    );
}

void SWowMinimap::PaintCircularBorder(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements,
                                     int32 LayerId, const FVector2D& Center, float Radius) const
{
    // Paint gold circular border
    FLinearColor BorderColor = FLinearColor(1.0f, 0.8f, 0.0f, 1.0f); // Gold
    const float BorderThickness = 3.0f;

    // Create circle outline
    TArray<FSlateVertex> Vertices;
    TArray<SlateIndex> Indices;

    const int32 NumSegments = 64;
    const float AngleStep = 2.0f * PI / NumSegments;

    // Outer circle vertices
    for (int32 i = 0; i <= NumSegments; ++i)
    {
        float Angle = i * AngleStep;
        FVector2D OuterPos = Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
        FVector2D InnerPos = Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * (Radius - BorderThickness);

        Vertices.Add(FSlateVertex::Make(FSlateRenderTransform(), OuterPos, FVector2D::ZeroVector, BorderColor));
        Vertices.Add(FSlateVertex::Make(FSlateRenderTransform(), InnerPos, FVector2D::ZeroVector, BorderColor));
    }

    // Create quad strips for border
    for (int32 i = 0; i < NumSegments; ++i)
    {
        int32 Base = i * 2;
        int32 Next = ((i + 1) % (NumSegments + 1)) * 2;

        // First triangle
        Indices.Add(Base);
        Indices.Add(Base + 1);
        Indices.Add(Next);

        // Second triangle
        Indices.Add(Next);
        Indices.Add(Base + 1);
        Indices.Add(Next + 1);
    }

    FSlateDrawElement::MakeCustomVerts(
        OutDrawElements,
        LayerId,
        FSlateResourceHandle(),
        Vertices,
        Indices,
        nullptr,
        0,
        0,
        ESlateDrawEffect::None
    );
}

FVector2D SWowMinimap::WorldToMinimapCoords(const FVector& WorldPos, const FVector& PlayerPos,
                                          const FVector2D& Center, float Radius, float PlayerYaw) const
{
    // Convert world offset to minimap offset
    FVector WorldOffset = WorldPos - PlayerPos;

    // Convert to 2D (UE X = north, Y = east)
    FVector2D Offset2D = FVector2D(WorldOffset.X, WorldOffset.Y);

    // Rotate by negative player yaw so the minimap rotates with player facing
    float CosYaw = FMath::Cos(-PlayerYaw);
    float SinYaw = FMath::Sin(-PlayerYaw);
    FVector2D RotatedOffset = FVector2D(
        Offset2D.X * CosYaw - Offset2D.Y * SinYaw,
        Offset2D.X * SinYaw + Offset2D.Y * CosYaw
    );

    // Scale to minimap radius (convert from UE units to minimap pixels)
    float ZoomRangeUE = ZoomYards * 100.0f; // Zoom range in UE units
    FVector2D ScaledOffset = RotatedOffset * (Radius / ZoomRangeUE);

    return Center + ScaledOffset;
}

FLinearColor SWowMinimap::GetEntityColor(const FWowEntity& Entity) const
{
    if (Entity.IsPlayer())
    {
        // Other players are blue
        return FLinearColor::Blue;
    }
    else if (Entity.IsUnit())
    {
        // Check faction/hostility for NPCs
        uint32 FactionTemplate = Entity.GetField(UnitField::FACTIONTEMPLATE);

        // Simple faction check - this would need proper faction relationship logic
        // For now, just use basic colors
        if (FactionTemplate == 14 || FactionTemplate == 16) // Some hostile factions
        {
            return FLinearColor::Red; // Hostile
        }
        else
        {
            return FLinearColor::Green; // Friendly
        }
    }

    return FLinearColor::White; // Default
}

FString SWowMinimap::GetCurrentZoneName() const
{
    if (!WorldManager.IsValid())
    {
        return TEXT("Unknown Zone");
    }

    // For now, return the map name from WorldManager
    // TODO: This should be enhanced to get the actual current area name from AreaTable.dbc
    // based on the player's current position and MCNK area IDs
    const FString& MapName = WorldManager->GetMapName();
    if (!MapName.IsEmpty())
    {
        return MapName;
    }

    return TEXT("Unknown Zone");
}

bool SWowMinimap::GetPlayerPositionAndRotation(FVector& OutPosition, float& OutYaw) const
{
    if (!EntityManager)
    {
        return false;
    }

    FWowPlayerEntity* LocalPlayer = EntityManager->GetLocalPlayer();
    if (!LocalPlayer)
    {
        return false;
    }

    OutPosition = LocalPlayer->Movement.Position;
    OutYaw = LocalPlayer->Movement.Orientation;
    return true;
}

bool SWowMinimap::IsWithinMinimapCircle(const FVector2D& Center, float Radius, const FVector2D& Point) const
{
    return FVector2D::Distance(Center, Point) <= Radius;
}