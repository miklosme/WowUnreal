#include "Crypto/WowAuthCrypt.h"

#include "OpenSSLIncludes.h"

// These are the seeds the SERVER uses.
// From the CLIENT perspective:
//   - To decrypt incoming data, we use the server's encrypt seed
//   - To encrypt outgoing data, we use the server's decrypt seed

static const uint8 ServerEncryptSeed[] = {
    0xCC, 0x98, 0xAE, 0x04, 0xE8, 0x97, 0xEA, 0xCA,
    0x12, 0xDD, 0xC0, 0x93, 0x42, 0x91, 0x53, 0x57
};

static const uint8 ServerDecryptSeed[] = {
    0xC2, 0xB3, 0x72, 0x3C, 0xC6, 0xAE, 0xD9, 0xB5,
    0x34, 0x3C, 0x53, 0xEE, 0x2F, 0x43, 0x67, 0xCE
};

void FWowAuthCrypt::ARC4Init(uint8* State, const uint8* Key, int32 KeyLen)
{
    for (int32 i = 0; i < 256; ++i)
    {
        State[i] = static_cast<uint8>(i);
    }

    uint8 j = 0;
    for (int32 i = 0; i < 256; ++i)
    {
        j = j + State[i] + Key[i % KeyLen];
        uint8 Temp = State[i];
        State[i] = State[j];
        State[j] = Temp;
    }
}

void FWowAuthCrypt::ARC4Process(uint8* State, uint8& I, uint8& J, uint8* Data, int32 Len)
{
    for (int32 n = 0; n < Len; ++n)
    {
        I = I + 1;
        J = J + State[I];

        uint8 Temp = State[I];
        State[I] = State[J];
        State[J] = Temp;

        Data[n] ^= State[(State[I] + State[J]) & 0xFF];
    }
}

void FWowAuthCrypt::Init(const TArray<uint8>& SessionKey)
{
    // Client decrypt key = HMAC-SHA1(ServerEncryptSeed, SessionKey)
    // (Server encrypts with this, so client decrypts with it)
    uint8 DecryptKey[20];
    unsigned int DecryptKeyLen = 20;
    HMAC(EVP_sha1(), ServerEncryptSeed, sizeof(ServerEncryptSeed),
         SessionKey.GetData(), SessionKey.Num(),
         DecryptKey, &DecryptKeyLen);

    // Client encrypt key = HMAC-SHA1(ServerDecryptSeed, SessionKey)
    // (Server decrypts with this, so client encrypts with it)
    uint8 EncryptKey[20];
    unsigned int EncryptKeyLen = 20;
    HMAC(EVP_sha1(), ServerDecryptSeed, sizeof(ServerDecryptSeed),
         SessionKey.GetData(), SessionKey.Num(),
         EncryptKey, &EncryptKeyLen);

    // Initialize ARC4 states
    ARC4Init(DecryptState, DecryptKey, 20);
    ARC4Init(EncryptState, EncryptKey, 20);

    // Drop first 1024 bytes (ARC4-drop1024)
    uint8 DropBuf[1024];
    FMemory::Memzero(DropBuf, sizeof(DropBuf));

    ARC4Process(DecryptState, DecryptI, DecryptJ, DropBuf, 1024);

    FMemory::Memzero(DropBuf, sizeof(DropBuf));
    ARC4Process(EncryptState, EncryptI, EncryptJ, DropBuf, 1024);

    bInitialized = true;
}

void FWowAuthCrypt::DecryptRecv(uint8* Data, int32 Len)
{
    check(bInitialized);
    ARC4Process(DecryptState, DecryptI, DecryptJ, Data, Len);
}

void FWowAuthCrypt::EncryptSend(uint8* Data, int32 Len)
{
    check(bInitialized);
    ARC4Process(EncryptState, EncryptI, EncryptJ, Data, Len);
}
