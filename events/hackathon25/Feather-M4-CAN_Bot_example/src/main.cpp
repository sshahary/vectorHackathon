#include <Arduino.h>
#include <CAN.h>
#include "Hackathon25.h"
#include <queue>

// Global variables
const uint32_t hardware_ID = (*(RoReg *)0x008061FCUL);
uint8_t player_ID = 0;
uint8_t game_ID = 0;
uint8_t player_index = 0;
struct Player_info player_info;
MSG_State positions;
int8_t grid[64][64] = {0};

const int8_t dx[] = {0, 0, 1, 0, -1};
const int8_t dy[] = {0, 1, 0, -1, 0};

DIR currentDir = Right;
bool goingRight = true;

DIR spiralDirs[4] = {Right, Down, Left, Up};
uint8_t spiralStepLength = 1;
uint8_t spiralStepsRemaining = 1;
uint8_t spiralTurnCounter = 0;
uint8_t spiralDirIndex = 0;

// Function prototypes
void send_Join();
void rcv_Player();
void rcv_state();
void rcv_Error();
void rcv_Game();
void move(DIR direction);
void rcv_die();
void rcv_Finish();
void send_Name();
DIR chooseDirection();

// CAN receive callback
void onReceive(int packetSize)
{
    if (packetSize)
    {
        switch (CAN.packetId())
        {
        case Player:
            Serial.println("CAN: Received Player packet");
            rcv_Player();
            break;
        case Game:
            Serial.println("CAN: Received Game packet");
            rcv_Game();
            break;
        case State:
            Serial.println("CAN: Received State packet");
            rcv_state();
            break;
        case Die:
            rcv_die();
            Serial.println("CAN: Received Die packet");
            break;
        case Finish:
            Serial.println("CAN: Received Finish packet");
            rcv_Finish();
            break;
        case Error:
            Serial.println("CAN: Received Finish packet");
            rcv_Error();
            break;
        default:
            Serial.println("CAN: Received unknown packet");
            break;
        }
    }
}

// CAN setup
bool setupCan(long baudRate)
{
    pinMode(PIN_CAN_STANDBY, OUTPUT);
    digitalWrite(PIN_CAN_STANDBY, false);
    pinMode(PIN_CAN_BOOSTEN, OUTPUT);
    digitalWrite(PIN_CAN_BOOSTEN, true);

    if (!CAN.begin(baudRate))
    {
        return false;
    }
    return true;
}

// Setup
void setup()
{
    Serial.begin(115200);
    while (!Serial)
        ;

    Serial.println("Initializing CAN bus...");
    if (!setupCan(500000))
    {
        Serial.println("Error: CAN initialization failed!");
        while (1)
            ;
    }
    Serial.println("CAN bus initialized successfully.");

    CAN.onReceive(onReceive);

    delay(1000);
    player_info.playing = 0;
    send_Join();
}

void init_g()
{
    currentDir = Right;
    goingRight = true;
    spiralStepLength = 1;
    spiralStepsRemaining = 1;
    spiralTurnCounter = 0;
    spiralDirIndex = 0;
}

// Loop remains empty, logic is event-driven via CAN callback
void loop() {}

// Send JOIN packet via CAN
void send_Join()
{
    MSG_Join msg_join;
    msg_join.HardwareID = hardware_ID;

    CAN.beginPacket(Join);
    CAN.write((uint8_t *)&msg_join, sizeof(MSG_Join));
    CAN.endPacket();

    Serial.printf("JOIN packet sent (Hardware ID: %u)\n", hardware_ID);
}

// Receive player information
void rcv_Player()
{
    MSG_Player msg_player;
    CAN.readBytes((uint8_t *)&msg_player, sizeof(MSG_Player));

    if (msg_player.HardwareID == hardware_ID)
    {
        player_ID = msg_player.PlayerID;
        Serial.printf("Player ID recieved\n");
        send_Name();
    }

    Serial.printf("Received Player packet | Player ID received: %u | Own Player ID: %u | Hardware ID received: %u | Own Hardware ID: %u\n",
                  msg_player.PlayerID, player_ID, msg_player.HardwareID, hardware_ID);
}

// set player name
void send_Name()
{
    MSG_Name msg_name;
    msg_name.playerID = player_ID;
    msg_name.name[0] = 'R';
    msg_name.name[1] = 'S';
    msg_name.name[2] = 'A';
    msg_name.size = 3;

    CAN.beginPacket(Rename);
    CAN.write((uint8_t *)&msg_name, sizeof(MSG_Name));
    CAN.endPacket();

    Serial.printf("RENAME packet sent (Hardware ID: %u)\n", hardware_ID);
}

