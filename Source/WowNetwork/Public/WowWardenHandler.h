#pragma once
#include "CoreMinimal.h"

DECLARE_DELEGATE_TwoParams(FOnSendWardenResponse, uint32 /*Opcode*/, const TArray<uint8>& /*Data*/);

/**
 * Basic Warden anti-cheat response handler for WoW 3.3.5a
 *
 * This implementation provides minimal responses to avoid being kicked by servers.
 * It's not designed to pass comprehensive Warden checks, but to respond appropriately
 * to common Warden packets that would otherwise cause disconnection.
 */
class WOWNETWORK_API FWowWardenHandler
{
public:
    FWowWardenHandler();

    /** Handle incoming SMSG_WARDEN_DATA packet */
    void HandleWardenData(const TArray<uint8>& Data);

    /** Bind this to send CMSG_WARDEN_DATA responses back to server */
    FOnSendWardenResponse OnSendResponse;

    /** Initialize Warden encryption with session key */
    void InitializeEncryption(const TArray<uint8>& SessionKey);

private:
    // Warden packet types (first byte of decrypted packet)
    enum class EWardenOpcode : uint8
    {
        ModuleTransfer      = 0x00,  // Server sends module data
        ModuleHashRequest   = 0x01,  // Server requests module hash
        MemoryCheck         = 0x02,  // Server requests memory check
        PageCheck           = 0x03,  // Server requests page scan
        MPQCheck            = 0x04,  // Server requests MPQ file check
        TimingCheck         = 0x05   // Server requests timing data
    };

    // Response handlers for each Warden opcode type
    void HandleModuleTransfer(const TArray<uint8>& Data);
    void HandleModuleHashRequest(const TArray<uint8>& Data);
    void HandleMemoryCheck(const TArray<uint8>& Data);
    void HandlePageCheck(const TArray<uint8>& Data);
    void HandleMPQCheck(const TArray<uint8>& Data);
    void HandleTimingCheck(const TArray<uint8>& Data);

    /** Send a Warden response packet */
    void SendWardenResponse(const TArray<uint8>& ResponseData);

    /** ARC4 encryption for Warden packets - separate from session encryption */
    struct FWardenCrypto
    {
        uint8 EncryptState[256] = {};
        uint8 EncryptI = 0, EncryptJ = 0;
        uint8 DecryptState[256] = {};
        uint8 DecryptI = 0, DecryptJ = 0;
        bool bInitialized = false;

        void Init(const TArray<uint8>& SessionKey);
        void Decrypt(uint8* Data, int32 Length);
        void Encrypt(uint8* Data, int32 Length);

    private:
        static void ARC4Init(uint8* State, const uint8* Key, int32 KeyLength);
        static void ARC4Process(uint8* State, uint8& I, uint8& J, uint8* Data, int32 Length);
    } WardenCrypto;

    /** Track module data for acknowledgment */
    TArray<uint8> LastModuleData;
    bool bHasReceivedModule = false;
};