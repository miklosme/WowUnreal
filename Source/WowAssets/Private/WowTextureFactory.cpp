#include "WowTextureFactory.h"
#include "Engine/Texture2D.h"

UTexture2D* FWowTextureFactory::CreateTexture(const FBlpTexture& BlpData, const FString& Name)
{
    if (!BlpData.bIsValid || BlpData.MipLevels.Num() == 0) return nullptr;
    EPixelFormat PF;
    switch (BlpData.PixelFormat)
    {
        case EBlpPixelFormat::DXT1: PF = PF_DXT1; break;
        case EBlpPixelFormat::DXT3: PF = PF_DXT3; break;
        case EBlpPixelFormat::DXT5: PF = PF_DXT5; break;
        case EBlpPixelFormat::RGBA8: PF = PF_R8G8B8A8; break;
        default: return nullptr;
    }
    UTexture2D* Tex = UTexture2D::CreateTransient(BlpData.Width, BlpData.Height, PF, *Name);
    if (!Tex) return nullptr;
    Tex->Filter = TF_Bilinear;
    Tex->SRGB = true;
    Tex->NeverStream = true;
    const FBlpMipLevel& Mip0 = BlpData.MipLevels[0];
    void* TexData = Tex->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
    if (TexData) { FMemory::Memcpy(TexData, Mip0.Data.GetData(), Mip0.Data.Num()); Tex->GetPlatformData()->Mips[0].BulkData.Unlock(); }
    Tex->UpdateResource();
    return Tex;
}
