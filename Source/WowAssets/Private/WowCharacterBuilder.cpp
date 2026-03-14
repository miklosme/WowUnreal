#include "WowCharacterBuilder.h"
#include "WowSkeletalMeshBuilder.h"
#include "WowAssetCache.h"
#include "WowTextureFactory.h"
#include "Mpq/MpqManager.h"
#include "Formats/M2Parser.h"
#include "Formats/M2Types.h"
#include "Formats/Dbc/DbcStore.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowCharacter, Log, All);

// Parsed M2 cache (shared with doodad manager via static)
static TMap<FString, TSharedPtr<FM2Data>> CharM2Cache;
static FCriticalSection CharCacheLock;
static TMap<FString, TWeakObjectPtr<USkeleton>> CharSkeletonCache;
static TMap<FString, TArray<TWeakObjectPtr<UAnimSequence>>> CharAnimCache;

FString FWowCharacterBuilder::GetCharacterModelPath(ERace Race, EGender Gender)
{
    // Look up ChrRaces.dbc for the ClientFileString
    const FDbcStore& Dbc = FDbcStore::Get();
    uint32 RaceId = static_cast<uint32>(Race);
    const FChrRacesDbcEntry* RaceEntry = Dbc.ChrRaces().GetById(RaceId);

    if (!RaceEntry)
    {
        UE_LOG(LogWowCharacter, Warning, TEXT("Unknown race ID %d"), RaceId);
        return FString();
    }

    // WoW 3.3.5 character model path convention:
    // Character\{ClientFileString}\{Gender}\{ClientFileString}{Gender}.m2
    FString RaceStr = RaceEntry->ClientFileString;
    FString GenderStr = (Gender == EGender::Male) ? TEXT("Male") : TEXT("Female");

    return FString::Printf(TEXT("Character\\%s\\%s\\%s%s.m2"), *RaceStr, *GenderStr, *RaceStr, *GenderStr);
}

AActor* FWowCharacterBuilder::SpawnCharacter(UWorld* World, FMpqManager* Mpq, FWowAssetCache* Cache,
    ERace Race, EGender Gender, const FVector& Location, const FRotator& Rotation)
{
    FString ModelPath = GetCharacterModelPath(Race, Gender);
    if (ModelPath.IsEmpty())
    {
        return nullptr;
    }

    UE_LOG(LogWowCharacter, Log, TEXT("Spawning character: Race=%d Gender=%d Model=%s"),
        static_cast<int32>(Race), static_cast<int32>(Gender), *ModelPath);

    // Character models are in WoW units — scale to UE (100x)
    return SpawnM2Actor(World, Mpq, Cache, ModelPath, Location, Rotation, 100.0f);
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
    if (ModelPath.IsEmpty())
    {
        return nullptr;
    }

    // Replace .mdx with .m2 if needed
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
    const FString& ModelPath, const FVector& Location, const FRotator& Rotation, float Scale)
{
    if (!World || !Mpq) return nullptr;

    FString NormPath = ModelPath.ToLower();
    NormPath.ReplaceInline(TEXT("/"), TEXT("\\"));

    // Check skel mesh cache first
    USkeletalMesh* SkelMesh = Cache ? Cache->FindSkelMesh(NormPath) : nullptr;
    USkeleton* Skeleton = nullptr;

    if (!SkelMesh)
    {
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

            // Skin file
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

        // Build skeleton
        {
            FScopeLock Lock(&CharCacheLock);
            auto* CachedSkel = CharSkeletonCache.Find(NormPath);
            if (CachedSkel && CachedSkel->IsValid())
                Skeleton = CachedSkel->Get();
        }

        if (!Skeleton)
        {
            Skeleton = FWowSkeletalMeshBuilder::CreateSkeleton(*M2Data, ModelPath);
            if (!Skeleton)
            {
                UE_LOG(LogWowCharacter, Warning, TEXT("Failed to create skeleton: %s"), *ModelPath);
                return nullptr;
            }
            Skeleton->AddToRoot();

            FScopeLock Lock(&CharCacheLock);
            CharSkeletonCache.Add(NormPath, Skeleton);
        }

        // Build skeletal mesh
        SkelMesh = FWowSkeletalMeshBuilder::CreateSkeletalMesh(*M2Data, Skeleton, ModelPath, Mpq, Cache);
        if (!SkelMesh)
        {
            UE_LOG(LogWowCharacter, Warning, TEXT("Failed to create skeletal mesh: %s"), *ModelPath);
            return nullptr;
        }
        SkelMesh->AddToRoot();

        if (Cache) Cache->CacheSkelMesh(NormPath, SkelMesh);

        // Build animations
        TArray<UAnimSequence*> Anims = FWowSkeletalMeshBuilder::CreateAnimations(*M2Data, Skeleton, ModelPath);
        {
            FScopeLock Lock(&CharCacheLock);
            TArray<TWeakObjectPtr<UAnimSequence>> WeakAnims;
            for (UAnimSequence* A : Anims)
            {
                if (A)
                {
                    A->AddToRoot();
                    WeakAnims.Add(A);
                }
            }
            CharAnimCache.Add(NormPath, MoveTemp(WeakAnims));
        }

        UE_LOG(LogWowCharacter, Log, TEXT("Built skeletal mesh for %s: %d verts, %d bones, %d anims, %d attachments"),
            *ModelPath, M2Data->Vertices.Num(), M2Data->Bones.Num(),
            Anims.Num(), M2Data->Attachments.Num());
    }

    // Spawn actor
    AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Rotation, Location));
    if (!Actor) return nullptr;

    USceneComponent* Root = NewObject<USceneComponent>(Actor, TEXT("Root"));
    Actor->SetRootComponent(Root);
    Root->RegisterComponent();

    USkeletalMeshComponent* MeshComp = NewObject<USkeletalMeshComponent>(Actor, TEXT("CharacterMesh"));
    MeshComp->SetSkeletalMesh(SkelMesh);
    MeshComp->SetupAttachment(Root);
    MeshComp->SetWorldScale3D(FVector(Scale));
    MeshComp->SetCastShadow(true);
    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComp->RegisterComponent();

    // Play idle animation if available
    {
        FScopeLock Lock(&CharCacheLock);
        auto* Anims = CharAnimCache.Find(NormPath);
        if (Anims && Anims->Num() > 0 && (*Anims)[0].IsValid())
        {
            MeshComp->PlayAnimation((*Anims)[0].Get(), true);
            UE_LOG(LogWowCharacter, Log, TEXT("Playing animation on %s"), *ModelPath);
        }
    }

    UE_LOG(LogWowCharacter, Log, TEXT("Spawned M2 actor: %s at %s (scale=%.1f)"),
        *ModelPath, *Location.ToString(), Scale);

    return Actor;
}
