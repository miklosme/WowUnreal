#pragma once
#include "CoreMinimal.h"

// WoW 3.3.5a (build 12340) opcodes — subset used by the client
// Reference: azerothcore-wotlk/src/server/game/Server/Protocol/Opcodes.h
namespace WowOpcode
{
    // ── Client → Server ──────────────────────────────────────────────────────
    inline constexpr uint16 CMSG_CHAR_CREATE            = 0x0036;
    inline constexpr uint16 CMSG_CHAR_ENUM              = 0x0037;
    inline constexpr uint16 CMSG_CHAR_DELETE            = 0x0038;
    inline constexpr uint16 CMSG_PLAYER_LOGIN           = 0x003D;
    inline constexpr uint16 CMSG_SET_SELECTION            = 0x013D;
    inline constexpr uint16 CMSG_MESSAGECHAT            = 0x0095;
    inline constexpr uint16 CMSG_CAST_SPELL             = 0x012E;
    inline constexpr uint16 CMSG_ATTACKSWING            = 0x0141;
    inline constexpr uint16 CMSG_ATTACKSTOP             = 0x0142;
    inline constexpr uint16 CMSG_AUTH_SESSION            = 0x01ED;
    inline constexpr uint16 CMSG_SET_ACTIVE_MOVER       = 0x026A;
    inline constexpr uint16 CMSG_MOVE_TIME_SKIPPED      = 0x02CE;
    inline constexpr uint16 CMSG_TIME_SYNC_RESP          = 0x0391;
    inline constexpr uint16 CMSG_KEEP_ALIVE             = 0x0406;
    inline constexpr uint16 CMSG_WORLD_STATE_UI_TIMER_UPDATE = 0x04F6;

    // ── Movement (bidirectional MSG) ─────────────────────────────────────────
    inline constexpr uint16 MSG_MOVE_START_FORWARD       = 0x00B5;
    inline constexpr uint16 MSG_MOVE_START_BACKWARD      = 0x00B6;
    inline constexpr uint16 MSG_MOVE_STOP                = 0x00B7;
    inline constexpr uint16 MSG_MOVE_START_STRAFE_LEFT   = 0x00B8;
    inline constexpr uint16 MSG_MOVE_START_STRAFE_RIGHT  = 0x00B9;
    inline constexpr uint16 MSG_MOVE_STOP_STRAFE         = 0x00BA;
    inline constexpr uint16 MSG_MOVE_JUMP                = 0x00BB;
    inline constexpr uint16 MSG_MOVE_START_TURN_LEFT     = 0x00BC;
    inline constexpr uint16 MSG_MOVE_START_TURN_RIGHT    = 0x00BD;
    inline constexpr uint16 MSG_MOVE_STOP_TURN           = 0x00BE;
    inline constexpr uint16 MSG_MOVE_START_PITCH_UP      = 0x00BF;
    inline constexpr uint16 MSG_MOVE_START_PITCH_DOWN    = 0x00C0;
    inline constexpr uint16 MSG_MOVE_STOP_PITCH          = 0x00C1;
    inline constexpr uint16 MSG_MOVE_SET_RUN_MODE        = 0x00C2;
    inline constexpr uint16 MSG_MOVE_SET_WALK_MODE       = 0x00C3;
    inline constexpr uint16 MSG_MOVE_TELEPORT            = 0x00C5;
    inline constexpr uint16 MSG_MOVE_TELEPORT_ACK        = 0x00C7;
    inline constexpr uint16 MSG_MOVE_FALL_LAND           = 0x00C9;
    inline constexpr uint16 MSG_MOVE_START_SWIM          = 0x00CA;
    inline constexpr uint16 MSG_MOVE_STOP_SWIM           = 0x00CB;
    inline constexpr uint16 MSG_MOVE_SET_RUN_SPEED       = 0x00CD;
    inline constexpr uint16 MSG_MOVE_SET_RUN_BACK_SPEED  = 0x00CF;
    inline constexpr uint16 MSG_MOVE_SET_WALK_SPEED      = 0x00D1;
    inline constexpr uint16 MSG_MOVE_SET_SWIM_SPEED      = 0x00D3;
    inline constexpr uint16 MSG_MOVE_SET_SWIM_BACK_SPEED = 0x00D5;
    inline constexpr uint16 MSG_MOVE_SET_TURN_RATE       = 0x00D8;
    inline constexpr uint16 MSG_MOVE_SET_FACING          = 0x00DA;
    inline constexpr uint16 MSG_MOVE_SET_PITCH           = 0x00DB;
    inline constexpr uint16 MSG_MOVE_WORLDPORT_ACK       = 0x00DC;
    inline constexpr uint16 MSG_MOVE_ROOT                = 0x00EC;
    inline constexpr uint16 MSG_MOVE_UNROOT              = 0x00ED;
    inline constexpr uint16 MSG_MOVE_HEARTBEAT           = 0x00EE;