void send_GameAck()
{
    MSG_GameAck ack;
    ack.player_ID = player_ID;

    CAN.beginPacket(GameAck);
    CAN.write((uint8_t *)&ack, sizeof(MSG_GameAck));
    CAN.endPacket();

    Serial.printf("Sent GameAck for Player ID: %u\n", player_ID);
}

void set_players(MSG_Game gameMsg)
{
    for (int i = 1; i <= 4; i++)
        player_info.alive[i] = 1;
    player_info.id[1] = gameMsg.player1_ID;
    player_info.id[2] = gameMsg.player2_ID;
    player_info.id[3] = gameMsg.player3_ID;
    player_info.id[4] = gameMsg.player4_ID;
    for (int i = 1; i <= 4; i++)
    {
        if (player_info.id[i] == player_ID)
            player_index = i;
    }
}

// Receive game information
void rcv_Game()
{
    MSG_Game gameMsg;
    CAN.readBytes((uint8_t *)&gameMsg, sizeof(MSG_Game));
    bool isSelected = (gameMsg.player1_ID == player_ID ||
                       gameMsg.player2_ID == player_ID ||
                       gameMsg.player3_ID == player_ID ||
                       gameMsg.player4_ID == player_ID);
    Serial.printf("Game started with players: %u, %u, %u, %u\n",
                  gameMsg.player1_ID, gameMsg.player2_ID, gameMsg.player3_ID, gameMsg.player4_ID);
    if (isSelected)
    {
        init_g();
        Serial.println("I am selected! Sending GameAck...");
        player_info.playing = 1;
        set_players(gameMsg);
        send_GameAck();
    }
    else
    {
        player_info.playing = 0;
        Serial.println("I am NOT part of this game.");
        Serial.printf("[your ID: %d]\n", player_ID);
    }
}

// Receive error information
void rcv_Error()
{
    MSG_Error errorMsg;
    CAN.readBytes((uint8_t *)&errorMsg, sizeof(MSG_Error));

    if (errorMsg.playerID != player_ID)
        return;
    switch (errorMsg.error_code)
    {
    case ERROR_INVALID_PLAYER_ID:
        send_Join();
        break;
    case ERROR_UNALLOWED_RENAME:
        Serial.println("PROBLEM WITH RENAME");
        break;
    case ERROR_YOU_ARE_NOT_PLAYING:
        player_info.playing = 0;
        break;
    case WARNING_UNKNOWN_MOVE:
        Serial.println("PROBLEM WITH MOVE -> ignored");
        break;
    default:
        break;
    }
}

void rcv_state()
{
    MSG_State msg_state;
    if (!player_info.playing)
        return;

    CAN.readBytes((uint8_t *)&msg_state, sizeof(MSG_State));
    positions.x1 = msg_state.x1;
    positions.x2 = msg_state.x2;
    positions.x3 = msg_state.x3;
    positions.x4 = msg_state.x4;
    positions.y1 = msg_state.y1;
    positions.y2 = msg_state.y2;
    positions.y3 = msg_state.y3;
    positions.y4 = msg_state.y4;

    // Update grid with player positions
    if (player_info.alive[1])
        grid[positions.x1][positions.y1] = 1;
    if (player_info.alive[2])
        grid[positions.x2][positions.y2] = 2;
    if (player_info.alive[3])
        grid[positions.x3][positions.y3] = 3;
    if (player_info.alive[4])
        grid[positions.x4][positions.y4] = 4;

    DIR safe = chooseDirection();
    currentDir = safe;
    // move(safe);
    move(safe);
    Serial.printf("Received Positions\n");
}

void move(DIR direction)
{
    if (player_ID == 0)
    {
        Serial.println("Player ID is not set. Cannot send Move packet.");
        return;
    }
    MSG_Move msg_move;
    msg_move.playerID = player_ID;
    msg_move.direction = direction;

    CAN.beginPacket(Move);
    CAN.write((uint8_t *)&msg_move, sizeof(MSG_Move));
    CAN.endPacket();

    Serial.printf("Sent Move packet | Player ID: %u | Direction: %u\n", player_ID, direction);
}

