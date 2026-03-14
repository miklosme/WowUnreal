#include "WowTextureFactory.h"
#include "Engine/Texture2D.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowTexture, Log, All);

UTexture2D* FWowTextureFactory::CreateTexture(const FBlpTexture& BlpData, const FString& Name)
{
    if (!BlpData.bIsValid || BlpData.MipLevels.Num() == 0) return nullptr;

    EPixelFormat PF;
    const TCHAR* PFName = TEXT("Unknown");
    switch (BlpData.PixelFormat)
    {
        case EBlpPixelFormat::DXT1: PF = PF_DXT1; PFName = TEXT("DXT1"); break;
        case EBlpPixelFormat::DXT3: PF = PF_DXT3; PFName = TEXT("DXT3"); break;
        case EBlpPixelFormat::DXT5: PF = PF_DXT5; PFName = TEXT("DXT5"); break;
        case EBlpPixelFormat::RGBA8: PF = PF_R8G8B8A8; PFName = TEXT("RGBA8"); break;
        default:
            UE_LOG(LogWowTexture, Warning, TEXT("Unsupported BLP pixel format for '%s'"), *Name);
            return nullptr;
    }

    UTexture2D* Tex = UTexture2D::CreateTransient(BlpData.Width, BlpData.Height, PF, *Name);
    if (!Tex)
    {
        UE_LOG(LogWowTexture, Error, TEXT("CreateTransient failed for '%s' (%dx%d %s)"),
            *Name, BlpData.Width, BlpData.Height, PFName);
        return nullptr;
    }

    Tex->Filter = TF_Bilinear;
    Tex->SRGB = true;
    Tex->NeverStream = true;

    // For DXT formats, ensure dimensions are at least 4x4 (block-compressed requirement)
    const FBlpMipLevel& Mip0 = BlpData.MipLevels[0];

    void* TexData = Tex->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
    if (TexData)
    {
        FMemory::Memcpy(TexData, Mip0.Data.GetData(), Mip0.Data.Num());
        Tex->GetPlatformData()->Mips[0].BulkData.Unlock();
    }
    Tex->UpdateResource();

    UE_LOG(LogWowTexture, Verbose, TEXT("Created texture '%s': %dx%d %s (%d bytes)"),
        *Name, BlpData.Width, BlpData.Height, PFName, Mip0.Data.Num());

    return Tex;
}
