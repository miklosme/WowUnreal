#include "WowCharacterBuilder.h"
#include "WowSkeletalMeshBuilder.h"
#include "WowCharacterTexture.h"
#include "WowEquipmentManager.h"
#include "WowAssetCache.h"
#include "WowTextureFactory.h"
#include "Mpq/MpqManager.h"
#include "Formats/M2Parser.h"
#include "Formats/M2Types.h"
#include "Formats/BlpParser.h"
#include "Formats/BlpTypes.h"
#include "Formats/Dbc/DbcStore.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "Coord/WowCoordinate.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowCharacter, Log, All);

// Parsed M2 cache (shared with doodad manager via static)
static TMap<FString, TSharedPtr<FM2Data>> CharM2Cache;
static FCriticalSection CharCacheLock;
static TMap<FString, TWeakObjectPtr<USkeleton>> CharSkeletonCache;
static TMap<FString, TArray<TWeakObjectPtr<UAnimSequence>>> CharAnimCache;

FString FWowCharacterBuilder::GetCharacterModelPath(ERace Race, EGender Gender)
{
    const FDbcStore& Dbc = FDbcStore::Get();
    uint32 RaceId = static_cast<uint32>(Race);
    const FChrRacesDbcEntry* RaceEntry = Dbc.ChrRaces().GetById(RaceId);

    if (!RaceEntry)
    {
        UE_LOG(LogWowCharacter, Warning, TEXT("Unknown race ID %d"), RaceId);
        return FString();
    }

    FString RaceStr = RaceEntry->ClientFileString;
    FString GenderStr = (Gender == EGender::Male) ? TEXT("Male") : TEXT("Female");

    return FString::Printf(TEXT("Character\\%s\\%s\\%s%s.m2"), *RaceStr, *GenderStr, *RaceStr, *GenderStr);
}

AActor* FWowCharacterBuilder::SpawnCharacter(UWorld* World, FMpqManager* Mpq, FWowAssetCache* Cache,
    ERace Race, EGender Gender, const FVector& Location, const FRotator& Rotation)
{
    FString ModelPath = GetCharacterModelPath(Race, Gender);
    if (ModelPath.IsEmpty()) return nullptr;

    UE_LOG(LogWowCharacter, Log, TEXT("Spawning character: Race=%d Gender=%d Model=%s"),
        static_cast<int32>(Race), static_cast<int32>(Gender), *ModelPath);

    // Build composite texture with default customization
    FWowCharacterTexture::FCustomization Cust;
    Cust.RaceId = static_cast<uint32>(Race);
    Cust.Gender = static_cast<uint32>(Gender);
    UTexture2D* CompositeTex = FWowCharacterTexture::BuildCompositeTexture(Mpq, Cache, Cust);

    return SpawnM2Actor(World, Mpq, Cache, ModelPath, Location, Rotation, 100.0f, CompositeTex);
}

AActor* FWowCharacterBuilder::SpawnCharacterWithEquipment(UWorld* World, FMpqManager* Mpq, FWowAssetCache* Cache,
    const FCharacterParams& Params, const FVector& Location, const FRotator& Rotation)
{
    FString ModelPath = GetCharacterModelPath(Params.Race, Params.Gender);
    if (ModelPath.IsEmpty()) return nullptr;

    UE_LOG(LogWowCharacter, Log, TEXT("Spawning equipped character: Race=%d Gender=%d Equipment=%d slots"),
        static_cast<int32>(Params.Race), static_cast<int32>(Params.Gender), Params.Equipment.Num());

    // Build composite texture with full customization
    UTexture2D* CompositeTex = FWowCharacterTexture::BuildCompositeTexture(Mpq, Cache, Params.Customization);

    // Spawn base character
    AActor* Actor = SpawnM2Actor(World, Mpq, Cache, ModelPath, Location, Rotation, 100.0f, CompositeTex);
    if (!Actor) return nullptr;

    // Attach equipment
    if (Params.Equipment.Num() > 0)
    {
        USkeletalMeshComponent* MeshComp = Actor->FindComponentByClass<USkeletalMeshComponent>();
        if (MeshComp)
        {
            // Get the character's M2 data for attachment point bone lookup
            FString NormPath = ModelPath.ToLower();
            NormPath.ReplaceInline(TEXT("/"), TEXT("\\"));

            TSharedPtr<FM2Data> CharM2;
            {
                FScopeLock Lock(&CharCacheLock);
                TSharedPtr<FM2Data>* Found = CharM2Cache.Find(NormPath);
                if (Found) CharM2 = *Found;
            }

            if (CharM2)
            {
                for (const FEquipmentSlot& Slot : Params.Equipment)
                {
                    if (Slot.ItemDisplayId == 0) continue;

                    USceneComponent* EquipComp = FWowEquipmentManager::AttachEquipment(
                        Mpq, Cache, Slot.ItemDisplayId, Slot.AttachPoint, MeshComp, *CharM2);

                    if (EquipComp)
                    {
                        UE_LOG(LogWowCharacter, Log, TEXT("Equipped item DisplayID=%d at attachment %d"),
                            Slot.ItemDisplayId, static_cast<uint32>(Slot.AttachPoint));
                    }
                }
            }
            else
            {
                UE_LOG(LogWowCharacter, Warning, TEXT("No cached M2 data for equipment attachment: %s"), *NormPath);
            }
        }
    }

    return Actor;
}

