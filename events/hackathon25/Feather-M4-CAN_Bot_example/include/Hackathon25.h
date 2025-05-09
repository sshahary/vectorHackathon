#ifndef HACKATHON25_H
#define HACKATHON25_H

#include <stdint.h>
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif
#define PIN        6
#define NUMPIXELS 16
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
#define DELAYVAL 500


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

struct Player_info
{
    uint8_t id[5];
    uint8_t alive[5];
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

struct FloodResult {
    int size;
    int exitCount;
};

const uint8_t LETTERS[7][7] = {
    // W
    {
      0b10001,
      0b10001,
      0b10101,
      0b10101,
      0b10101,
      0b11011,
      0b10001
    },
    // I
    {
      0b11111,
      0b00100,
      0b00100,
      0b00100,
      0b00100,
      0b00100,
      0b11111
    },
    // N
    {
      0b10001,
      0b11001,
      0b10101,
      0b10011,
      0b10001,
      0b10001,
      0b10001
    },
    // L
    {
      0b10000,
      0b10000,
      0b10000,
      0b10000,
      0b10000,
      0b10000,
      0b11111
    },
    // O
    {
      0b01110,
      0b10001,
      0b10001,
      0b10001,
      0b10001,
      0b10001,
      0b01110
    },
    // S
    {
      0b01111,
      0b10000,
      0b10000,
      0b01110,
      0b00001,
      0b00001,
      0b11110
    },
    // E
    {
      0b11111,
      0b10000,
      0b11110,
      0b10000,
      0b10000,
      0b10000,
      0b11111
    }
};


#endif