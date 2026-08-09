#include "WowAudioManager.h"
#include "Mpq/MpqManager.h"
#include "Formats/Dbc/DbcStore.h"
#include "Audio.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWaveProcedural.h"
#include "Kismet/GameplayStatics.h"

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

    if (AreaEntry)
    {
        if (AreaEntry->ZoneMusicID != 0 && AreaEntry->ZoneMusicID != PendingZoneMusicId)
        {
            PendingZoneMusicId = AreaEntry->ZoneMusicID;
            UpdateMusic();
        }

        if (AreaEntry->AmbienceID != 0 && AreaEntry->AmbienceID != PendingAmbienceId)
        {
            PendingAmbienceId = AreaEntry->AmbienceID;
            UpdateAmbience();
        }
    }

    UE_LOG(LogWowAudio, Log, TEXT("Zone changed: zone=%d area=%d musicId=%d ambienceId=%d"),
        ZoneId, AreaId, PendingZoneMusicId, PendingAmbienceId);
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

void AWowAudioManager::UpdateAmbience()
{
    if (!Mpq || PendingAmbienceId == 0) return;

    const FSoundAmbienceDbcEntry* Ambience = FDbcStore::Get().SoundAmbience().GetById(PendingAmbienceId);
    if (!Ambience) return;

    uint32 SoundId = bIsDaytime ? Ambience->DayAmbience : Ambience->NightAmbience;
    if (SoundId == 0) SoundId = Ambience->DayAmbience;
    if (SoundId == 0) return;

    FString FilePath = GetSoundFilePath(SoundId);
    if (FilePath.IsEmpty()) return;

    USoundWave* Sound = LoadSoundFromMpq(FilePath);
    if (!Sound) return;

    if (AmbienceComponent)
    {
        AmbienceComponent->Stop();
        AmbienceComponent->SetSound(Sound);
        AmbienceComponent->SetVolumeMultiplier(AmbienceVolume * MasterVolume);
        AmbienceComponent->Play();

        UE_LOG(LogWowAudio, Log, TEXT("Playing ambience: %s (AmbienceId %d, %s)"),
            *FilePath, PendingAmbienceId, bIsDaytime ? TEXT("day") : TEXT("night"));
    }
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

    // Detect format from file extension
    bool bIsWav = FilePath.EndsWith(TEXT(".wav"), ESearchCase::IgnoreCase);

    if (!bIsWav)
    {
        // UE 5.8 cannot construct a playable compressed sound merely by
        // attaching MP3 bytes to a transient USoundWave. Keep this disabled
        // until the runtime importer supplies decoded PCM.
        UE_LOG(LogWowAudio, Warning, TEXT("Unsupported runtime audio format: %s (MP3 decoding not implemented)"), *FilePath);
        return nullptr;
    }

    FWaveModInfo WaveInfo;
    FString WaveError;
    if (!WaveInfo.ReadWaveInfo(RawData.GetData(), RawData.Num(), &WaveError))
    {
        UE_LOG(LogWowAudio, Warning, TEXT("Invalid WAV %s: %s"), *FilePath, *WaveError);
        return nullptr;
    }

    const uint16 FormatTag = WaveInfo.pFormatTag ? *WaveInfo.pFormatTag : 0;
    const uint16 BitsPerSample = WaveInfo.pBitsPerSample ? *WaveInfo.pBitsPerSample : 0;
    const uint16 NumChannels = WaveInfo.pChannels ? *WaveInfo.pChannels : 0;
    const uint32 SampleRate = WaveInfo.pSamplesPerSec ? *WaveInfo.pSamplesPerSec : 0;
    const uint32 AverageBytesPerSecond = WaveInfo.pAvgBytesPerSec ? *WaveInfo.pAvgBytesPerSec : 0;
    if (FormatTag != FWaveModInfo::WAVE_INFO_FORMAT_PCM || BitsPerSample != 16 ||
        NumChannels == 0 || SampleRate == 0 || AverageBytesPerSecond == 0 ||
        !WaveInfo.SampleDataStart || WaveInfo.SampleDataSize == 0)
    {
        UE_LOG(LogWowAudio, Warning,
            TEXT("Unsupported WAV %s (format=%u bits=%u channels=%u rate=%u)"),
            *FilePath, FormatTag, BitsPerSample, NumChannels, SampleRate);
        return nullptr;
    }

    // Transient PCM must use the procedural path. A regular USoundWave is an
    // asset-backed object; UE 5.8 tries to build its streamed platform data on
    // the audio thread and asserts in CacheInheritedLoadingBehavior().
    USoundWaveProcedural* SoundWave = NewObject<USoundWaveProcedural>(this);
    SoundWave->SetSampleRate(SampleRate);
    SoundWave->NumChannels = NumChannels;
    SoundWave->Duration = static_cast<float>(WaveInfo.SampleDataSize) /
        static_cast<float>(AverageBytesPerSecond);
    SoundWave->bCanProcessAsync = true;
    SoundWave->QueueAudio(WaveInfo.SampleDataStart, static_cast<int32>(WaveInfo.SampleDataSize));

    SoundCache.Add(FilePath, SoundWave);
    UE_LOG(LogWowAudio, Log, TEXT("Loaded sound: %s (%d bytes)"), *FilePath, RawData.Num());

    return SoundWave;
}