AActor* FWowCharacterBuilder::SpawnCreatureByDisplayId(UWorld* World, FMpqManager* Mpq, FWowAssetCache* Cache,
    uint32 DisplayId, const FVector& Location, const FRotator& Rotation)
{
    const FDbcStore& Dbc = FDbcStore::Get();
    const FCreatureDisplayInfoDbcEntry* DisplayInfo = Dbc.CreatureDisplayInfo().GetById(DisplayId);
    if (!DisplayInfo)
    {
        UE_LOG(LogWowCharacter, Warning, TEXT("Unknown CreatureDisplayInfo ID %d"), DisplayId);
        return nullptr;
    }

    const FCreatureModelDataDbcEntry* ModelData = Dbc.CreatureModelData().GetById(DisplayInfo->ModelID);
    if (!ModelData)
    {
        UE_LOG(LogWowCharacter, Warning, TEXT("Unknown CreatureModelData ID %d for display %d"),
            DisplayInfo->ModelID, DisplayId);
        return nullptr;
    }

    FString ModelPath = ModelData->ModelPath;
    if (ModelPath.IsEmpty()) return nullptr;

    if (ModelPath.EndsWith(TEXT(".mdx"), ESearchCase::IgnoreCase))
    {
        ModelPath = ModelPath.Left(ModelPath.Len() - 4) + TEXT(".m2");
    }

    float Scale = DisplayInfo->Scale * ModelData->Scale * 100.0f;
    UE_LOG(LogWowCharacter, Log, TEXT("Spawning creature: DisplayID=%d Model=%s Scale=%.2f"),
        DisplayId, *ModelPath, Scale);

    return SpawnM2Actor(World, Mpq, Cache, ModelPath, Location, Rotation, Scale);
}

