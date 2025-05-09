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

enum StrategyMode { EXPLORE, DEFEND, ATTACK };

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

struct FloodResult
{
    int size;
    int exitCount;
};

#endif


// int scoreDirection(DIR dir, uint8_t px, uint8_t py, uint8_t selfIndex)
// {
//     int nx = (px + dx[dir] + 64) % 64;
//     int ny = (py + dy[dir] + 64) % 64;
//     int score = 0;

//     for (int i = 1; i <= 4; ++i)
//     {
//         if (i == selfIndex || !player_info.alive[i])
//             continue;

//         // int dx_enemy = std::abs(nx - px);
//         int dx_enemy = (nx - px < 0) ? -(nx - px) : nx - px;
//         int dy_enemy = (ny - py < 0) ? -(ny - py) : ny - py;
//         int manhattan = dx_enemy + dy_enemy;

//         if (manhattan <= 2)
//         {
//             score -= (3 - manhattan) * 200; // nearby players = dangerous
//         }
//     }

//     if (grid[nx][ny] != 0)
//         return -10000;

//     if (willBeOccupied(nx, ny, selfIndex))
//         return -5000;

//     // Lookahead
//     int cx = px;
//     int cy = py;
//     for (int i = 1; i <= 3; ++i)
//     {
//         cx = (cx + dx[dir] + 64) % 64;
//         cy = (cy + dy[dir] + 64) % 64;
//         if (grid[cx][cy] != 0)
//             break;
//         score += 10;
//     }

//     // Free neighbors around next step
//     int free_neighbors = 0;
//     for (int d = 1; d <= 4; ++d)
//     {
//         int ax = (nx + dx[d] + 64) % 64;
//         int ay = (ny + dy[d] + 64) % 64;
//         if (grid[ax][ay] == 0)
//             free_neighbors++;
//     }
//     score += 5 * free_neighbors;

//     // // Flood fill: how much space will I have if I go here?
//     // score += floodFillSize(nx, ny);

//     // // Trap check: does this direction lead into a dead space?
//     // if (isTrap(nx, ny, 10)) {
//     //     score -= 3000;
//     //     Serial.printf("DIR %d leads to a trap!\n", dir);
//     // }

//     FloodResult flood = advancedFloodScore(nx, ny);
//     // score += flood.size; // main score but for test purposes only i am putting something down below

//     // Penalize tunnels
//     if (flood.exitCount <= 1 && flood.size < 30)
//     {
//         score -= 5000;
//         Serial.printf("DIR %d leads into closed tunnel (exits: %d, size: %d)\n", dir, flood.exitCount, flood.size);
//     }

//     return score;
// }