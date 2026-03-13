#include "WowAssetCache.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMesh.h"

void FWowAssetCache::CacheTexture(const FString& P, UTexture2D* T) { FScopeLock L(&Lock); Textures.Add(P, T); }
UTexture2D* FWowAssetCache::FindTexture(const FString& P) const { FScopeLock L(&Lock); auto* F = Textures.Find(P); return (F && F->IsValid()) ? F->Get() : nullptr; }
void FWowAssetCache::CacheMesh(const FString& P, UStaticMesh* M) { FScopeLock L(&Lock); Meshes.Add(P, M); }
UStaticMesh* FWowAssetCache::FindMesh(const FString& P) const { FScopeLock L(&Lock); auto* F = Meshes.Find(P); return (F && F->IsValid()) ? F->Get() : nullptr; }
void FWowAssetCache::Clear() { FScopeLock L(&Lock); Textures.Empty(); Meshes.Empty(); }
