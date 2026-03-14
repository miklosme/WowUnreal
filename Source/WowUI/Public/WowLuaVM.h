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
    /** Save all addon SavedVariables to disk */
    void SaveAllVariables();
    bool ExecuteString(const FString& Code, const FString& ChunkName = TEXT("="));
    bool ExecuteBuffer(const TArray<uint8>& Buffer, const FString& ChunkName);
    void FireEvent(const FString& EventName, const TArray<FString>& Args = {});
    lua_State* GetState() const { return L; }
    bool IsInitialized() const { return L != nullptr; }

    /** Memory limit in bytes (default 128 MB, 0 = unlimited) */
    void SetMemoryLimit(SIZE_T LimitBytes) { MemoryLimit = LimitBytes; }
    SIZE_T GetMemoryUsage() const { return MemoryUsed; }
    SIZE_T GetMemoryLimit() const { return MemoryLimit; }

    /** Execution timeout in instructions per script call (default 10M, 0 = unlimited) */
    void SetInstructionLimit(int32 Limit) { InstructionLimit = Limit; }
    int32 GetInstructionLimit() const { return InstructionLimit; }

private:
    void RegisterWowApi();
    void SandboxGlobals();
    void InstallInstructionHook();

    lua_State* L = nullptr;

    // Memory sandbox
    SIZE_T MemoryUsed = 0;
    SIZE_T MemoryLimit = 128 * 1024 * 1024; // 128 MB default

    // Execution timeout
    int32 InstructionLimit = 10000000; // 10M instructions default

    static void* LuaAlloc(void* ud, void* ptr, size_t osize, size_t nsize);
};
