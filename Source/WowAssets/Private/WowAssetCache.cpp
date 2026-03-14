#include "WowAssetCache.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMesh.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowCache, Log, All);

void FWowAssetCache::CacheTexture(const FString& P, UTexture2D* T)
{
	FScopeLock L(&Lock);
	Textures.Add(P, T);
}

UTexture2D* FWowAssetCache::FindTexture(const FString& P) const
{
	FScopeLock L(&Lock);
	auto* F = Textures.Find(P);
	return (F && F->IsValid()) ? F->Get() : nullptr;
}

void FWowAssetCache::CacheMesh(const FString& P, UStaticMesh* M)
{
	FScopeLock L(&Lock);
	Meshes.Add(P, M);
}

UStaticMesh* FWowAssetCache::FindMesh(const FString& P) const
{
	FScopeLock L(&Lock);
	auto* F = Meshes.Find(P);
	return (F && F->IsValid()) ? F->Get() : nullptr;
}

void FWowAssetCache::Clear()
{
	FScopeLock L(&Lock);
	Textures.Empty();
	Meshes.Empty();
}

void FWowAssetCache::PurgeStaleEntries()
{
	FScopeLock L(&Lock);

	int32 PurgedTex = 0, PurgedMesh = 0;

	for (auto It = Textures.CreateIterator(); It; ++It)
	{
		if (!It.Value().IsValid())
		{
			It.RemoveCurrent();
			PurgedTex++;
		}
	}

	for (auto It = Meshes.CreateIterator(); It; ++It)
	{
		if (!It.Value().IsValid())
		{
			It.RemoveCurrent();
			PurgedMesh++;
		}
	}

	if (PurgedTex > 0 || PurgedMesh > 0)
	{
		UE_LOG(LogWowCache, Log, TEXT("Purged %d stale textures, %d stale meshes"), PurgedTex, PurgedMesh);
	}
}

FWowCacheStats FWowAssetCache::GetStats() const
{
	FScopeLock L(&Lock);
	FWowCacheStats Stats;

	for (const auto& Pair : Textures)
	{
		if (Pair.Value.IsValid())
		{
			Stats.TextureCount++;
			Stats.EstimatedTextureMemory += EstimateTextureMemory(Pair.Value.Get());
		}
	}

	for (const auto& Pair : Meshes)
	{
		if (Pair.Value.IsValid())
		{
			Stats.MeshCount++;
			Stats.EstimatedMeshMemory += EstimateMeshMemory(Pair.Value.Get());
		}
	}

	return Stats;
}

bool FWowAssetCache::IsOverBudget() const
{
	if (MemoryBudget <= 0) return false;
	FWowCacheStats Stats = GetStats();
	return Stats.TotalEstimatedMemory() > MemoryBudget;
}

int64 FWowAssetCache::EstimateTextureMemory(UTexture2D* Tex)
{
	if (!Tex) return 0;
	int32 W = Tex->GetSizeX();
	int32 H = Tex->GetSizeY();
	// Estimate: W * H * 4 bytes (RGBA) * 1.33 (mipmaps)
	return (int64)(W * H * 4 * 1.33);
}

int64 FWowAssetCache::EstimateMeshMemory(UStaticMesh* Mesh)
{
	if (!Mesh) return 0;
	// Rough estimate: vertex count * ~48 bytes + index count * 4
	int64 Est = 0;
	if (Mesh->GetRenderData())
	{
		for (const FStaticMeshLODResources& LOD : Mesh->GetRenderData()->LODResources)
		{
			Est += LOD.GetNumVertices() * 48;
			Est += LOD.IndexBuffer.GetNumIndices() * 4;
		}
	}
	return Est > 0 ? Est : 64 * 1024; // default 64KB if we can't calculate
}
