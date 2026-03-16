#include "WowWardenHandler.h"
#include "Crypto/OpenSSLIncludes.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowWarden, Log, All);

FWowWardenHandler::FWowWardenHandler()
{
}

void FWowWardenHandler::InitializeEncryption(const TArray<uint8>& SessionKey)
{
    WardenCrypto.Init(SessionKey);
    UE_LOG(LogWowWarden, Log, TEXT("Warden encryption initialized"));
}

void FWowWardenHandler::HandleWardenData(const TArray<uint8>& Data)
{
    if (Data.Num() < 1)
    {
        UE_LOG(LogWowWarden, Warning, TEXT("Warden packet too short: %d bytes"), Data.Num());
        return;
    }

    // Make a copy for decryption
    TArray<uint8> DecryptedData = Data;

    // Decrypt the packet if we have initialized encryption
    if (WardenCrypto.bInitialized)
    {
        WardenCrypto.Decrypt(DecryptedData.GetData(), DecryptedData.Num());
    }

    // First byte is the Warden opcode type
    EWardenOpcode OpType = static_cast<EWardenOpcode>(DecryptedData[0]);

    UE_LOG(LogWowWarden, Log, TEXT("Received Warden packet: type=0x%02X, size=%d bytes"),
           static_cast<uint8>(OpType), Data.Num());

    // Remove the opcode byte for handlers
    TArray<uint8> PacketData;
    if (DecryptedData.Num() > 1)
    {
        PacketData.Append(DecryptedData.GetData() + 1, DecryptedData.Num() - 1);
    }

    switch (OpType)
    {
        case EWardenOpcode::ModuleTransfer:
            HandleModuleTransfer(PacketData);
            break;
        case EWardenOpcode::ModuleHashRequest:
            HandleModuleHashRequest(PacketData);
            break;
        case EWardenOpcode::MemoryCheck:
            HandleMemoryCheck(PacketData);
            break;
        case EWardenOpcode::PageCheck:
            HandlePageCheck(PacketData);
            break;
        case EWardenOpcode::MPQCheck:
            HandleMPQCheck(PacketData);
            break;
        case EWardenOpcode::TimingCheck:
            HandleTimingCheck(PacketData);
            break;
        default:
            UE_LOG(LogWowWarden, Warning, TEXT("Unknown Warden opcode: 0x%02X"), static_cast<uint8>(OpType));
            // Send generic empty response
            TArray<uint8> EmptyResponse = { static_cast<uint8>(OpType) };
            SendWardenResponse(EmptyResponse);
            break;
    }
}

void FWowWardenHandler::HandleModuleTransfer(const TArray<uint8>& Data)
{
    UE_LOG(LogWowWarden, Log, TEXT("Warden module transfer: %d bytes"), Data.Num());

    // Store the module data
    LastModuleData = Data;
    bHasReceivedModule = true;

    // Send acknowledgment: opcode + empty response
    TArray<uint8> Response = { static_cast<uint8>(EWardenOpcode::ModuleTransfer) };
    SendWardenResponse(Response);
}

void FWowWardenHandler::HandleModuleHashRequest(const TArray<uint8>& Data)
{
    UE_LOG(LogWowWarden, Log, TEXT("Warden module hash request"));

    // Respond with dummy hash (16 bytes of zeros for MD5)
    TArray<uint8> Response;
    Response.Add(static_cast<uint8>(EWardenOpcode::ModuleHashRequest));

    // Add 16 bytes of dummy hash
    for (int32 i = 0; i < 16; ++i)
    {
        Response.Add(0x00);
    }

    SendWardenResponse(Response);
}

void FWowWardenHandler::HandleMemoryCheck(const TArray<uint8>& Data)
{
    UE_LOG(LogWowWarden, Log, TEXT("Warden memory check: %d bytes"), Data.Num());

    // Parse the memory check request
    if (Data.Num() < 1)
    {
        // Send empty response
        TArray<uint8> Response = { static_cast<uint8>(EWardenOpcode::MemoryCheck) };
        SendWardenResponse(Response);
        return;
    }

    uint8 CheckId = Data[0];

    // Basic response: check ID + result (0 = not found/failed)
    TArray<uint8> Response;
    Response.Add(static_cast<uint8>(EWardenOpcode::MemoryCheck));
    Response.Add(CheckId);
    Response.Add(0x00); // Result: 0 = not found/no match

    SendWardenResponse(Response);
}

void FWowWardenHandler::HandlePageCheck(const TArray<uint8>& Data)
{
    UE_LOG(LogWowWarden, Log, TEXT("Warden page check: %d bytes"), Data.Num());

    // Respond with dummy page data
    TArray<uint8> Response;
    Response.Add(static_cast<uint8>(EWardenOpcode::PageCheck));

    // Add some dummy page data (usually checksums)
    for (int32 i = 0; i < 20; ++i) // SHA1 size
    {
        Response.Add(0x00);
    }

    SendWardenResponse(Response);
}

