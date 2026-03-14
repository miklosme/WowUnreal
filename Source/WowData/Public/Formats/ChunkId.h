#pragma once
#include "CoreMinimal.h"

// Build a FourCC uint32 from reversed char order (as WoW stores chunk IDs on disk).
// E.g. MakeFourCC('R','E','V','M') produces the LE uint32 for "MVER".
inline constexpr uint32 MakeFourCC(char A, char B, char C, char D)
{
    return static_cast<uint32>(A)
        | (static_cast<uint32>(B) << 8)
        | (static_cast<uint32>(C) << 16)
        | (static_cast<uint32>(D) << 24);
}
