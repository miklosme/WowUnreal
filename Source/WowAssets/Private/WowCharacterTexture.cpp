#include "WowCharacterTexture.h"
#include "WowTextureFactory.h"
#include "WowAssetCache.h"
#include "Mpq/MpqManager.h"
#include "Formats/BlpParser.h"
#include "Formats/BlpTypes.h"
#include "Formats/Dbc/DbcStore.h"
#include "Engine/Texture2D.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowCharTex, Log, All);

FString FWowCharacterTexture::GetSectionTexture(uint32 RaceId, uint32 Gender, ESectionType Type,
    int32 Variation, int32 Color)
{
    const FCharSectionsDbc& CharSections = FDbcStore::Get().CharSections();

    FString BestMatch;
    int32 BestScore = -1;
    for (const FCharSectionsDbcEntry& Entry : CharSections.GetAll())
    {
        if (Entry.RaceID != RaceId ||
            Entry.SexID != Gender ||
            Entry.Type != static_cast<uint32>(Type))
        {
            continue;
        }

        int32 Score = 0;
        if (Variation != INDEX_NONE)
        {
            if (Entry.Variation != static_cast<uint32>(Variation))
            {
                continue;
            }
            Score += 2;
        }

        if (Color != INDEX_NONE)
        {
            if (Entry.Color != static_cast<uint32>(Color))
            {
                continue;
            }
            Score += 4;
        }

        // When variation is unspecified, prefer variation 0 (the default)
        if (Variation == INDEX_NONE && Entry.Variation == 0)
        {
            Score += 1;
        }

        for (int32 i = 0; i < 3; ++i)
        {
            if (!Entry.Textures[i].IsEmpty() && Score > BestScore)
            {
                BestMatch = Entry.Textures[i];
                BestScore = Score;
                break;
            }
        }
    }

    return BestMatch;
}

/** Decode a DXT1 4x4 block into 16 RGBA pixels */
static void DecodeDXT1Block(const uint8* Block, uint8 OutPixels[64])
{
    uint16 C0 = Block[0] | (Block[1] << 8);
    uint16 C1 = Block[2] | (Block[3] << 8);

    uint8 Colors[4][4]; // [index][RGBA]
    auto Decode565 = [](uint16 C, uint8* Out) {
        Out[0] = ((C >> 11) & 0x1F) * 255 / 31;
        Out[1] = ((C >> 5) & 0x3F) * 255 / 63;
        Out[2] = (C & 0x1F) * 255 / 31;
        Out[3] = 255;
    };

    Decode565(C0, Colors[0]);
    Decode565(C1, Colors[1]);

    if (C0 > C1)
    {
        for (int i = 0; i < 3; i++)
        {
            Colors[2][i] = (2 * Colors[0][i] + Colors[1][i] + 1) / 3;
            Colors[3][i] = (Colors[0][i] + 2 * Colors[1][i] + 1) / 3;
        }
        Colors[2][3] = 255;
        Colors[3][3] = 255;
    }
    else
    {
        for (int i = 0; i < 3; i++)
        {
            Colors[2][i] = (Colors[0][i] + Colors[1][i]) / 2;
            Colors[3][i] = 0;
        }
        Colors[2][3] = 255;
        Colors[3][3] = 0; // Transparent
    }

    uint32 Bits = Block[4] | (Block[5] << 8) | (Block[6] << 16) | (Block[7] << 24);
    for (int p = 0; p < 16; p++)
    {
        uint32 Idx = (Bits >> (p * 2)) & 0x3;
        FMemory::Memcpy(&OutPixels[p * 4], Colors[Idx], 4);
    }
}

/** Decompress a DXT BLP to RGBA8 pixels. Returns empty array for unsupported formats. */
static TArray<uint8> DecompressToRGBA(const FBlpTexture& Blp)
{
    TArray<uint8> Pixels;
    if (!Blp.bIsValid || Blp.MipLevels.Num() == 0) return Pixels;

    uint32 W = Blp.Width;
    uint32 H = Blp.Height;

    if (Blp.PixelFormat == EBlpPixelFormat::RGBA8)
    {
        // Already RGBA — just copy
        Pixels = Blp.MipLevels[0].Data;
        return Pixels;
    }

    if (Blp.PixelFormat == EBlpPixelFormat::DXT1)
    {
        Pixels.SetNumZeroed(W * H * 4);
        const uint8* Src = Blp.MipLevels[0].Data.GetData();
        uint32 BlocksX = FMath::Max(1u, W / 4);
        uint32 BlocksY = FMath::Max(1u, H / 4);

        for (uint32 by = 0; by < BlocksY; by++)
        {
            for (uint32 bx = 0; bx < BlocksX; bx++)
            {
                uint8 BlockPixels[64];
                DecodeDXT1Block(Src, BlockPixels);
                Src += 8;

                for (uint32 py = 0; py < 4 && (by * 4 + py) < H; py++)
                {
                    for (uint32 px = 0; px < 4 && (bx * 4 + px) < W; px++)
                    {
                        uint32 DstIdx = ((by * 4 + py) * W + (bx * 4 + px)) * 4;
                        FMemory::Memcpy(&Pixels[DstIdx], &BlockPixels[(py * 4 + px) * 4], 4);
                    }
                }
            }
        }
        return Pixels;
    }

    // DXT3/DXT5 — skip compositing for these (rare for character textures)
    return Pixels;
}

