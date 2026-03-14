#include "WowAudioManager.h"
#include "Mpq/MpqManager.h"
#include "Formats/Dbc/DbcStore.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWave.h"
#include "Kismet/GameplayStatics.h"
#include "Memory/SharedBuffer.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowAudio, Log, All);

AWowAudioManager::AWowAudioManager()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.05f; // 20Hz for smooth crossfade
}

void AWowAudioManager::BeginPlay()
{
    Super::BeginPlay();

    // Create two music audio components for crossfading (A/B pattern)
    MusicComponentA = NewObject<UAudioComponent>(this, TEXT("MusicA"));
    MusicComponentA->SetupAttachment(GetRootComponent());
    MusicComponentA->bAutoActivate = false;
    MusicComponentA->bIsUISound = true; // Non-positional
    MusicComponentA->RegisterComponent();

    MusicComponentB = NewObject<UAudioComponent>(this, TEXT("MusicB"));
    MusicComponentB->SetupAttachment(GetRootComponent());
    MusicComponentB->bAutoActivate = false;
    MusicComponentB->bIsUISound = true;
    MusicComponentB->RegisterComponent();

    AmbienceComponent = NewObject<UAudioComponent>(this, TEXT("Ambience"));
    AmbienceComponent->SetupAttachment(GetRootComponent());
    AmbienceComponent->bAutoActivate = false;
    AmbienceComponent->bIsUISound = true;
    AmbienceComponent->RegisterComponent();

    UE_LOG(LogWowAudio, Log, TEXT("Audio manager initialized"));
}

void AWowAudioManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Handle crossfade
    if (CrossfadeProgress < 1.0f)
    {
        CrossfadeProgress = FMath::Min(CrossfadeProgress + DeltaTime / CrossfadeDuration, 1.0f);

        float FadeIn = CrossfadeProgress;
        float FadeOut = 1.0f - CrossfadeProgress;

        UAudioComponent* ActiveComp = bMusicAActive ? MusicComponentA : MusicComponentB;
        UAudioComponent* FadingComp = bMusicAActive ? MusicComponentB : MusicComponentA;

        if (ActiveComp) ActiveComp->SetVolumeMultiplier(FadeIn * MusicVolume * MasterVolume);
        if (FadingComp) FadingComp->SetVolumeMultiplier(FadeOut * MusicVolume * MasterVolume);

        if (CrossfadeProgress >= 1.0f && FadingComp)
        {
            FadingComp->Stop();
        }
    }
}

void AWowAudioManager::SetCurrentZone(uint32 ZoneId, uint32 AreaId)
{
    if (ZoneId == CurrentZoneId && AreaId == CurrentAreaId)
    {
        return;
    }

    CurrentZoneId = ZoneId;
    CurrentAreaId = AreaId;

    // Look up zone music from AreaTable.dbc
    const FAreaTableDbcEntry* AreaEntry = FDbcStore::Get().AreaTable().GetById(AreaId);
    if (!AreaEntry)
    {
        AreaEntry = FDbcStore::Get().AreaTable().GetById(ZoneId);
    }

    if (AreaEntry && AreaEntry->ZoneMusicID != 0)
    {
        if (AreaEntry->ZoneMusicID != PendingZoneMusicId)
        {
            PendingZoneMusicId = AreaEntry->ZoneMusicID;
            UpdateMusic();
        }
    }

    UE_LOG(LogWowAudio, Log, TEXT("Zone changed: zone=%d area=%d musicId=%d"),
        ZoneId, AreaId, PendingZoneMusicId);
}

void AWowAudioManager::UpdateMusic()
{
    if (!Mpq || PendingZoneMusicId == 0) return;

    const FZoneMusicDbcEntry* ZoneMusic = FDbcStore::Get().ZoneMusic().GetById(PendingZoneMusicId);
    if (!ZoneMusic) return;

    // Pick day or night variant
    uint32 SoundId = bIsDaytime ? ZoneMusic->SoundDayID : ZoneMusic->SoundNightID;
    if (SoundId == 0) SoundId = ZoneMusic->SoundDayID; // Fallback to day

    FString FilePath = GetSoundFilePath(SoundId);
    if (FilePath.IsEmpty())
    {
        UE_LOG(LogWowAudio, Warning, TEXT("No music file for ZoneMusic %d (SoundEntry %d)"),
            PendingZoneMusicId, SoundId);
        return;
    }

    USoundWave* Sound = LoadSoundFromMpq(FilePath);
    if (!Sound)
    {
        UE_LOG(LogWowAudio, Warning, TEXT("Failed to load music: %s"), *FilePath);
        return;
    }

    // Start crossfade — switch active component
    bMusicAActive = !bMusicAActive;
    UAudioComponent* NewActive = bMusicAActive ? MusicComponentA : MusicComponentB;
    NewActive->SetSound(Sound);
    NewActive->SetVolumeMultiplier(0.0f); // Start silent, crossfade will ramp up
    NewActive->Play();
    CrossfadeProgress = 0.0f;

    UE_LOG(LogWowAudio, Log, TEXT("Playing music: %s (ZoneMusic %d, %s)"),
        *FilePath, PendingZoneMusicId, bIsDaytime ? TEXT("day") : TEXT("night"));
}

