#include "WowScreenshotManager.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "HAL/PlatformFileManager.h"
#include "UnrealClient.h"

FString UWowScreenshotManager::GetScreenshotDir() const { return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots")); }

FString UWowScreenshotManager::TakeScreenshot()
{
    FString Dir = GetScreenshotDir();
    IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
    PF.CreateDirectoryTree(*Dir);
    FString File = FPaths::Combine(Dir, FString::Printf(TEXT("WoW_%s.png"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"))));
    FScreenshotRequest::RequestScreenshot(File, true, false);
    return File;
}