    // ── Server → Client ──────────────────────────────────────────────────────
    inline constexpr uint16 SMSG_MESSAGECHAT             = 0x0096;
    inline constexpr uint16 SMSG_UPDATE_OBJECT           = 0x00A9;
    inline constexpr uint16 SMSG_DESTROY_OBJECT          = 0x00AA;
    inline constexpr uint16 SMSG_MONSTER_MOVE            = 0x00DD;
    inline constexpr uint16 SMSG_TUTORIAL_FLAGS          = 0x00FD;
    inline constexpr uint16 SMSG_ACTION_BUTTONS          = 0x0129;
    inline constexpr uint16 SMSG_INITIAL_SPELLS          = 0x012A;
    inline constexpr uint16 SMSG_SET_PROFICIENCY         = 0x0127;
    inline constexpr uint16 SMSG_SPELL_START             = 0x0131;
    inline constexpr uint16 SMSG_SPELL_GO                = 0x0132;
    inline constexpr uint16 SMSG_AUTH_CHALLENGE          = 0x01EC;
    inline constexpr uint16 SMSG_AUTH_RESPONSE           = 0x01EE;
    inline constexpr uint16 SMSG_COMPRESSED_UPDATE_OBJECT = 0x01F6;
    inline constexpr uint16 SMSG_ACCOUNT_DATA_TIMES      = 0x0209;
    inline constexpr uint16 SMSG_LOGIN_VERIFY_WORLD      = 0x0236;
    inline constexpr uint16 SMSG_ADDON_INFO              = 0x02EF;
    inline constexpr uint16 SMSG_WARDEN_DATA             = 0x02E6;
    inline constexpr uint16 SMSG_MOTD                    = 0x033D;
    inline constexpr uint16 SMSG_TIME_SYNC_REQ           = 0x0390;
    inline constexpr uint16 SMSG_POWER_UPDATE            = 0x0480;
    inline constexpr uint16 SMSG_AURA_UPDATE             = 0x0495;
    inline constexpr uint16 SMSG_CHAR_CREATE              = 0x003A;
    inline constexpr uint16 SMSG_CHAR_ENUM               = 0x003B;
    inline constexpr uint16 SMSG_CHAR_DELETE              = 0x003C;

    // Inventory system opcodes
    inline constexpr uint16 CMSG_AUTOEQUIP_ITEM          = 0x010A;
    inline constexpr uint16 CMSG_SWAP_INV_ITEM           = 0x010C;
    inline constexpr uint16 CMSG_SWAP_ITEM               = 0x010D;
    inline constexpr uint16 CMSG_DESTROYITEM             = 0x0111;
    inline constexpr uint16 SMSG_INVENTORY_CHANGE_FAILURE = 0x0112;
    inline constexpr uint16 CMSG_LOOT                    = 0x015D;
    inline constexpr uint16 CMSG_LOOT_RELEASE            = 0x015E;
    inline constexpr uint16 SMSG_LOOT_RESPONSE           = 0x0160;
    inline constexpr uint16 SMSG_LOOT_RELEASE_RESPONSE   = 0x0161;
    inline constexpr uint16 SMSG_ITEM_PUSH_RESULT        = 0x0166;
    inline constexpr uint16 SMSG_SELL_ITEM               = 0x01A1;
    inline constexpr uint16 SMSG_BUY_ITEM                = 0x01A2;

