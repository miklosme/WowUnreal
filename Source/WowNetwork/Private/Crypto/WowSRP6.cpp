#include "Crypto/WowSRP6.h"

#include "OpenSSLIncludes.h"

TArray<uint8> FWowSRP6::SHA1Hash(const uint8* Data, int32 Len)
{
    TArray<uint8> Result;
    Result.SetNumUninitialized(SHA_DIGEST_LENGTH);
    SHA1(Data, Len, Result.GetData());
    return Result;
}

TArray<uint8> FWowSRP6::SHA1Hash(const TArray<uint8>& Data)
{
    return SHA1Hash(Data.GetData(), Data.Num());
}

TArray<uint8> FWowSRP6::SHA1Interleave(const TArray<uint8>& S_bytes)
{
    // S_bytes is in little-endian (WoW wire order), 32 bytes
    // Split into even and odd byte indices
    const int32 HalfLen = S_bytes.Num() / 2;

    TArray<uint8> Buf0, Buf1;
    Buf0.SetNumUninitialized(HalfLen);
    Buf1.SetNumUninitialized(HalfLen);
    for (int32 i = 0; i < HalfLen; ++i)
    {
        Buf0[i] = S_bytes[2 * i];
        Buf1[i] = S_bytes[2 * i + 1];
    }

    // Find position of first non-zero byte in S
    int32 p = 0;
    while (p < S_bytes.Num() && S_bytes[p] == 0)
    {
        ++p;
    }
    if (p & 1)
    {
        ++p; // skip one extra byte if p is odd
    }
    p /= 2; // offset into half-buffers

    // Hash each half starting from first non-zero position
    TArray<uint8> Hash0 = SHA1Hash(Buf0.GetData() + p, HalfLen - p);
    TArray<uint8> Hash1 = SHA1Hash(Buf1.GetData() + p, HalfLen - p);

    // Interleave the two hashes into 40-byte session key
    TArray<uint8> Result;
    Result.SetNumUninitialized(SHA_DIGEST_LENGTH * 2);
    for (int32 i = 0; i < SHA_DIGEST_LENGTH; ++i)
    {
        Result[2 * i] = Hash0[i];
        Result[2 * i + 1] = Hash1[i];
    }
    return Result;
}

