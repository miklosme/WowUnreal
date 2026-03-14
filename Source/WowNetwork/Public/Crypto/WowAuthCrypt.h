#pragma once
#include "CoreMinimal.h"

class WOWNETWORK_API FWowAuthCrypt
{
public:
    /** Initialize encryption with session key K (40 bytes) */
    void Init(const TArray<uint8>& SessionKey);

    /** Decrypt server packet header (4 bytes: 2 size + 2 opcode) */
    void DecryptRecv(uint8* Data, int32 Len);

    /** Encrypt client packet header (6 bytes: 2 size + 4 opcode) */
    void EncryptSend(uint8* Data, int32 Len);

    bool IsInitialized() const { return bInitialized; }

private:
    uint8 DecryptState[256] = {};
    uint8 DecryptI = 0, DecryptJ = 0;

    uint8 EncryptState[256] = {};
    uint8 EncryptI = 0, EncryptJ = 0;

    bool bInitialized = false;

    /** ARC4 process bytes in-place */
    static void ARC4Process(uint8* State, uint8& I, uint8& J, uint8* Data, int32 Len);

    /** ARC4 key schedule */
    static void ARC4Init(uint8* State, const uint8* Key, int32 KeyLen);
};