    // Quest system opcodes
    inline constexpr uint16 CMSG_QUESTGIVER_HELLO        = 0x0182;
    inline constexpr uint16 SMSG_QUESTGIVER_STATUS       = 0x0183;
    inline constexpr uint16 SMSG_QUESTGIVER_QUEST_LIST   = 0x0184;
    inline constexpr uint16 SMSG_QUESTGIVER_QUEST_DETAILS = 0x0188;
    inline constexpr uint16 CMSG_QUESTGIVER_ACCEPT_QUEST = 0x0189;
    inline constexpr uint16 CMSG_QUESTGIVER_COMPLETE_QUEST = 0x018A;
    inline constexpr uint16 SMSG_QUESTGIVER_REQUEST_ITEMS = 0x018B;
    inline constexpr uint16 SMSG_QUESTGIVER_QUEST_COMPLETE = 0x018C;
    inline constexpr uint16 SMSG_QUESTGIVER_OFFER_REWARD = 0x018D;
    inline constexpr uint16 CMSG_QUESTGIVER_CHOOSE_REWARD = 0x018E;
    inline constexpr uint16 SMSG_QUESTLOG_FULL           = 0x0195;
    inline constexpr uint16 SMSG_QUEST_UPDATE_COMPLETE   = 0x0198;
    inline constexpr uint16 SMSG_QUEST_UPDATE_ADD_KILL   = 0x0199;
    inline constexpr uint16 CMSG_QUEST_QUERY             = 0x005C;
    inline constexpr uint16 SMSG_QUEST_QUERY_RESPONSE    = 0x005D;

    // Talent system opcodes
    inline constexpr uint16 SMSG_LEARNED_SPELL           = 0x012B;
    inline constexpr uint16 SMSG_REMOVED_SPELL           = 0x0203;
    inline constexpr uint16 CMSG_LEARN_TALENT            = 0x0251;
    inline constexpr uint16 SMSG_TALENTS_INFO            = 0x04C0;
    inline constexpr uint16 CMSG_LEARN_TALENTS_MULTIPLE  = 0x04C1;

    // ── Social / Guild / Friends ────────────────────────────────────────────────
    inline constexpr uint16 SMSG_FRIEND_LIST             = 0x0067;
    inline constexpr uint16 SMSG_FRIEND_STATUS           = 0x0068;
    inline constexpr uint16 CMSG_FRIEND_LIST             = 0x0066;
    inline constexpr uint16 CMSG_ADD_FRIEND              = 0x0069;
    inline constexpr uint16 CMSG_DEL_FRIEND              = 0x006A;
    inline constexpr uint16 CMSG_ADD_IGNORE              = 0x006C;
    inline constexpr uint16 CMSG_DEL_IGNORE              = 0x006D;
    inline constexpr uint16 SMSG_IGNORE_LIST             = 0x006B;
    inline constexpr uint16 SMSG_GUILD_ROSTER            = 0x008A;
    inline constexpr uint16 SMSG_GUILD_EVENT             = 0x0092;
    inline constexpr uint16 CMSG_GUILD_ROSTER            = 0x0089;
    inline constexpr uint16 SMSG_CHANNEL_NOTIFY          = 0x0099;
    inline constexpr uint16 SMSG_WHO                     = 0x0063;
    inline constexpr uint16 CMSG_WHO                     = 0x0062;
    inline constexpr uint16 SMSG_GROUP_LIST              = 0x007D;
    inline constexpr uint16 SMSG_PARTY_COMMAND_RESULT    = 0x007F;
    inline constexpr uint16 CMSG_GROUP_INVITE            = 0x006E;
    inline constexpr uint16 CMSG_GROUP_ACCEPT            = 0x0072;

    // ── Death / Corpse / Resurrection ────────────────────────────────────────
    inline constexpr uint16 CMSG_REPOP_REQUEST          = 0x015A;
    inline constexpr uint16 SMSG_CORPSE_RECLAIM_DELAY   = 0x015B;
    inline constexpr uint16 SMSG_RESURRECT_REQUEST      = 0x015C;
    inline constexpr uint16 CMSG_RESURRECT_RESPONSE     = 0x015F;
    inline constexpr uint16 SMSG_MOVE_TELEPORT          = 0x02F0;

    // Auth result codes
    inline constexpr uint8 AUTH_OK = 0x0C;

    // Helper to check if an opcode is a movement MSG
    inline bool IsMovementOpcode(uint16 Op)
    {
        return (Op >= MSG_MOVE_START_FORWARD && Op <= MSG_MOVE_SET_PITCH)
            || Op == MSG_MOVE_ROOT || Op == MSG_MOVE_UNROOT || Op == MSG_MOVE_HEARTBEAT;
    }

    // Opcode name for logging
    const TCHAR* GetName(uint16 Opcode);
}
