#pragma once
#include "CoreMinimal.h"

typedef void* HANDLE;

class WOWDATA_API FMpqManager
{
public:
    FMpqManager();
    ~FMpqManager();
    bool Initialize(const FString& DataPath);
    void Shutdown();
    bool ReadFile(const FString& FilePath, TArray<uint8>& OutData) const;
    bool FileExists(const FString& FilePath) const;
    const FString& GetDataPath() const { return DataPath; }
    bool IsInitialized() const { return bInitialized; }

private:
    HANDLE OpenArchive(const FString& ArchivePath) const;
    TArray<HANDLE> Archives;
    FString DataPath;
    bool bInitialized = false;
    mutable FCriticalSection ArchiveLock;
};
