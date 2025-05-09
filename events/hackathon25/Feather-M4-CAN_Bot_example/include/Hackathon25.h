#ifndef HACKATHON25_H
#define HACKATHON25_H

#include <stdint.h>

enum CAN_MSGs
{
    Join = 0x100,
    Leave = 0x101,
    Player = 0x110,
    Rename = 0x500,
    Game = 0x040,
    GameAck = 0x120,
    State = 0x050,
    Move = 0x090,
    Die = 0x080,
    Finish = 0x070,
    Error = 0x020
};

enum DIR
{
    Up = 1,
    Right = 2,
    Down = 3,
    Left = 4
};

enum ERROR_CODE
{
    ERROR_INVALID_PLAYER_ID = 1,
    ERROR_UNALLOWED_RENAME = 2,
    ERROR_YOU_ARE_NOT_PLAYING = 3,
    WARNING_UNKNOWN_MOVE = 4
};

struct Player_info
{
    uint8_t id[5];
    uint8_t alive[5];
    uint8_t playing;
};

struct __attribute__((packed)) MSG_Join
{
    uint32_t HardwareID;
};

struct __attribute__((packed)) MSG_Player
{
    uint32_t HardwareID;
    uint8_t PlayerID;
};

struct __attribute__((packed)) MSG_Name
{
    uint8_t playerID;
    uint8_t size;
    char name[7];
};

struct __attribute__((packed)) MSG_Game
{
    uint8_t player1_ID;
    uint8_t player2_ID;
    uint8_t player3_ID;
    uint8_t player4_ID;
};

struct __attribute__((packed)) MSG_GameAck
{
    uint8_t player_ID;
};

struct __attribute__((packed)) MSG_Move
{
    uint8_t playerID;
    uint8_t direction;
};

struct __attribute__((packed)) MSG_Die
{
    uint8_t playerID;
};

struct __attribute__((packed)) MSG_Error
{
    uint8_t playerID;
    uint8_t error_code;
};

struct __attribute__((packed)) MSG_Finish
{
    uint8_t id1;
    uint8_t point1;
    uint8_t id2;
    uint8_t point2;
    uint8_t id3;
    uint8_t point3;
    uint8_t id4;
    uint8_t point4;
};

struct __attribute__((packed)) MSG_State
{
    uint8_t x1;
    uint8_t y1;
    uint8_t x2;
    uint8_t y2;
    uint8_t x3;
    uint8_t y3;
    uint8_t x4;
    uint8_t y4;
};

#endif