/** Alpha-blend overlay pixels onto base pixels. Both arrays must be W*H*4 RGBA. */
static void AlphaBlendLayer(TArray<uint8>& Base, const TArray<uint8>& Overlay, uint32 W, uint32 H)
{
    uint32 Count = FMath::Min((uint32)Base.Num(), (uint32)Overlay.Num()) / 4;
    for (uint32 i = 0; i < Count; i++)
    {
        uint32 Off = i * 4;
        uint8 Alpha = Overlay[Off + 3];
        if (Alpha == 0) continue;
        if (Alpha == 255)
        {
            Base[Off + 0] = Overlay[Off + 0];
            Base[Off + 1] = Overlay[Off + 1];
            Base[Off + 2] = Overlay[Off + 2];
            Base[Off + 3] = 255;
        }
        else
        {
            float A = Alpha / 255.0f;
            float InvA = 1.0f - A;
            Base[Off + 0] = FMath::Clamp<int32>(FMath::RoundToInt32(Overlay[Off + 0] * A + Base[Off + 0] * InvA), 0, 255);
            Base[Off + 1] = FMath::Clamp<int32>(FMath::RoundToInt32(Overlay[Off + 1] * A + Base[Off + 1] * InvA), 0, 255);
            Base[Off + 2] = FMath::Clamp<int32>(FMath::RoundToInt32(Overlay[Off + 2] * A + Base[Off + 2] * InvA), 0, 255);
            Base[Off + 3] = 255;
        }
    }
}

/** Load a BLP from MPQ and decompress to RGBA8. Returns empty on failure. */
static TArray<uint8> LoadBlpAsRGBA(FMpqManager* Mpq, const FString& Path, uint32& OutW, uint32& OutH)
{
    OutW = OutH = 0;
    TArray<uint8> BlpRaw;
    if (!Mpq->ReadFile(Path, BlpRaw)) return TArray<uint8>();

    FBlpTexture Blp = FBlpParser::Parse(BlpRaw);
    if (!Blp.bIsValid) return TArray<uint8>();

    OutW = Blp.Width;
    OutH = Blp.Height;
    return DecompressToRGBA(Blp);
}

/** Scale RGBA pixels to target dimensions using nearest-neighbor sampling */
static TArray<uint8> ScalePixels(const TArray<uint8>& Src, uint32 SrcW, uint32 SrcH, uint32 DstW, uint32 DstH)
{
    if (SrcW == DstW && SrcH == DstH) return Src;

    TArray<uint8> Dst;
    Dst.SetNumZeroed(DstW * DstH * 4);
    for (uint32 y = 0; y < DstH; y++)
    {
        uint32 sy = y * SrcH / DstH;
        for (uint32 x = 0; x < DstW; x++)
        {
            uint32 sx = x * SrcW / DstW;
            FMemory::Memcpy(&Dst[(y * DstW + x) * 4], &Src[(sy * SrcW + sx) * 4], 4);
        }
    }
    return Dst;
}