FString AWowAudioManager::GetSoundFilePath(uint32 SoundEntryId)
{
    const FSoundEntriesDbcEntry* SoundEntry = FDbcStore::Get().SoundEntries().GetById(SoundEntryId);
    if (!SoundEntry) return FString();

    // Pick a random file from the entry's file list
    TArray<FString> ValidFiles;
    for (int32 i = 0; i < 10; i++)
    {
        if (!SoundEntry->FileDataID[i].IsEmpty())
        {
            ValidFiles.Add(SoundEntry->FileDataID[i]);
        }
    }

    if (ValidFiles.Num() == 0) return FString();

    FString FileName = ValidFiles[FMath::RandRange(0, ValidFiles.Num() - 1)];
    FString DirBase = SoundEntry->DirectoryBase;

    // Build full path
    if (DirBase.IsEmpty())
    {
        return FString::Printf(TEXT("Sound\\%s"), *FileName);
    }
    return FString::Printf(TEXT("%s\\%s"), *DirBase, *FileName);
}

void AWowAudioManager::PlaySoundEffect(uint32 SoundEntryId)
{
    if (!Mpq) return;

    FString FilePath = GetSoundFilePath(SoundEntryId);
    if (FilePath.IsEmpty()) return;

    USoundWave* Sound = LoadSoundFromMpq(FilePath);
    if (!Sound) return;

    UGameplayStatics::PlaySound2D(this, Sound, MasterVolume);
}

USoundWave* AWowAudioManager::LoadSoundFromMpq(const FString& FilePath)
{
    if (!Mpq) return nullptr;

    // Check cache
    if (TObjectPtr<USoundWave>* Found = SoundCache.Find(FilePath))
    {
        if (Found->Get()) return Found->Get();
    }

    // Read from MPQ
    TArray<uint8> RawData;
    if (!Mpq->ReadFile(FilePath, RawData))
    {
        return nullptr;
    }

    if (RawData.Num() == 0) return nullptr;

    // Create USoundWave from raw audio data
    USoundWave* SoundWave = NewObject<USoundWave>();
    SoundWave->AddToRoot(); // Prevent GC

    // Detect format from file extension
    bool bIsWav = FilePath.EndsWith(TEXT(".wav"), ESearchCase::IgnoreCase);

    if (bIsWav)
    {
        // WAV: parse header for PCM data
        if (RawData.Num() < 44) return nullptr;

        // WAV header parsing
        const uint8* D = RawData.GetData();
        uint16 NumChannels = *reinterpret_cast<const uint16*>(D + 22);
        uint32 SampleRate = *reinterpret_cast<const uint32*>(D + 24);
        uint16 BitsPerSample = *reinterpret_cast<const uint16*>(D + 34);

        // Find 'data' chunk
        int32 DataOffset = 12;
        int32 DataSize = 0;
        while (DataOffset + 8 < RawData.Num())
        {
            uint32 ChunkId = *reinterpret_cast<const uint32*>(D + DataOffset);
            uint32 ChunkSize = *reinterpret_cast<const uint32*>(D + DataOffset + 4);
            if (ChunkId == 0x61746164) // 'data'
            {
                DataOffset += 8;
                DataSize = FMath::Min(static_cast<int32>(ChunkSize), RawData.Num() - DataOffset);
                break;
            }
            DataOffset += 8 + ChunkSize;
        }

        if (DataSize == 0) return nullptr;

        SoundWave->SetSampleRate(SampleRate);
        SoundWave->NumChannels = NumChannels;
        SoundWave->Duration = static_cast<float>(DataSize) / (SampleRate * NumChannels * (BitsPerSample / 8));

        // Copy PCM data directly into sound wave's runtime buffer
        SoundWave->RawPCMDataSize = DataSize;
        SoundWave->RawPCMData = static_cast<uint8*>(FMemory::Malloc(DataSize));
        FMemory::Memcpy(SoundWave->RawPCMData, D + DataOffset, DataSize);
    }
    else
    {
        // MP3: store raw file data via FSharedBuffer for UE to decode
#if WITH_EDITORONLY_DATA
        FSharedBuffer SharedBuf = FSharedBuffer::Clone(RawData.GetData(), RawData.Num());
        SoundWave->RawData.UpdatePayload(SharedBuf);
#endif

        SoundWave->SetSampleRate(44100);
        SoundWave->NumChannels = 2;
        SoundWave->Duration = 60.0f; // Estimate — corrected on decode
        SoundWave->SoundGroup = ESoundGroup::SOUNDGROUP_Music;
    }

    SoundCache.Add(FilePath, SoundWave);
    UE_LOG(LogWowAudio, Log, TEXT("Loaded sound: %s (%d bytes)"), *FilePath, RawData.Num());

    return SoundWave;
}
