#pragma once
#include "CoreMinimal.h"
#include "WowExpansionTypes.h"

class UMediaPlayer;
class UMediaTexture;
class UMediaSource;
class FMpqManager;

/**
 * Manages playback of WoW login cinematics.
 * Finds AVI files in the WoW Data directory (loose files, not in MPQ)
 * and plays them via UE5 MediaFramework.
 */
class FWowCinematicManager
{
public:
    FWowCinematicManager();
    ~FWowCinematicManager();

    /** Find the cinematic file for the given expansion on disk */
    bool PrepareCinematic(FMpqManager* Mpq, EWowExpansion Expansion);

    /** Get the full path for an expansion's cinematic (empty if not found) */
    FString GetCinematicPath(EWowExpansion Expansion) const;

    /** Start playing the cinematic for the given expansion */
    void PlayCinematic(EWowExpansion Expansion);

    /** Stop current playback */
    void StopCinematic();

    /** Get the media texture for rendering (bind to UI) */
    UMediaTexture* GetMediaTexture() const { return MediaTexture; }

    /** Get the media player */
    UMediaPlayer* GetMediaPlayer() const { return MediaPlayer; }

    /** Is a cinematic currently playing? */
    bool IsPlaying() const;

    /** Check if a cinematic file was found for the given expansion */
    bool HasCinematic(EWowExpansion Expansion) const;

private:
    /** Get the expected filename for an expansion's cinematic */
    static FString GetCinematicFilename(EWowExpansion Expansion);

    UMediaPlayer* MediaPlayer = nullptr;
    UMediaTexture* MediaTexture = nullptr;

    /** Maps expansion → absolute file path on disk */
    TMap<EWowExpansion, FString> CinematicPaths;

    /** Track which expansions have been searched */
    TSet<EWowExpansion> ExtractedExpansions;
};