AActor* FWowCharacterBuilder::SpawnM2Actor(UWorld* World, FMpqManager* Mpq, FWowAssetCache* Cache,
    const FString& ModelPath, const FVector& Location, const FRotator& Rotation, float Scale,
    UTexture2D* OverrideTexture)
{
    if (!World || !Mpq) return nullptr;

    FString NormPath = ModelPath.ToLower();
    NormPath.ReplaceInline(TEXT("/"), TEXT("\\"));

    // Parse M2
    TSharedPtr<FM2Data> M2Data;
    {
        FScopeLock Lock(&CharCacheLock);
        TSharedPtr<FM2Data>* Found = CharM2Cache.Find(NormPath);
        if (Found) M2Data = *Found;
    }

    if (!M2Data)
    {
        TArray<uint8> M2Raw;
        if (!Mpq->ReadFile(ModelPath, M2Raw))
        {
            UE_LOG(LogWowCharacter, Warning, TEXT("Failed to read M2: %s"), *ModelPath);
            return nullptr;
        }

        FString SkinPath = ModelPath;
        if (SkinPath.EndsWith(TEXT(".m2"), ESearchCase::IgnoreCase))
            SkinPath = SkinPath.Left(SkinPath.Len() - 3);
        SkinPath += TEXT("00.skin");

        TArray<uint8> SkinRaw;
        Mpq->ReadFile(SkinPath, SkinRaw);

        M2Data = MakeShared<FM2Data>(FM2Parser::Parse(M2Raw, SkinRaw));
        if (!M2Data->bIsValid)
        {
            UE_LOG(LogWowCharacter, Warning, TEXT("Failed to parse M2: %s"), *ModelPath);
            return nullptr;
        }

        FScopeLock Lock(&CharCacheLock);
        CharM2Cache.Add(NormPath, M2Data);
    }

    if (M2Data->Vertices.Num() == 0 || M2Data->Indices.Num() == 0)
    {
        UE_LOG(LogWowCharacter, Warning, TEXT("M2 has no geometry: %s"), *ModelPath);
        return nullptr;
    }

    // Build skeleton (cached for future skeletal animation support)
    {
        FScopeLock Lock(&CharCacheLock);
        auto* CachedSkel = CharSkeletonCache.Find(NormPath);
        if (!CachedSkel || !CachedSkel->IsValid())
        {
            USkeleton* Skeleton = FWowSkeletalMeshBuilder::CreateSkeleton(*M2Data, ModelPath);
            if (Skeleton)
            {
                Skeleton->AddToRoot();
                CharSkeletonCache.Add(NormPath, Skeleton);
            }
        }
    }

    // Spawn actor with ProceduralMeshComponent (avoids skeletal mesh render pipeline issues)
    AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Rotation, Location));
    if (!Actor) return nullptr;

    USceneComponent* Root = NewObject<USceneComponent>(Actor, TEXT("Root"));
    Actor->SetRootComponent(Root);
    Root->RegisterComponent();

    UProceduralMeshComponent* ProcMesh = NewObject<UProceduralMeshComponent>(Actor, TEXT("CharacterMesh"));
    ProcMesh->SetupAttachment(Root);
    ProcMesh->SetWorldScale3D(FVector(Scale));
    ProcMesh->SetCastShadow(true);
    ProcMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // Build mesh section from M2 vertex data
    const int32 NumVerts = M2Data->Vertices.Num();
    const int32 NumIndices = M2Data->Indices.Num();

    TArray<FVector> Vertices;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FColor> Colors;
    TArray<int32> Triangles;

    Vertices.SetNum(NumVerts);
    Normals.SetNum(NumVerts);
    UVs.SetNum(NumVerts);
    Colors.SetNum(NumVerts);

    for (int32 i = 0; i < NumVerts; ++i)
    {
        const FM2Vertex& V = M2Data->Vertices[i];
        // Positions in WoW units (yards), component world scale handles UE conversion
        Vertices[i] = FVector(V.Position.Y, V.Position.X, V.Position.Z);
        Normals[i] = FVector(V.Normal.Y, V.Normal.X, V.Normal.Z).GetSafeNormal();
        UVs[i] = FVector2D(V.TexCoord.X, V.TexCoord.Y);
        Colors[i] = FColor::White;
    }

    Triangles.SetNum(NumIndices);
    for (int32 i = 0; i < NumIndices; ++i)
    {
        Triangles[i] = M2Data->Indices[i];
    }

    ProcMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, Colors, TArray<FProcMeshTangent>(), false);

    // Load and apply texture
    UTexture2D* TexToApply = OverrideTexture;
    if (!TexToApply && M2Data->TexturePaths.Num() > 0 && !M2Data->TexturePaths[0].IsEmpty())
    {
        const FString& TexPath = M2Data->TexturePaths[0];
        TexToApply = Cache ? Cache->FindTexture(TexPath) : nullptr;
        if (!TexToApply)
        {
            TArray<uint8> BlpRaw;
            if (Mpq->ReadFile(TexPath, BlpRaw))
            {
                FBlpTexture BlpData = FBlpParser::Parse(BlpRaw);
                if (BlpData.bIsValid)
                {
                    TexToApply = FWowTextureFactory::CreateTexture(BlpData, TexPath);
                    if (TexToApply && Cache) Cache->CacheTexture(TexPath, TexToApply);
                }
            }
        }
    }

    if (TexToApply)
    {
        UMaterial* BaseMat = UMaterial::GetDefaultMaterial(MD_Surface);
        if (BaseMat)
        {
            UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, ProcMesh);
            MID->SetTextureParameterValue(FName(TEXT("BaseColor")), TexToApply);
            ProcMesh->SetMaterial(0, MID);
            UE_LOG(LogWowCharacter, Log, TEXT("Applied texture to character: %s"), *ModelPath);
        }
    }

    ProcMesh->RegisterComponent();

    UE_LOG(LogWowCharacter, Log, TEXT("Spawned M2 actor: %s at %s (scale=%.1f, %d verts, %d tris)"),
        *ModelPath, *Location.ToString(), Scale, NumVerts, NumIndices / 3);

    return Actor;
}
