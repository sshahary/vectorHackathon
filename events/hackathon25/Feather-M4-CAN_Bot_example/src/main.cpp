#include <Arduino.h>
#include <CAN.h>
#include "Hackathon25.h"

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
void rcv_Game();
void move(DIR direction);
void rcv_die();
void rcv_Finish();
void send_Name();

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
    send_Join();
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
    // else
    // {
    //     player_ID = 0;
    // }

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
        Serial.println("I am selected! Sending GameAck...");
        set_players(gameMsg);
        send_GameAck();
    }
    else
    {
        Serial.println("I am NOT part of this game.");
        Serial.printf("your ID: %d\n", player_ID);
    }
}
// direction

DIR chooseSpiralDirection()
{
    uint8_t px, py;
    switch (player_index) {
        case 1: px = positions.x1; py = positions.y1; break;
        case 2: px = positions.x2; py = positions.y2; break;
        case 3: px = positions.x3; py = positions.y3; break;
        case 4: px = positions.x4; py = positions.y4; break;
        default: return currentDir;
    }

    DIR dir = spiralDirs[spiralDirIndex];
    int nx = (px + dx[dir] + 64) % 64;
    int ny = (py + dy[dir] + 64) % 64;

    if (grid[nx][ny] == 0) {
        spiralStepsRemaining--;
        if (spiralStepsRemaining == 0) {
            spiralDirIndex = (spiralDirIndex + 1) % 4;
            spiralTurnCounter++;
            if (spiralTurnCounter % 2 == 0)
                spiralStepLength++;
            spiralStepsRemaining = spiralStepLength;
        }
        return dir;
    }
    for (int i = 1; i < 4; i++) {
        DIR altDir = spiralDirs[(spiralDirIndex + i) % 4];
        int ax = (px + dx[altDir] + 64) % 64;
        int ay = (py + dy[altDir] + 64) % 64;
        if (grid[ax][ay] == 0) {
            return altDir;
        }
    }
    return dir;
}

DIR chooseSafeDirection()
{
    uint8_t px, py;
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
    DIR tryDirs[3] = {
        currentDir,
        static_cast<DIR>(currentDir % 4 + 1),
        static_cast<DIR>(currentDir == 1 ? 4 : currentDir - 1)};

    for (DIR dir : tryDirs)
    {
        int nx = (px + dx[dir] + 64) % 64;
        int ny = (py + dy[dir] + 64) % 64;

        if (grid[nx][ny] == 0)
        {
            return dir;
        }
    }
    return currentDir;
}

void rcv_state()
{
    MSG_State msg_state;
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
    // print the grid
    // Serial.println("Grid:");
    // for (int i = 0; i < 64; i++)
    // {
    //     for (int j = 0; j < 64; j++)
    //         Serial.printf("%d ", grid[i][j]);
    // }
    // Serial.println("End of grid");

    // positions = msg_state;
    // move(Right);
    DIR safe = chooseSafeDirection();
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