UTexture2D* FWowCharacterTexture::BuildCompositeTexture(FMpqManager* Mpq, FWowAssetCache* Cache,
    const FCustomization& Customization)
{
    if (!Mpq) return nullptr;

    // Cache key includes all customization parameters
    FString CacheKey = FString::Printf(TEXT("CharTex_%d_%d_%d_%d_%d_%d_%d"),
        Customization.RaceId, Customization.Gender, Customization.SkinColor,
        Customization.FaceVariation, Customization.HairStyle, Customization.HairColor,
        Customization.FacialHairStyle);

    UTexture2D* CachedTex = Cache ? Cache->FindTexture(CacheKey) : nullptr;
    if (CachedTex) return CachedTex;

    // 1. Load base skin
    FString SkinPath = GetSectionTexture(Customization.RaceId, Customization.Gender,
        ESectionType::Skin, INDEX_NONE, static_cast<int32>(Customization.SkinColor));
    if (SkinPath.IsEmpty())
    {
        UE_LOG(LogWowCharTex, Warning, TEXT("No skin texture for Race=%d Gender=%d SkinColor=%d"),
            Customization.RaceId, Customization.Gender, Customization.SkinColor);
        return nullptr;
    }

    uint32 SkinW, SkinH;
    TArray<uint8> Composite = LoadBlpAsRGBA(Mpq, SkinPath, SkinW, SkinH);
    if (Composite.Num() == 0)
    {
        UE_LOG(LogWowCharTex, Warning, TEXT("Failed to load/decompress skin: %s"), *SkinPath);
        // Fall back to returning BLP as-is via texture factory
        TArray<uint8> BlpRaw;
        if (Mpq->ReadFile(SkinPath, BlpRaw))
        {
            FBlpTexture Blp = FBlpParser::Parse(BlpRaw);
            if (Blp.bIsValid)
            {
                UTexture2D* Tex = FWowTextureFactory::CreateTexture(Blp, CacheKey);
                if (Tex && Cache) Cache->CacheTexture(CacheKey, Tex);
                return Tex;
            }
        }
        return nullptr;
    }

    int32 LayersComposited = 0;

    // 2. Overlay face texture
    FString FacePath = GetSectionTexture(Customization.RaceId, Customization.Gender,
        ESectionType::Face, static_cast<int32>(Customization.FaceVariation),
        static_cast<int32>(Customization.SkinColor));
    if (!FacePath.IsEmpty())
    {
        uint32 FaceW, FaceH;
        TArray<uint8> FacePixels = LoadBlpAsRGBA(Mpq, FacePath, FaceW, FaceH);
        if (FacePixels.Num() > 0)
        {
            if (FaceW != SkinW || FaceH != SkinH)
                FacePixels = ScalePixels(FacePixels, FaceW, FaceH, SkinW, SkinH);
            AlphaBlendLayer(Composite, FacePixels, SkinW, SkinH);
            LayersComposited++;
            UE_LOG(LogWowCharTex, Verbose, TEXT("Composited face: %s"), *FacePath);
        }
    }

    // 3. Overlay hair texture for scalp/visible hair regions
    FString HairPath = GetSectionTexture(Customization.RaceId, Customization.Gender,
        ESectionType::Hair, static_cast<int32>(Customization.HairStyle),
        static_cast<int32>(Customization.HairColor));
    if (!HairPath.IsEmpty())
    {
        uint32 HairW, HairH;
        TArray<uint8> HairPixels = LoadBlpAsRGBA(Mpq, HairPath, HairW, HairH);
        if (HairPixels.Num() > 0)
        {
            if (HairW != SkinW || HairH != SkinH)
                HairPixels = ScalePixels(HairPixels, HairW, HairH, SkinW, SkinH);
            AlphaBlendLayer(Composite, HairPixels, SkinW, SkinH);
            LayersComposited++;
            UE_LOG(LogWowCharTex, Verbose, TEXT("Composited hair: %s"), *HairPath);
        }
    }

    // 4. Overlay facial hair texture
    FString FacialPath = GetSectionTexture(Customization.RaceId, Customization.Gender,
        ESectionType::FacialHair, static_cast<int32>(Customization.FacialHairStyle),
        static_cast<int32>(Customization.HairColor));
    if (!FacialPath.IsEmpty())
    {
        uint32 FhW, FhH;
        TArray<uint8> FhPixels = LoadBlpAsRGBA(Mpq, FacialPath, FhW, FhH);
        if (FhPixels.Num() > 0)
        {
            if (FhW != SkinW || FhH != SkinH)
                FhPixels = ScalePixels(FhPixels, FhW, FhH, SkinW, SkinH);
            AlphaBlendLayer(Composite, FhPixels, SkinW, SkinH);
            LayersComposited++;
            UE_LOG(LogWowCharTex, Verbose, TEXT("Composited facial hair: %s"), *FacialPath);
        }
    }

    // 5. Overlay underwear texture
    FString UnderwearPath = GetSectionTexture(Customization.RaceId, Customization.Gender,
        ESectionType::Underwear, INDEX_NONE, static_cast<int32>(Customization.SkinColor));
    if (!UnderwearPath.IsEmpty())
    {
        uint32 UwW, UwH;
        TArray<uint8> UwPixels = LoadBlpAsRGBA(Mpq, UnderwearPath, UwW, UwH);
        if (UwPixels.Num() > 0)
        {
            if (UwW != SkinW || UwH != SkinH)
                UwPixels = ScalePixels(UwPixels, UwW, UwH, SkinW, SkinH);
            AlphaBlendLayer(Composite, UwPixels, SkinW, SkinH);
            LayersComposited++;
            UE_LOG(LogWowCharTex, Verbose, TEXT("Composited underwear: %s"), *UnderwearPath);
        }
    }

    // 6. Create UTexture2D from composited RGBA pixels
    UTexture2D* Tex = UTexture2D::CreateTransient(SkinW, SkinH, PF_R8G8B8A8, *CacheKey);
    if (!Tex) return nullptr;

    Tex->Filter = TF_Bilinear;
    Tex->SRGB = true;
    Tex->NeverStream = true;

    void* TexData = Tex->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
    if (TexData)
    {
        FMemory::Memcpy(TexData, Composite.GetData(), Composite.Num());
        Tex->GetPlatformData()->Mips[0].BulkData.Unlock();
    }
    Tex->UpdateResource();

    if (Cache) Cache->CacheTexture(CacheKey, Tex);

    UE_LOG(LogWowCharTex, Log, TEXT("Built composite character texture: %s (%dx%d, %d layers composited)"),
        *SkinPath, SkinW, SkinH, LayersComposited);

    return Tex;
}