bool FWowSRP6::ComputeCredentials(
    const FString& Username,
    const FString& Password,
    const TArray<uint8>& B_bytes,
    const TArray<uint8>& g_bytes,
    const TArray<uint8>& N_bytes,
    const TArray<uint8>& s_bytes)
{
    // All BigNumbers: SetBinary takes little-endian (WoW wire format)
    FWowBigNumber B, g, N, s;
    B.SetBinary(B_bytes);
    g.SetBinary(g_bytes);
    N.SetBinary(N_bytes);
    s.SetBinary(s_bytes);

    // Check B % N != 0
    FWowBigNumber BmodN = B % N;
    if (BmodN.IsZero())
    {
        return false;
    }

    // 1. Generate random a (19 bytes = 152 bits)
    a.SetRand(19 * 8);

    // 2. A = g^a mod N
    A = FWowBigNumber::ModExp(g, a, N);

    // Check A % N != 0
    FWowBigNumber AmodN = A % N;
    if (AmodN.IsZero())
    {
        return false;
    }

    // Get A and B as 32-byte little-endian arrays
    TArray<uint8> A_bytes = A.GetBytes(32);
    // B_bytes is already the wire format

    // 3. u = SHA1(A || B) — both as 32-byte little-endian
    TArray<uint8> AB_concat;
    AB_concat.Append(A_bytes);
    AB_concat.Append(B_bytes);
    TArray<uint8> u_hash = SHA1Hash(AB_concat);
    FWowBigNumber u;
    u.SetBinary(u_hash);

    // 4. x = SHA1(s || SHA1(USERNAME:PASSWORD))
    // USERNAME and PASSWORD must be uppercased
    FString UpperUser = Username.ToUpper();
    FString UpperPass = Password.ToUpper();
    FString Credentials = UpperUser + TEXT(":") + UpperPass;

    // Convert to UTF-8 bytes for hashing
    FTCHARToUTF8 CredUtf8(*Credentials);
    TArray<uint8> CredHash = SHA1Hash(reinterpret_cast<const uint8*>(CredUtf8.Get()), CredUtf8.Length());

    // x = SHA1(s || credHash) — s is 32 bytes in wire format (little-endian)
    TArray<uint8> sx_concat;
    sx_concat.Append(s_bytes);
    sx_concat.Append(CredHash);
    TArray<uint8> x_hash = SHA1Hash(sx_concat);
    FWowBigNumber x;
    x.SetBinary(x_hash);

    // 5. S = (B - 3 * g^x mod N) ^ (a + u*x) mod N
    FWowBigNumber gx = FWowBigNumber::ModExp(g, x, N);
    FWowBigNumber Three(3u);
    FWowBigNumber ThreeGx = (Three * gx) % N;

    // Need to handle B - 3*g^x which might go negative; add N to keep positive
    FWowBigNumber Diff = (B + N - ThreeGx) % N;

    FWowBigNumber Exponent = a + u * x;
    FWowBigNumber S = FWowBigNumber::ModExp(Diff, Exponent, N);

    // 6. Session key K = SHA1Interleave(S)
    TArray<uint8> S_le = S.GetBytes(32);
    K = SHA1Interleave(S_le);

    // 7. M1 = SHA1(H(N) xor H(g), H(username), s, A, B, K)
    // H(N) = SHA1(N as little-endian bytes)
    TArray<uint8> N_le = N.GetBytes(32);
    TArray<uint8> NHash = SHA1Hash(N_le);

    // H(g) = SHA1(g as little-endian bytes)
    // g is typically 1 byte (value 7)
    TArray<uint8> g_le = g.GetBytes(1);
    TArray<uint8> gHash = SHA1Hash(g_le);

    // XOR NHash and gHash
    TArray<uint8> NgHash;
    NgHash.SetNumUninitialized(SHA_DIGEST_LENGTH);
    for (int32 i = 0; i < SHA_DIGEST_LENGTH; ++i)
    {
        NgHash[i] = NHash[i] ^ gHash[i];
    }

    // H(username) = SHA1(uppercased username)
    FTCHARToUTF8 UserUtf8(*UpperUser);
    TArray<uint8> UserHash = SHA1Hash(reinterpret_cast<const uint8*>(UserUtf8.Get()), UserUtf8.Length());

    // M1 = SHA1(NgHash, UserHash, s, A, B, K)
    TArray<uint8> M1_concat;
    M1_concat.Append(NgHash);
    M1_concat.Append(UserHash);
    M1_concat.Append(s_bytes);
    M1_concat.Append(A_bytes);
    M1_concat.Append(B_bytes);
    M1_concat.Append(K);
    M1 = SHA1Hash(M1_concat);

    // 8. M2 = SHA1(A, M1, K) for later verification
    TArray<uint8> M2_concat;
    M2_concat.Append(A_bytes);
    M2_concat.Append(M1);
    M2_concat.Append(K);
    M2Expected = SHA1Hash(M2_concat);

    return true;
}

TArray<uint8> FWowSRP6::GetA() const
{
    return A.GetBytes(32);
}

TArray<uint8> FWowSRP6::GetM1() const
{
    return M1;
}

bool FWowSRP6::VerifyM2(const TArray<uint8>& ServerM2) const
{
    if (ServerM2.Num() != M2Expected.Num())
    {
        return false;
    }
    return FMemory::Memcmp(ServerM2.GetData(), M2Expected.GetData(), M2Expected.Num()) == 0;
}

TArray<uint8> FWowSRP6::GetSessionKey() const
{
    return K;
}
