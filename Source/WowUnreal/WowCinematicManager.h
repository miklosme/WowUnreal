#pragma once
#include "CoreMinimal.h"
#include "WowExpansionTypes.h"

class UMediaPlayer;
class UMediaTexture;
class UMediaSource;
class FMpqManager;

/**
 * Manages extraction and playback of WoW login cinematics from MPQ archives.
 * Extracts AVI files to Saved/Cinematics/ and plays them via UE5 MediaFramework.
 */
class FWowCinematicManager
{
public:
    FWowCinematicManager();
    ~FWowCinematicManager();

    /** Extract cinematics from MPQ for the given expansion. Returns true if file is ready. */
    bool PrepareCinematic(FMpqManager* Mpq, EWowExpansion Expansion);

    /** Get the extracted file path for an expansion's cinematic */
    FString GetCinematicPath(EWowExpansion Expansion) const;

    /** Start playing the cinematic for the given expansion */
    void PlayCinematic(EWowExpansion Expansion);

    /** Stop current playback */
    void StopCinematic();

    /** Get the media texture for rendering (bind to UI material) */
    UMediaTexture* GetMediaTexture() const { return MediaTexture; }

    /** Get the media player */
    UMediaPlayer* GetMediaPlayer() const { return MediaPlayer; }

    /** Is a cinematic currently playing? */
    bool IsPlaying() const;

    /** Check if a cinematic file exists for the given expansion */
    bool HasCinematic(EWowExpansion Expansion) const;

private:
    /** Extract a single file from MPQ to disk */
    bool ExtractFromMpq(FMpqManager* Mpq, const FString& MpqPath, const FString& DiskPath);

    /** Get the MPQ path for an expansion's cinematic */
    static FString GetMpqCinematicPath(EWowExpansion Expansion);

    /** Get the directory for extracted cinematics */
    FString GetCinematicDir() const;

    UMediaPlayer* MediaPlayer = nullptr;
    UMediaTexture* MediaTexture = nullptr;

    /** Track which expansions have been extracted */
    TSet<EWowExpansion> ExtractedExpansions;
};
