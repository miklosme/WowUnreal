#include "WowEntityManager.h"

namespace
{
EWowEntityKind ResolveEntityKind(uint32 TypeMask)
{
    if (TypeMask & WowTypeMask::PLAYER)
    {
        return EWowEntityKind::Player;
    }
    if (TypeMask & WowTypeMask::CONTAINER)
    {
        return EWowEntityKind::Container;
    }
    if (TypeMask & WowTypeMask::ITEM)
    {
        return EWowEntityKind::Item;
    }
    if (TypeMask & WowTypeMask::GAMEOBJECT)
    {
        return EWowEntityKind::GameObject;
    }
    if (TypeMask & WowTypeMask::DYNAMICOBJECT)
    {
        return EWowEntityKind::DynamicObject;
    }
    if (TypeMask & WowTypeMask::CORPSE)
    {
        return EWowEntityKind::Corpse;
    }
    if (TypeMask & WowTypeMask::UNIT)
    {
        return EWowEntityKind::Unit;
    }
    return EWowEntityKind::Object;
}

TSharedPtr<FWowEntity> MakeTypedEntity(const FWowEntity& Source, EWowEntityKind Kind)
{
    switch (Kind)
    {
    case EWowEntityKind::Item:
        return MakeShared<FWowItemEntity>(Source);
    case EWowEntityKind::Container:
        return MakeShared<FWowContainerEntity>(Source);
    case EWowEntityKind::Unit:
        return MakeShared<FWowUnitEntity>(Source);
    case EWowEntityKind::Player:
        return MakeShared<FWowPlayerEntity>(Source);
    case EWowEntityKind::GameObject:
        return MakeShared<FWowGameObjectEntity>(Source);
    case EWowEntityKind::DynamicObject:
        return MakeShared<FWowDynamicObjectEntity>(Source);
    case EWowEntityKind::Corpse:
        return MakeShared<FWowCorpseEntity>(Source);
    case EWowEntityKind::Object:
    default:
        return MakeShared<FWowEntity>(Source);
    }
}
}

FWowEntity& FWowEntityManager::GetOrCreate(uint64 Guid)
{
    TSharedPtr<FWowEntity>& EntityPtr = Entities.FindOrAdd(Guid);
    if (!EntityPtr)
    {
        EntityPtr = MakeShared<FWowEntity>();
        EntityPtr->Guid = Guid;
    }
    return *EntityPtr;
}

FWowEntity& FWowEntityManager::PromoteToTyped(uint64 Guid, uint32 TypeMask)
{
    FWowEntity& CurrentEntity = GetOrCreate(Guid);
    if (TypeMask == 0)
    {
        return CurrentEntity;
    }

    TSharedPtr<FWowEntity>* EntityPtr = Entities.Find(Guid);
    if (!EntityPtr || !EntityPtr->IsValid())
    {
        return CurrentEntity;
    }

    (*EntityPtr)->Guid = Guid;
    (*EntityPtr)->TypeMask = TypeMask;

    const EWowEntityKind DesiredKind = ResolveEntityKind(TypeMask);
    if ((*EntityPtr)->GetEntityKind() == DesiredKind)
    {
        return **EntityPtr;
    }

    *EntityPtr = MakeTypedEntity(**EntityPtr, DesiredKind);
    (*EntityPtr)->Guid = Guid;
    (*EntityPtr)->TypeMask = TypeMask;
    return **EntityPtr;
}

FWowEntity* FWowEntityManager::Find(uint64 Guid)
{
    TSharedPtr<FWowEntity>* Ptr = Entities.Find(Guid);
    return Ptr ? Ptr->Get() : nullptr;
}

FWowItemEntity* FWowEntityManager::FindItem(uint64 Guid)
{
    FWowEntity* Entity = Find(Guid);
    if (!Entity)
    {
        return nullptr;
    }

    const EWowEntityKind Kind = Entity->GetEntityKind();
    return (Kind == EWowEntityKind::Item || Kind == EWowEntityKind::Container)
        ? static_cast<FWowItemEntity*>(Entity)
        : nullptr;
}

FWowContainerEntity* FWowEntityManager::FindContainer(uint64 Guid)
{
    FWowEntity* Entity = Find(Guid);
    return (Entity && Entity->GetEntityKind() == EWowEntityKind::Container)
        ? static_cast<FWowContainerEntity*>(Entity)
        : nullptr;
}

FWowUnitEntity* FWowEntityManager::FindUnit(uint64 Guid)
{
    FWowEntity* Entity = Find(Guid);
    if (!Entity)
    {
        return nullptr;
    }

    const EWowEntityKind Kind = Entity->GetEntityKind();
    return (Kind == EWowEntityKind::Unit || Kind == EWowEntityKind::Player)
        ? static_cast<FWowUnitEntity*>(Entity)
        : nullptr;
}

FWowPlayerEntity* FWowEntityManager::FindPlayer(uint64 Guid)
{
    FWowEntity* Entity = Find(Guid);
    return (Entity && Entity->GetEntityKind() == EWowEntityKind::Player)
        ? static_cast<FWowPlayerEntity*>(Entity)
        : nullptr;
}

FWowGameObjectEntity* FWowEntityManager::FindGameObject(uint64 Guid)
{
    FWowEntity* Entity = Find(Guid);
    return (Entity && Entity->GetEntityKind() == EWowEntityKind::GameObject)
        ? static_cast<FWowGameObjectEntity*>(Entity)
        : nullptr;
}

FWowDynamicObjectEntity* FWowEntityManager::FindDynamicObject(uint64 Guid)
{
    FWowEntity* Entity = Find(Guid);
    return (Entity && Entity->GetEntityKind() == EWowEntityKind::DynamicObject)
        ? static_cast<FWowDynamicObjectEntity*>(Entity)
        : nullptr;
}

FWowCorpseEntity* FWowEntityManager::FindCorpse(uint64 Guid)
{
    FWowEntity* Entity = Find(Guid);
    return (Entity && Entity->GetEntityKind() == EWowEntityKind::Corpse)
        ? static_cast<FWowCorpseEntity*>(Entity)
        : nullptr;
}

void FWowEntityManager::Remove(uint64 Guid)
{
    Entities.Remove(Guid);
    OnEntityDestroyed.Broadcast(Guid);
}

void FWowEntityManager::Clear()
{
    Entities.Empty();
}
