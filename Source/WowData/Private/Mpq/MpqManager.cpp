#include "Mpq/MpqManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "StormLibWrapper.h"

DEFINE_LOG_CATEGORY_STATIC(LogMpq, Log, All);

FMpqManager::FMpqManager() {}
FMpqManager::~FMpqManager() { Shutdown(); }

bool FMpqManager::Initialize(const FString& InDataPath)
{
    if (bInitialized) Shutdown();

    FScopeLock Lock(&ArchiveLock);
    DataPath = InDataPath;

    if (!FPaths::DirectoryExists(DataPath))
    {
        UE_LOG(LogMpq, Error, TEXT("WoW Data directory not found: %s"), *DataPath);
        return false;
    }

    // Load order: patches first (highest priority), then base archives.
    // locale-enUS.MPQ contains Interface/ textures needed by some M2 models.
    TArray<FString> ArchiveNames = {
        TEXT("enUS/patch-enUS-3.MPQ"), TEXT("enUS/patch-enUS-2.MPQ"), TEXT("enUS/patch-enUS.MPQ"),
        TEXT("patch-3.MPQ"), TEXT("patch-2.MPQ"), TEXT("patch.MPQ"),
        TEXT("enUS/lichking-locale-enUS.MPQ"), TEXT("enUS/lichking-speech-enUS.MPQ"), TEXT("lichking.MPQ"),
        TEXT("enUS/expansion-locale-enUS.MPQ"), TEXT("enUS/expansion-speech-enUS.MPQ"), TEXT("expansion.MPQ"),
        TEXT("enUS/locale-enUS.MPQ"), TEXT("enUS/base-enUS.MPQ"), TEXT("enUS/speech-enUS.MPQ"),
        TEXT("enUS/backup-enUS.MPQ"),
        TEXT("common.MPQ"), TEXT("common-2.MPQ"),
    };

    int32 OpenedCount = 0;
    for (const FString& Name : ArchiveNames)
    {
        FString FullPath = FPaths::Combine(DataPath, Name);
        if (FPaths::FileExists(FullPath))
        {
            HANDLE hArchive = OpenArchive(FullPath);
            if (hArchive)
            {
                Archives.Add(hArchive);
                OpenedCount++;
                UE_LOG(LogMpq, Log, TEXT("Opened MPQ: %s"), *Name);
            }
        }
    }
    bInitialized = OpenedCount > 0;
    UE_LOG(LogMpq, Log, TEXT("MPQ Manager: %d archives from %s"), OpenedCount, *DataPath);
    return bInitialized;
}

void FMpqManager::Shutdown()
{
    FScopeLock Lock(&ArchiveLock);
    for (HANDLE h : Archives)
    {
        if (h) StormLib_CloseArchive(h);
    }
    Archives.Empty();
    bInitialized = false;
}

HANDLE FMpqManager::OpenArchive(const FString& ArchivePath) const
{
    STORMLIB_HANDLE hArchive = nullptr;
    if (StormLib_OpenArchive(TCHAR_TO_UTF8(*ArchivePath), &hArchive))
        return hArchive;
    return nullptr;
}

bool FMpqManager::ReadFile(const FString& FilePath, TArray<uint8>& OutData) const
{
    FScopeLock Lock(&ArchiveLock);
    if (!bInitialized) return false;

    FString Normalized = FilePath;
    Normalized.ReplaceInline(TEXT("/"), TEXT("\\"));

    for (HANDLE h : Archives)
    {
        STORMLIB_HANDLE hFile = nullptr;
        if (StormLib_OpenFile(h, TCHAR_TO_UTF8(*Normalized), &hFile))
        {
            uint32 Size = StormLib_GetFileSize(hFile);
            if (Size == 0 || Size == 0xFFFFFFFF)
            {
                StormLib_CloseFile(hFile);
                continue;
            }
            OutData.SetNumUninitialized(Size);
            uint32 Read = 0;
            bool Ok = StormLib_ReadFile(hFile, OutData.GetData(), Size, &Read);
            StormLib_CloseFile(hFile);
            if (Ok && Read == Size) return true;
            OutData.Empty();
        }
    }
    return false;
}

bool FMpqManager::FileExists(const FString& FilePath) const
{
    FScopeLock Lock(&ArchiveLock);
    if (!bInitialized) return false;

    FString Normalized = FilePath;
    Normalized.ReplaceInline(TEXT("/"), TEXT("\\"));

    for (HANDLE h : Archives)
    {
        STORMLIB_HANDLE hFile = nullptr;
        if (StormLib_OpenFile(h, TCHAR_TO_UTF8(*Normalized), &hFile))
        {
            StormLib_CloseFile(hFile);
            return true;
        }
    }
    return false;
}
