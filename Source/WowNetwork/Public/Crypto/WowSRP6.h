#pragma once
#include "CoreMinimal.h"
#include "Crypto/WowBigNumber.h"

class WOWNETWORK_API FWowSRP6
{
public:
    /**
     * Compute client credentials for SRP6 authentication.
     *
     * @param Username - uppercased account name
     * @param Password - uppercased password
     * @param B_bytes - server's public ephemeral (32 bytes, little-endian)
     * @param g_bytes - generator (variable length, little-endian)
     * @param N_bytes - prime modulus (32 bytes, little-endian)
     * @param s_bytes - salt (32 bytes)
     * @return true if successful (B is valid)
     */
    bool ComputeCredentials(
        const FString& Username,
        const FString& Password,
        const TArray<uint8>& B_bytes,
        const TArray<uint8>& g_bytes,
        const TArray<uint8>& N_bytes,
        const TArray<uint8>& s_bytes);

    /** Get client public ephemeral A (32 bytes, little-endian) */
    TArray<uint8> GetA() const;

    /** Get client proof M1 (20 bytes) */
    TArray<uint8> GetM1() const;

    /** Verify server proof M2 (20 bytes). Returns true if valid. */
    bool VerifyM2(const TArray<uint8>& ServerM2) const;

    /** Get session key K (40 bytes) */
    TArray<uint8> GetSessionKey() const;

private:
    /** SHA1 hash helper */
    static TArray<uint8> SHA1Hash(const TArray<uint8>& Data);
    static TArray<uint8> SHA1Hash(const uint8* Data, int32 Len);

    /** SHA1 interleave for session key computation */
    static TArray<uint8> SHA1Interleave(const TArray<uint8>& S_bytes);

    FWowBigNumber A;   // Client public ephemeral
    FWowBigNumber a;   // Client private ephemeral
    TArray<uint8> M1;  // Client proof
    TArray<uint8> M2Expected; // Expected server proof
    TArray<uint8> K;   // Session key (40 bytes)
};