void FWowWardenHandler::HandleMPQCheck(const TArray<uint8>& Data)
{
    UE_LOG(LogWowWarden, Log, TEXT("Warden MPQ check: %d bytes"), Data.Num());

    // Respond with dummy MPQ data
    TArray<uint8> Response;
    Response.Add(static_cast<uint8>(EWardenOpcode::MPQCheck));

    // Add dummy response
    Response.Add(0x00); // Result code: 0 = file not found/no match

    SendWardenResponse(Response);
}

void FWowWardenHandler::HandleTimingCheck(const TArray<uint8>& Data)
{
    UE_LOG(LogWowWarden, Log, TEXT("Warden timing check"));

    // Respond with current timestamp
    TArray<uint8> Response;
    Response.Add(static_cast<uint8>(EWardenOpcode::TimingCheck));

    // Add current time as uint32 (milliseconds since some epoch)
    uint32 CurrentTime = static_cast<uint32>(FPlatformTime::Seconds() * 1000.0);
    Response.Append(reinterpret_cast<const uint8*>(&CurrentTime), 4);

    SendWardenResponse(Response);
}

void FWowWardenHandler::SendWardenResponse(const TArray<uint8>& ResponseData)
{
    if (!OnSendResponse.IsBound())
    {
        UE_LOG(LogWowWarden, Error, TEXT("No send response delegate bound"));
        return;
    }

    TArray<uint8> EncryptedData = ResponseData;

    // Encrypt the response data if encryption is initialized
    if (WardenCrypto.bInitialized && EncryptedData.Num() > 0)
    {
        WardenCrypto.Encrypt(EncryptedData.GetData(), EncryptedData.Num());
    }

    // Send via CMSG_WARDEN_DATA (0x02E7)
    OnSendResponse.Execute(0x02E7, EncryptedData);

    UE_LOG(LogWowWarden, Log, TEXT("Sent Warden response: %d bytes"), ResponseData.Num());
}

// ── Warden Crypto Implementation ──────────────────────────────────────────────

void FWowWardenHandler::FWardenCrypto::Init(const TArray<uint8>& SessionKey)
{
    // Derive Warden keys from session key using known method
    // For simplicity, we'll use the session key directly but with different initialization

    // Initialize encrypt key (using session key with 0x38 XOR)
    TArray<uint8> EncryptKey = SessionKey;
    for (uint8& Byte : EncryptKey)
    {
        Byte ^= 0x38;
    }

    // Initialize decrypt key (using session key with 0x5C XOR)
    TArray<uint8> DecryptKey = SessionKey;
    for (uint8& Byte : DecryptKey)
    {
        Byte ^= 0x5C;
    }

    // Use first 16 bytes for keys
    const int32 KeyLength = FMath::Min(16, SessionKey.Num());

    ARC4Init(EncryptState, EncryptKey.GetData(), KeyLength);
    ARC4Init(DecryptState, DecryptKey.GetData(), KeyLength);

    EncryptI = EncryptJ = 0;
    DecryptI = DecryptJ = 0;

    bInitialized = true;
}

void FWowWardenHandler::FWardenCrypto::Decrypt(uint8* Data, int32 Length)
{
    if (bInitialized && Length > 0)
    {
        ARC4Process(DecryptState, DecryptI, DecryptJ, Data, Length);
    }
}

void FWowWardenHandler::FWardenCrypto::Encrypt(uint8* Data, int32 Length)
{
    if (bInitialized && Length > 0)
    {
        ARC4Process(EncryptState, EncryptI, EncryptJ, Data, Length);
    }
}

void FWowWardenHandler::FWardenCrypto::ARC4Init(uint8* State, const uint8* Key, int32 KeyLength)
{
    // Initialize state array
    for (int32 i = 0; i < 256; ++i)
    {
        State[i] = static_cast<uint8>(i);
    }

    // Key scheduling
    int32 j = 0;
    for (int32 i = 0; i < 256; ++i)
    {
        j = (j + State[i] + Key[i % KeyLength]) & 0xFF;

        // Swap State[i] and State[j]
        uint8 temp = State[i];
        State[i] = State[j];
        State[j] = temp;
    }
}

void FWowWardenHandler::FWardenCrypto::ARC4Process(uint8* State, uint8& I, uint8& J, uint8* Data, int32 Length)
{
    for (int32 k = 0; k < Length; ++k)
    {
        I = (I + 1) & 0xFF;
        J = (J + State[I]) & 0xFF;

        // Swap State[I] and State[J]
        uint8 temp = State[I];
        State[I] = State[J];
        State[J] = temp;

        // XOR with keystream byte
        Data[k] ^= State[(State[I] + State[J]) & 0xFF];
    }
}