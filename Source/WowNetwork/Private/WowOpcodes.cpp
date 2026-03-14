#include "WowOpcodes.h"

const TCHAR* WowOpcode::GetName(uint16 Opcode)
{
    switch (Opcode)
    {
    case SMSG_UPDATE_OBJECT:            return TEXT("SMSG_UPDATE_OBJECT");
    case SMSG_COMPRESSED_UPDATE_OBJECT: return TEXT("SMSG_COMPRESSED_UPDATE_OBJECT");
    case SMSG_DESTROY_OBJECT:           return TEXT("SMSG_DESTROY_OBJECT");
    case SMSG_LOGIN_VERIFY_WORLD:       return TEXT("SMSG_LOGIN_VERIFY_WORLD");
    case SMSG_MESSAGECHAT:              return TEXT("SMSG_MESSAGECHAT");
    case SMSG_INITIAL_SPELLS:           return TEXT("SMSG_INITIAL_SPELLS");
    case SMSG_ACTION_BUTTONS:           return TEXT("SMSG_ACTION_BUTTONS");
    case SMSG_AUTH_CHALLENGE:           return TEXT("SMSG_AUTH_CHALLENGE");
    case SMSG_AUTH_RESPONSE:            return TEXT("SMSG_AUTH_RESPONSE");
    case SMSG_CHAR_ENUM:                return TEXT("SMSG_CHAR_ENUM");
    case SMSG_MOTD:                     return TEXT("SMSG_MOTD");
    case SMSG_TIME_SYNC_REQ:            return TEXT("SMSG_TIME_SYNC_REQ");
    case SMSG_TUTORIAL_FLAGS:           return TEXT("SMSG_TUTORIAL_FLAGS");
    case SMSG_ACCOUNT_DATA_TIMES:       return TEXT("SMSG_ACCOUNT_DATA_TIMES");
    case SMSG_SET_PROFICIENCY:          return TEXT("SMSG_SET_PROFICIENCY");
    case SMSG_POWER_UPDATE:             return TEXT("SMSG_POWER_UPDATE");
    case SMSG_AURA_UPDATE:              return TEXT("SMSG_AURA_UPDATE");
    case SMSG_ADDON_INFO:               return TEXT("SMSG_ADDON_INFO");
    case SMSG_WARDEN_DATA:              return TEXT("SMSG_WARDEN_DATA");
    case SMSG_MONSTER_MOVE:             return TEXT("SMSG_MONSTER_MOVE");
    case SMSG_SPELL_START:              return TEXT("SMSG_SPELL_START");
    case SMSG_SPELL_GO:                 return TEXT("SMSG_SPELL_GO");
    case MSG_MOVE_HEARTBEAT:            return TEXT("MSG_MOVE_HEARTBEAT");
    case CMSG_PLAYER_LOGIN:             return TEXT("CMSG_PLAYER_LOGIN");
    case CMSG_CHAR_ENUM:                return TEXT("CMSG_CHAR_ENUM");
    case CMSG_KEEP_ALIVE:               return TEXT("CMSG_KEEP_ALIVE");
    default:                            return TEXT("UNKNOWN");
    }
}
