#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WowAudioManager.generated.h"

class USoundWave;
class UAudioComponent;
class FMpqManager;

/**
 * Manages WoW audio: zone music, ambient sounds, and sound effects.
 * Loads MP3/WAV from MPQ archives into USoundWave, handles crossfading.
 */
UCLASS()
class WOWWORLD_API AWowAudioManager : public AActor
{
    GENERATED_BODY()
public:
    AWowAudioManager();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    /** Set the MPQ manager for loading audio files */
    void SetMpqManager(FMpqManager* InMpq) { Mpq = InMpq; }

    /** Update the current zone (triggers music/ambience change) */
    void SetCurrentZone(uint32 ZoneId, uint32 AreaId);

    /** Play a sound effect by SoundEntries.dbc ID (one-shot) */
    void PlaySoundEffect(uint32 SoundEntryId);

    /** Master volume (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
    float MasterVolume = 1.0f;

    /** Music volume (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
    float MusicVolume = 0.5f;

    /** Ambient volume (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
    float AmbienceVolume = 0.5f;

    /** Is it currently daytime? (affects music/ambience variant) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
    bool bIsDaytime = true;

    /** Crossfade duration in seconds */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
    float CrossfadeDuration = 3.0f;

private:
    FMpqManager* Mpq = nullptr;

    /** Load an audio file from MPQ into a USoundWave */
    USoundWave* LoadSoundFromMpq(const FString& FilePath);

    /** Get a random sound file path from a SoundEntries.dbc entry */
    FString GetSoundFilePath(uint32 SoundEntryId);

    /** Start playing music for the current zone */
    void UpdateMusic();

    /** Current zone/area */
    uint32 CurrentZoneId = 0;
    uint32 CurrentAreaId = 0;
    uint32 PendingZoneMusicId = 0;

    /** Music playback */
    UPROPERTY()
    TObjectPtr<UAudioComponent> MusicComponentA;

    UPROPERTY()
    TObjectPtr<UAudioComponent> MusicComponentB;

    bool bMusicAActive = true;
    float CrossfadeProgress = 1.0f; // 1.0 = fully faded, no fade in progress

    /** Ambient playback */
    UPROPERTY()
    TObjectPtr<UAudioComponent> AmbienceComponent;

    /** Cached sound waves */
    UPROPERTY()
    TMap<FString, TObjectPtr<USoundWave>> SoundCache;
};
