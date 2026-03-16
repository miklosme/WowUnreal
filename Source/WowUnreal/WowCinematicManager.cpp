#include "WowCinematicManager.h"
#include "Mpq/MpqManager.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "FileMediaSource.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowCinematic, Log, All);

FWowCinematicManager::FWowCinematicManager()
{
    // Create media player
    MediaPlayer = NewObject<UMediaPlayer>(GetTransientPackage(), NAME_None, RF_Transient);
    if (MediaPlayer)
    {
        MediaPlayer->AddToRoot();
        MediaPlayer->SetLooping(true);
        MediaPlayer->PlayOnOpen = true;
    }

    // Create media texture bound to the player
    MediaTexture = NewObject<UMediaTexture>(GetTransientPackage(), NAME_None, RF_Transient);
    if (MediaTexture)
    {
        MediaTexture->AddToRoot();
        MediaTexture->AutoClear = true;
        MediaTexture->SetMediaPlayer(MediaPlayer);
        MediaTexture->UpdateResource();
    }
}

FWowCinematicManager::~FWowCinematicManager()
{
    StopCinematic();

    if (MediaTexture)
    {
        MediaTexture->RemoveFromRoot();
        MediaTexture = nullptr;
    }
    if (MediaPlayer)
    {
        MediaPlayer->RemoveFromRoot();
        MediaPlayer = nullptr;
    }
}

FString FWowCinematicManager::GetMpqCinematicPath(EWowExpansion Expansion)
{
    switch (Expansion)
    {
    case EWowExpansion::Classic:
        return TEXT("Interface\\Cinematics\\WOW_Intro.avi");
    case EWowExpansion::BurningCrusade:
        return TEXT("Interface\\Cinematics\\BurningCrusade_Intro.avi");
    case EWowExpansion::WrathOfTheLichKing:
    default:
        return TEXT("Interface\\Cinematics\\Wrath_Intro.avi");
    }
}

FString FWowCinematicManager::GetCinematicDir() const
{
    return FPaths::ProjectSavedDir() / TEXT("Cinematics");
}

FString FWowCinematicManager::GetCinematicPath(EWowExpansion Expansion) const
{
    FString Filename;
    switch (Expansion)
    {
    case EWowExpansion::Classic:
        Filename = TEXT("WOW_Intro.avi");
        break;
    case EWowExpansion::BurningCrusade:
        Filename = TEXT("BurningCrusade_Intro.avi");
        break;
    case EWowExpansion::WrathOfTheLichKing:
    default:
        Filename = TEXT("Wrath_Intro.avi");
        break;
    }
    return GetCinematicDir() / Filename;
}

bool FWowCinematicManager::ExtractFromMpq(FMpqManager* Mpq, const FString& MpqPath, const FString& DiskPath)
{
    if (!Mpq) return false;

    // Check if already extracted
    if (FPaths::FileExists(DiskPath))
    {
        UE_LOG(LogWowCinematic, Log, TEXT("Cinematic already extracted: %s"), *DiskPath);
        return true;
    }

    // Read from MPQ
    TArray<uint8> FileData;
    if (!Mpq->ReadFile(MpqPath, FileData))
    {
        UE_LOG(LogWowCinematic, Warning, TEXT("Cinematic not found in MPQ: %s"), *MpqPath);
        return false;
    }

    UE_LOG(LogWowCinematic, Log, TEXT("Extracting cinematic: %s (%d bytes)"), *MpqPath, FileData.Num());

    // Ensure directory exists
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    FString Dir = FPaths::GetPath(DiskPath);
    if (!PlatformFile.DirectoryExists(*Dir))
    {
        PlatformFile.CreateDirectoryTree(*Dir);
    }

    // Write to disk
    if (!FFileHelper::SaveArrayToFile(FileData, *DiskPath))
    {
        UE_LOG(LogWowCinematic, Error, TEXT("Failed to write cinematic to: %s"), *DiskPath);
        return false;
    }

    UE_LOG(LogWowCinematic, Log, TEXT("Extracted cinematic to: %s"), *DiskPath);
    return true;
}

bool FWowCinematicManager::PrepareCinematic(FMpqManager* Mpq, EWowExpansion Expansion)
{
    if (ExtractedExpansions.Contains(Expansion))
    {
        return HasCinematic(Expansion);
    }

    FString MpqPath = GetMpqCinematicPath(Expansion);
    FString DiskPath = GetCinematicPath(Expansion);

    bool bSuccess = ExtractFromMpq(Mpq, MpqPath, DiskPath);
    if (bSuccess)
    {
        ExtractedExpansions.Add(Expansion);
    }
    return bSuccess;
}

void FWowCinematicManager::PlayCinematic(EWowExpansion Expansion)
{
    if (!MediaPlayer) return;

    FString Path = GetCinematicPath(Expansion);
    if (!FPaths::FileExists(Path))
    {
        UE_LOG(LogWowCinematic, Warning, TEXT("Cinematic file not found: %s"), *Path);
        return;
    }

    // Convert to absolute path
    FString AbsPath = FPaths::ConvertRelativePathToFull(Path);

    UE_LOG(LogWowCinematic, Log, TEXT("Playing cinematic: %s"), *AbsPath);

    // Open the file directly via URL
    if (!MediaPlayer->OpenUrl(AbsPath))
    {
        UE_LOG(LogWowCinematic, Error, TEXT("Failed to open cinematic: %s"), *AbsPath);
    }
}

void FWowCinematicManager::StopCinematic()
{
    if (MediaPlayer)
    {
        MediaPlayer->Close();
    }
}

bool FWowCinematicManager::IsPlaying() const
{
    return MediaPlayer && MediaPlayer->IsPlaying();
}

bool FWowCinematicManager::HasCinematic(EWowExpansion Expansion) const
{
    return FPaths::FileExists(GetCinematicPath(Expansion));
}
