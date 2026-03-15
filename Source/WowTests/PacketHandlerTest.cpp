#include "CoreMinimal.h"
#include "WowPacketHandler.h"
#include "WowOpcodes.h"

// Simple test to verify packet handlers compile and basic functionality
void TestPacketHandlers()
{
    FWowPacketHandler Handler;

    // Test SMSG_SPELL_START
    {
        TArray<uint8> TestData;
        // Packed GUID (caster item) - simple GUID with mask 0x01, GUID low byte 0x42
        TestData.Add(0x01); // mask
        TestData.Add(0x42); // guid low byte

        // Packed GUID (caster) - same format
        TestData.Add(0x01); // mask
        TestData.Add(0x43); // guid low byte

        // Cast counter
        TestData.Add(0x01);

        // Spell ID (uint32) = 133 (fireball)
        TestData.Add(133); TestData.Add(0); TestData.Add(0); TestData.Add(0);

        // Cast flags (uint32)
        TestData.Add(0); TestData.Add(0); TestData.Add(0); TestData.Add(0);

        // Cast time (uint32) = 3000ms
        TestData.Add(0xB8); TestData.Add(0x0B); TestData.Add(0); TestData.Add(0);

        Handler.HandlePacket(WowOpcode::SMSG_SPELL_START, TestData);
    }

    // Test SMSG_POWER_UPDATE
    {
        TArray<uint8> TestData;
        // Packed GUID (target) - player GUID
        TestData.Add(0x01); // mask
        TestData.Add(0x44); // guid low byte

        // Power type (uint8) = 0 (mana)
        TestData.Add(0x00);

        // Power value (uint32) = 1500
        TestData.Add(0xDC); TestData.Add(0x05); TestData.Add(0); TestData.Add(0);

        Handler.HandlePacket(WowOpcode::SMSG_POWER_UPDATE, TestData);
    }

    // Test SMSG_AURA_UPDATE
    {
        TArray<uint8> TestData;
        // Packed GUID (target)
        TestData.Add(0x01); // mask
        TestData.Add(0x45); // guid low byte

        // Slot = 0
        TestData.Add(0x00);

        // Spell ID (uint32) = 1234 (blessing of might)
        TestData.Add(0xD2); TestData.Add(0x04); TestData.Add(0); TestData.Add(0);

        // Flags = 0x08 (has caster)
        TestData.Add(0x08);

        // Level = 60
        TestData.Add(60);

        // Charges = 0
        TestData.Add(0);

        // Caster GUID (packed)
        TestData.Add(0x01); // mask
        TestData.Add(0x46); // guid low byte

        Handler.HandlePacket(WowOpcode::SMSG_AURA_UPDATE, TestData);
    }

    UE_LOG(LogTemp, Warning, TEXT("Packet handler tests completed"));
}

// Auto-run test when this file is compiled
static struct FPacketHandlerTestRunner
{
    FPacketHandlerTestRunner()
    {
        // Delay test execution to ensure everything is initialized
        FTimerHandle TimerHandle;
        if (GEngine && GWorld)
        {
            GWorld->GetTimerManager().SetTimer(TimerHandle, []()
            {
                TestPacketHandlers();
            }, 1.0f, false);
        }
    }
} GPacketHandlerTestRunner;