#pragma once
#include "CoreMinimal.h"
#include "Formats/DbcParser.h"

struct FChrRacesDbcEntry
{
    uint32 ID = 0;                     // 0
    uint32 Flags = 0;                  // 1
    uint32 FactionID = 0;              // 2
    uint32 ExplorationSoundID = 0;     // 3
    uint32 MaleModelID = 0;            // 4
    uint32 FemaleModelID = 0;          // 5
    FString ClientPrefix;              // 6
    uint32 BaseLanguage = 0;           // 7
    uint32 CreatureTypeID = 0;         // 8
    uint32 ResSicknessSpellID = 0;     // 9
    uint32 SplashSoundID = 0;          // 10
    FString ClientFileString;          // 11
    uint32 CinematicSequenceID = 0;    // 12
    uint32 FactionGroup = 0;           // 13
    FString Name;                      // 14 (neutral)
    FString FemaleName;                // 31
    FString MaleName;                  // 48
    FString FacialHairCustomization1;  // 65
    FString FacialHairCustomization2;  // 66
    FString HairCustomization;         // 67
    uint32 RequiredExpansion = 0;      // 68
};

class WOWDATA_API FChrRacesDbc
{
public:
    bool Load(const FDbcParser& Parser);
    const FChrRacesDbcEntry* GetById(uint32 ID) const;
    const TArray<FChrRacesDbcEntry>& GetAll() const { return Entries; }
    int32 Num() const { return Entries.Num(); }

private:
    TArray<FChrRacesDbcEntry> Entries;
    TMap<uint32, int32> IdIndex;
};
