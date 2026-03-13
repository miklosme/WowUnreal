#include "WowTerrainTile.h"
#include "Formats/AdtTypes.h"
#include "Coord/WowCoordinate.h"

AWowTerrainTile::AWowTerrainTile()
{
    PrimaryActorTick.bCanEverTick = false;
    RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(RootScene);
}

void AWowTerrainTile::BuildFromAdtData(const FAdtData& Data, int32 TX, int32 TY, FMpqManager* Mpq, FWowAssetCache* Cache)
{
    TileCoord = FIntPoint(TX, TY);
    if (!Data.bIsValid) return;
    SetActorLocation(FWowCoordinate::TileToWorld(TX, TY));
    UE_LOG(LogTemp, Log, TEXT("Tile %d,%d: %d textures, %d doodads, %d WMOs"), TX, TY, Data.TexturePaths.Num(), Data.DoodadPlacements.Num(), Data.WmoPlacements.Num());
}
