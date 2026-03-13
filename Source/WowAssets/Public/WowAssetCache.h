#pragma once
#include "CoreMinimal.h"
class UTexture2D;
class UStaticMesh;
class WOWASSETS_API FWowAssetCache
{
public:
    void CacheTexture(const FString& Path, UTexture2D* Tex);
    UTexture2D* FindTexture(const FString& Path) const;
    void CacheMesh(const FString& Path, UStaticMesh* Mesh);
    UStaticMesh* FindMesh(const FString& Path) const;
    void Clear();
private:
    mutable FCriticalSection Lock;
    TMap<FString, TWeakObjectPtr<UTexture2D>> Textures;
    TMap<FString, TWeakObjectPtr<UStaticMesh>> Meshes;
};