void set_dead(uint8_t index)
{
    for (int x = 0; x < 64; x++)
    {
        for (int y = 0; y < 64; y++)
        {
            if (grid[x][y] == index)
                grid[x][y] = 0;
        }
    }
    player_info.alive[index] = 0;
}

void rcv_die()
{
    MSG_Die msg_die;
    if (!player_info.playing)
        return;

    CAN.readBytes((uint8_t *)&msg_die, sizeof(MSG_Die));
    for (int i = 1; i <= 4; i++)
    {
        if (player_info.id[i] == msg_die.playerID)
        {
            set_dead(i);
            break;
        }
    }
    Serial.printf("Received Die packet | Player ID: %u\n", msg_die.playerID);
}

void rcv_Finish()
{
    MSG_Finish msg_finish;
    CAN.readBytes((uint8_t *)&msg_finish, sizeof(MSG_Finish));

    uint8_t scores[5] = {0, msg_finish.point1, msg_finish.point2, msg_finish.point3, msg_finish.point4};
    Serial.println("Received Finish packet");
    uint8_t win = 0;
    for (int i = 1; i <= 4; i++)
    {
        if (scores[i] > scores[win])
            win = i;
    }
    Serial.printf("WINNER %s %d points\n", player_info.id[win], scores[win]);
}

// #######################################################################

// Detect if the cell will be occupied by another player
bool willBeOccupied(uint8_t x, uint8_t y, uint8_t selfIndex)
{
    uint8_t px[5] = {0, positions.x1, positions.x2, positions.x3, positions.x4};
    uint8_t py[5] = {0, positions.y1, positions.y2, positions.y3, positions.y4};

    for (int i = 1; i <= 4; ++i)
    {
        if (i == selfIndex || player_info.alive[i] == 0)
            continue;

        for (int d = 1; d <= 4; ++d)
        {
            int nx = (px[i] + dx[d] + 64) % 64;
            int ny = (py[i] + dy[d] + 64) % 64;
            if (nx == x && ny == y)
                return true;
        }
    }
    return false;
}

// // Basic flood fill to measure open area
// int floodFillSize(uint8_t sx, uint8_t sy)
// {
//     bool visited[64][64] = {false};
//     std::queue<std::pair<uint8_t, uint8_t>> q;
//     q.push({sx, sy});
//     visited[sx][sy] = true;
//     int count = 1;

//     while (!q.empty())
//     {
//         auto [x, y] = q.front();
//         q.pop();

//         for (int d = 1; d <= 4; ++d)
//         {
//             int nx = (x + dx[d] + 64) % 64;
//             int ny = (y + dy[d] + 64) % 64;

//             if (!visited[nx][ny] && grid[nx][ny] == 0)
//             {
//                 visited[nx][ny] = true;
//                 q.push({nx, ny});
//                 count++;
//             }
//         }
//     }
//     return count;
// }

// Detect whether a location leads into a trap
bool isTrap(uint8_t x, uint8_t y, int threshold = 10)
{
    bool visited[64][64] = {false};
    std::queue<std::pair<uint8_t, uint8_t>> q;
    q.push({x, y});
    visited[x][y] = true;
    int count = 1;

    while (!q.empty() && count <= threshold)
    {
        auto [cx, cy] = q.front();
        q.pop();

        for (int d = 1; d <= 4; ++d)
        {
            int nx = (cx + dx[d] + 64) % 64;
            int ny = (cy + dy[d] + 64) % 64;

            if (!visited[nx][ny] && grid[nx][ny] == 0)
            {
                visited[nx][ny] = true;
                q.push({nx, ny});
                count++;
            }
        }
    }

    return count < threshold;
}

int floodFillScore(uint8_t x, uint8_t y, int maxDepth = 10)
{
    bool visited[64][64] = {false};
    struct Node
    {
        uint8_t x, y;
        int depth;
    };
    Node queue[1024];
    int q_start = 0, q_end = 0;
    int score = 0;

    queue[q_end++] = {x, y, 0};
    visited[y][x] = true;

    while (q_start < q_end && score < 1024)
    {
        Node current = queue[q_start++];

        if (current.depth > maxDepth)
            continue;
        score++;

        const int dx[4] = {0, 1, 0, -1};
        const int dy[4] = {-1, 0, 1, 0};

        for (int i = 0; i < 4; ++i)
        {
            int nx = current.x + dx[i];
            int ny = current.y + dy[i];

            if (nx < 0 || ny < 0 || nx >= 64 || ny >= 64)
                continue;
            if (grid[ny][nx] != 0 || visited[ny][nx])
                continue;

            visited[ny][nx] = true;
            queue[q_end++] = {(uint8_t)nx, (uint8_t)ny, current.depth + 1};
        }
    }

    return score;
}

