#ifndef HACKATHON25_H
#define HACKATHON25_H

#include <stdint.h>

enum CAN_MSGs {
    Join = 0x100,
    Leave = 0x101,
    Player = 0x110,
    Game = 0x040,
    GameAck = 0x120,

};

struct __attribute__((packed)) MSG_Join {
    uint32_t HardwareID;
};

struct __attribute__((packed)) MSG_Player {
    uint32_t HardwareID;
    uint8_t PlayerID;
};

struct __attribute__((packed)) MSG_Game {
    uint8_t player1_ID;
    uint8_t player2_ID;
    uint8_t player3_ID;
    uint8_t player4_ID;
};

struct __attribute__((packed)) MSG_GameAck {
    uint8_t player_ID;
};


#endif