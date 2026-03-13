#include "Mpq/MpqManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"

// StormLib - conditionally include
#if __has_include("StormLib.h")
THIRD_PARTY_INCLUDES_START
#include "StormLib.h"
THIRD_PARTY_INCLUDES_END
#define HAS_STORMLIB 1
#else
#define HAS_STORMLIB 0
#endif

DEFINE_LOG_CATEGORY_STATIC(LogMpq, Log, All);

FMpqManager::FMpqManager() {}
FMpqManager::~FMpqManager() { Shutdown(); }

bool FMpqManager::Initialize(const FString& InDataPath)
{
    if (bInitialized) Shutdown();
    DataPath = InDataPath;

    if (!FPaths::DirectoryExists(DataPath))
    {
        UE_LOG(LogMpq, Error, TEXT("WoW Data directory not found: %s"), *DataPath);
        return false;
    }

#if HAS_STORMLIB
    TArray<FString> ArchiveNames = {
        TEXT("enUS/patch-enUS-3.MPQ"), TEXT("enUS/patch-enUS-2.MPQ"), TEXT("enUS/patch-enUS.MPQ"),
        TEXT("patch-3.MPQ"), TEXT("patch-2.MPQ"), TEXT("patch.MPQ"),
        TEXT("enUS/lichking-locale-enUS.MPQ"), TEXT("enUS/lichking-speech-enUS.MPQ"), TEXT("lichking.MPQ"),
        TEXT("enUS/expansion-locale-enUS.MPQ"), TEXT("enUS/expansion-speech-enUS.MPQ"), TEXT("expansion.MPQ"),
        TEXT("enUS/base-enUS.MPQ"), TEXT("enUS/speech-enUS.MPQ"),
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
#else
    UE_LOG(LogMpq, Warning, TEXT("StormLib not available - MPQ reading disabled"));
    bInitialized = false;
#endif
    return bInitialized;
}

void FMpqManager::Shutdown()
{
    FScopeLock Lock(&ArchiveLock);
#if HAS_STORMLIB
    for (HANDLE h : Archives) { if (h) SFileCloseArchive(h); }
#endif
    Archives.Empty();
    bInitialized = false;
}

HANDLE FMpqManager::OpenArchive(const FString& ArchivePath) const
{
#if HAS_STORMLIB
    HANDLE hArchive = nullptr;
    if (SFileOpenArchive(TCHAR_TO_UTF8(*ArchivePath), 0, MPQ_OPEN_READ_ONLY, &hArchive))
        return hArchive;
#endif
    return nullptr;
}

bool FMpqManager::ReadFile(const FString& FilePath, TArray<uint8>& OutData) const
{
    FScopeLock Lock(&ArchiveLock);
    if (!bInitialized) return false;

#if HAS_STORMLIB
    FString Normalized = FilePath;
    Normalized.ReplaceInline(TEXT("/"), TEXT("\\"));
    for (HANDLE h : Archives)
    {
        HANDLE hFile = nullptr;
        if (SFileOpenFileEx(h, TCHAR_TO_UTF8(*Normalized), 0, &hFile))
        {
            DWORD Size = SFileGetFileSize(hFile, nullptr);
            if (Size == SFILE_INVALID_SIZE || Size == 0) { SFileCloseFile(hFile); continue; }
            OutData.SetNumUninitialized(Size);
            DWORD Read = 0;
            bool Ok = SFileReadFile(hFile, OutData.GetData(), Size, &Read, nullptr);
            SFileCloseFile(hFile);
            if (Ok && Read == Size) return true;
            OutData.Empty();
        }
    }
#endif
    return false;
}

bool FMpqManager::FileExists(const FString& FilePath) const
{
    FScopeLock Lock(&ArchiveLock);
    if (!bInitialized) return false;
#if HAS_STORMLIB
    FString Normalized = FilePath;
    Normalized.ReplaceInline(TEXT("/"), TEXT("\\"));
    for (HANDLE h : Archives)
    {
        HANDLE hFile = nullptr;
        if (SFileOpenFileEx(h, TCHAR_TO_UTF8(*Normalized), 0, &hFile))
        {
            SFileCloseFile(hFile);
            return true;
        }
    }
#endif
    return false;
}
