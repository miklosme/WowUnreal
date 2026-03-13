#pragma once
#include "CoreMinimal.h"

struct lua_State;

class WOWUI_API FWowLuaVM
{
public:
    FWowLuaVM();
    ~FWowLuaVM();
    bool Initialize();
    void Shutdown();
    bool ExecuteString(const FString& Code, const FString& ChunkName = TEXT("="));
    bool ExecuteBuffer(const TArray<uint8>& Buffer, const FString& ChunkName);
    void FireEvent(const FString& EventName, const TArray<FString>& Args = {});
    lua_State* GetState() const { return L; }
    bool IsInitialized() const { return L != nullptr; }
private:
    void RegisterWowApi();
    void SandboxGlobals();
    lua_State* L = nullptr;
};
