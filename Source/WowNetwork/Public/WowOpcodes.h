#pragma once
#include "CoreMinimal.h"

// WoW 3.3.5a (build 12340) opcodes — subset used by the client
// Reference: azerothcore-wotlk/src/server/game/Server/Protocol/Opcodes.h
namespace WowOpcode
{
    // ── Client → Server ──────────────────────────────────────────────────────
    inline constexpr uint16 CMSG_CHAR_ENUM              = 0x0037;
    inline constexpr uint16 CMSG_PLAYER_LOGIN           = 0x003D;
    inline constexpr uint16 CMSG_SET_SELECTION            = 0x013D;
    inline constexpr uint16 CMSG_MESSAGECHAT            = 0x0095;
    inline constexpr uint16 CMSG_CAST_SPELL             = 0x012E;
    inline constexpr uint16 CMSG_ATTACKSWING            = 0x0141;
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
    inline constexpr uint16 SMSG_AURA_UPDATE             = 0x0496;
    inline constexpr uint16 SMSG_CHAR_ENUM               = 0x003B;

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
