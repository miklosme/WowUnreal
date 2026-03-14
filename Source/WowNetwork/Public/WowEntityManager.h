#pragma once
#include "CoreMinimal.h"
#include "WowEntity.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnEntityCreated, const FWowEntity&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnEntityUpdated, const FWowEntity&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnEntityDestroyed, uint64 /*Guid*/);

class WOWNETWORK_API FWowEntityManager
{
public:
    /** Get or create an entity by GUID */
    FWowEntity& GetOrCreate(uint64 Guid);

    /** Promote an existing generic entity to the concrete subclass implied by its type mask. */
    FWowEntity& PromoteToTyped(uint64 Guid, uint32 TypeMask);

    /** Find an entity, returns nullptr if not found */
    FWowEntity* Find(uint64 Guid);

    /** Typed find helpers */
    FWowItemEntity* FindItem(uint64 Guid);
    FWowContainerEntity* FindContainer(uint64 Guid);
    FWowUnitEntity* FindUnit(uint64 Guid);
    FWowPlayerEntity* FindPlayer(uint64 Guid);
    FWowGameObjectEntity* FindGameObject(uint64 Guid);
    FWowDynamicObjectEntity* FindDynamicObject(uint64 Guid);
    FWowCorpseEntity* FindCorpse(uint64 Guid);

    /** Remove an entity */
    void Remove(uint64 Guid);

    /** Remove all entities */
    void Clear();

    /** Get all entities */
    const TMap<uint64, TSharedPtr<FWowEntity>>& GetAll() const { return Entities; }

    /** Get entity count */
    int32 Num() const { return Entities.Num(); }

    /** The local player's GUID (set when LOGIN_VERIFY_WORLD is received) */
    uint64 LocalPlayerGuid = 0;

    /** Find the local player entity */
    FWowPlayerEntity* GetLocalPlayer() { return FindPlayer(LocalPlayerGuid); }

    // Events
    FOnEntityCreated OnEntityCreated;
    FOnEntityUpdated OnEntityUpdated;
    FOnEntityDestroyed OnEntityDestroyed;

private:
    TMap<uint64, TSharedPtr<FWowEntity>> Entities;
};