// Main decision logic
DIR chooseDirection()
{
    uint8_t px = 0, py = 0;
    switch (player_index)
    {
    case 1:
        px = positions.x1;
        py = positions.y1;
        break;
    case 2:
        px = positions.x2;
        py = positions.y2;
        break;
    case 3:
        px = positions.x3;
        py = positions.y3;
        break;
    case 4:
        px = positions.x4;
        py = positions.y4;
        break;
    default:
        return currentDir;
    }

    DIR options[3] = {
        currentDir,
        static_cast<DIR>(currentDir % 4 + 1),
        static_cast<DIR>(currentDir == 1 ? 4 : currentDir - 1)};

    const int dx[4] = {0, 1, 0, -1}; // UP, RIGHT, DOWN, LEFT
    const int dy[4] = {-1, 0, 1, 0};

    int bestScore = -1;
    DIR bestDir = currentDir;

    for (int i = 0; i < 3; ++i)
    {
        DIR dir = options[i];
        int dirIdx = static_cast<int>(dir) - 1;
        int nx = px + dx[dirIdx];
        int ny = py + dy[dirIdx];

        if (nx < 0 || ny < 0 || nx >= 64 || ny >= 64)
            continue;
        if (grid[ny][nx] != 0)
            continue;
        if (willBeOccupied(nx, ny, player_index))
            continue;
        if (isTrap(nx, ny))
            continue;

        int score = floodFillScore(nx, ny, 10);
        if (score > bestScore)
        {
            bestScore = score;
            bestDir = dir;
        }
    }

    return bestDir;
}

// #######################################################################

// direction

// DIR chooseSpiralDirection()
// {
//     uint8_t px, py;
//     switch (player_index)
//     {
//     case 1:
//         px = positions.x1;
//         py = positions.y1;
//         break;
//     case 2:
//         px = positions.x2;
//         py = positions.y2;
//         break;
//     case 3:
//         px = positions.x3;
//         py = positions.y3;
//         break;
//     case 4:
//         px = positions.x4;
//         py = positions.y4;
//         break;
//     default:
//         return currentDir;
//     }

//     DIR dir = spiralDirs[spiralDirIndex];
//     int nx = (px + dx[dir] + 64) % 64;
//     int ny = (py + dy[dir] + 64) % 64;

//     if (grid[nx][ny] == 0)
//     {
//         spiralStepsRemaining--;
//         if (spiralStepsRemaining == 0)
//         {
//             spiralDirIndex = (spiralDirIndex + 1) % 4;
//             spiralTurnCounter++;
//             if (spiralTurnCounter % 2 == 0)
//                 spiralStepLength++;
//             spiralStepsRemaining = spiralStepLength;
//         }
//         return dir;
//     }
//     for (int i = 1; i < 4; i++)
//     {
//         DIR altDir = spiralDirs[(spiralDirIndex + i) % 4];
//         int ax = (px + dx[altDir] + 64) % 64;
//         int ay = (py + dy[altDir] + 64) % 64;
//         if (grid[ax][ay] == 0)
//         {
//             return altDir;
//         }
//     }
//     return dir;
// }

// DIR chooseSafeDirection()
// {
//     uint8_t px, py;
//     switch (player_index)
//     {
//     case 1:
//         px = positions.x1;
//         py = positions.y1;
//         break;
//     case 2:
//         px = positions.x2;
//         py = positions.y2;
//         break;
//     case 3:
//         px = positions.x3;
//         py = positions.y3;
//         break;
//     case 4:
//         px = positions.x4;
//         py = positions.y4;
//         break;
//     default:
//         return currentDir;
//     }
//     DIR tryDirs[3] = {
//         currentDir,
//         static_cast<DIR>(currentDir % 4 + 1),
//         static_cast<DIR>(currentDir == 1 ? 4 : currentDir - 1)};

//     for (DIR dir : tryDirs)
//     {
//         int nx = (px + dx[dir] + 64) % 64;
//         int ny = (py + dy[dir] + 64) % 64;

//         if (grid[nx][ny] == 0)
//         {
//             return dir;
//         }
//     }
//     return currentDir;
// }
