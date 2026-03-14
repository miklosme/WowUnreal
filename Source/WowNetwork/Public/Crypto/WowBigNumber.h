#pragma once
#include "CoreMinimal.h"

struct bignum_st;
typedef struct bignum_st BIGNUM;

class WOWNETWORK_API FWowBigNumber
{
public:
    FWowBigNumber();
    FWowBigNumber(uint32 Value);
    FWowBigNumber(const FWowBigNumber& Other);
    FWowBigNumber(FWowBigNumber&& Other) noexcept;
    ~FWowBigNumber();

    FWowBigNumber& operator=(const FWowBigNumber& Other);
    FWowBigNumber& operator=(FWowBigNumber&& Other) noexcept;

    /** Set from little-endian byte array (WoW wire format) */
    void SetBinary(const uint8* Data, int32 Len);
    void SetBinary(const TArray<uint8>& Data);
    void SetRand(int32 NumBits);

    /** Get as little-endian byte array (WoW wire format), padded to MinSize */
    TArray<uint8> GetBytes(int32 MinSize = 0) const;
    int32 GetNumBytes() const;
    bool IsZero() const;

    static FWowBigNumber ModExp(const FWowBigNumber& Base, const FWowBigNumber& Exp, const FWowBigNumber& Mod);

    FWowBigNumber operator+(const FWowBigNumber& Other) const;
    FWowBigNumber operator-(const FWowBigNumber& Other) const;
    FWowBigNumber operator*(const FWowBigNumber& Other) const;
    FWowBigNumber operator%(const FWowBigNumber& Other) const;

    BIGNUM* GetBN() const { return BN; }

private:
    BIGNUM* BN;
};
