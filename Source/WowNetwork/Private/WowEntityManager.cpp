#include "WowEntityManager.h"

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

FWowEntity* FWowEntityManager::Find(uint64 Guid)
{
    TSharedPtr<FWowEntity>* Ptr = Entities.Find(Guid);
    return Ptr ? Ptr->Get() : nullptr;
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
