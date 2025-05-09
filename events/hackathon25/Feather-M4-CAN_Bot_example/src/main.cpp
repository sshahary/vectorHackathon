#include <Arduino.h>
#include <CAN.h>
#include "Hackathon25.h"
#include <queue>
#include <Adafruit_NeoPixel.h>

// LED setup

#define LED_PIN 6
#define LED_WIDTH 10
#define LED_HEIGHT 10
#define LED_COUNT (LED_WIDTH * LED_HEIGHT)

Adafruit_NeoPixel leds(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);



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
    // LED part:
    leds.begin();
    leds.clear();
    leds.show();
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

// Helper function to Convert 2D → LED Index (zigzag layout)
int XY(uint8_t x, uint8_t y) {
    if (y % 2 == 0) return y * LED_WIDTH + x;        // Even row: left → right
    else            return y * LED_WIDTH + (9 - x);  // Odd row: right → left
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

//#######################################################################

// Detect if the cell will be occupied by another player
bool willBeOccupied(uint8_t x, uint8_t y, uint8_t selfIndex) {
    uint8_t px[5] = {0, positions.x1, positions.x2, positions.x3, positions.x4};
    uint8_t py[5] = {0, positions.y1, positions.y2, positions.y3, positions.y4};

    for (int i = 1; i <= 4; ++i) {
        if (i == selfIndex || player_info.alive[i] == 0)
            continue;

        for (int d = 1; d <= 4; ++d) {
            int nx = (px[i] + dx[d] + 64) % 64;
            int ny = (py[i] + dy[d] + 64) % 64;
            if (nx == x && ny == y)
                return true;
        }
    }
    return false;
}

// Basic flood fill to measure open area
// int floodFillSize(uint8_t sx, uint8_t sy) {
//     bool visited[64][64] = {false};
//     std::queue<std::pair<uint8_t, uint8_t>> q;
//     q.push({sx, sy});
//     visited[sx][sy] = true;
//     int count = 1;

//     while (!q.empty()) {
//         auto [x, y] = q.front(); q.pop();

//         for (int d = 1; d <= 4; ++d) {
//             int nx = (x + dx[d] + 64) % 64;
//             int ny = (y + dy[d] + 64) % 64;

//             if (!visited[nx][ny] && grid[nx][ny] == 0) {
//                 visited[nx][ny] = true;
//                 q.push({nx, ny});
//                 count++;
//             }
//         }
//     }
//     return count;
// }

FloodResult advancedFloodScore(uint8_t sx, uint8_t sy) {
    bool visited[64][64] = {false};
    std::queue<std::pair<uint8_t, uint8_t>> q;
    q.push({sx, sy});
    visited[sx][sy] = true;

    int size = 1;
    int exits = 0;

    while (!q.empty()) {
        auto [x, y] = q.front(); q.pop();

        for (int d = 1; d <= 4; ++d) {
            int nx = (x + dx[d] + 64) % 64;
            int ny = (y + dy[d] + 64) % 64;

            if (!visited[nx][ny]) {
                if (grid[nx][ny] == 0) {
                    visited[nx][ny] = true;
                    q.push({nx, ny});
                    size++;
                } else {
                    exits++; // touches something that’s occupied
                }
            }
        }
    }

    return { size, exits };
}


// Detect whether a location leads into a trap
bool isTrap(uint8_t x, uint8_t y, int threshold = 10) {
    bool visited[64][64] = {false};
    std::queue<std::pair<uint8_t, uint8_t>> q;
    q.push({x, y});
    visited[x][y] = true;
    int count = 1;

    while (!q.empty() && count <= threshold) {
        auto [cx, cy] = q.front(); q.pop();

        for (int d = 1; d <= 4; ++d) {
            int nx = (cx + dx[d] + 64) % 64;
            int ny = (cy + dy[d] + 64) % 64;

            if (!visited[nx][ny] && grid[nx][ny] == 0) {
                visited[nx][ny] = true;
                q.push({nx, ny});
                count++;
            }
        }
    }

    return count < threshold;
}

// Score a direction using lookahead, openness, flood fill, trap check
int scoreDirection(DIR dir, uint8_t px, uint8_t py, uint8_t selfIndex) {
    int nx = (px + dx[dir] + 64) % 64;
    int ny = (py + dy[dir] + 64) % 64;

    if (grid[nx][ny] != 0)
        return -10000;

    if (willBeOccupied(nx, ny, selfIndex))
        return -5000;

    int score = 0;

    // Lookahead
    int cx = px;
    int cy = py;
    for (int i = 1; i <= 3; ++i) {
        cx = (cx + dx[dir] + 64) % 64;
        cy = (cy + dy[dir] + 64) % 64;
        if (grid[cx][cy] != 0)
            break;
        score += 10;
    }

    // Free neighbors around next step
    int free_neighbors = 0;
    for (int d = 1; d <= 4; ++d) {
        int ax = (nx + dx[d] + 64) % 64;
        int ay = (ny + dy[d] + 64) % 64;
        if (grid[ax][ay] == 0)
            free_neighbors++;
    }
    score += 5 * free_neighbors;

    // // Flood fill: how much space will I have if I go here?
    // score += floodFillSize(nx, ny);

    // // Trap check: does this direction lead into a dead space?
    // if (isTrap(nx, ny, 10)) {
    //     score -= 3000;
    //     Serial.printf("DIR %d leads to a trap!\n", dir);
    // }

    FloodResult flood = advancedFloodScore(nx, ny);
    score += flood.size;
    
    // Penalize tunnels
    if (flood.exitCount <= 1 && flood.size < 30) {
        score -= 5000;
        Serial.printf("DIR %d leads into closed tunnel (exits: %d, size: %d)\n", dir, flood.exitCount, flood.size);
    }
    


    return score;
}

// Main decision logic
DIR chooseDirection() {
    uint8_t px = 0, py = 0;
    switch (player_index) {
        case 1: px = positions.x1; py = positions.y1; break;
        case 2: px = positions.x2; py = positions.y2; break;
        case 3: px = positions.x3; py = positions.y3; break;
        case 4: px = positions.x4; py = positions.y4; break;
        default: return currentDir;
    }

    // Try: current, right, left
    DIR options[3] = {
        currentDir,
        static_cast<DIR>(currentDir % 4 + 1),
        static_cast<DIR>(currentDir == 1 ? 4 : currentDir - 1)
    };

    int bestScore = -100000;
    DIR bestDir = currentDir;

    for (DIR dir : options) {
        int score = scoreDirection(dir, px, py, player_index);
        Serial.printf("Direction %d has score %d\n", dir, score);
        if (score > bestScore) {
            bestScore = score;
            bestDir = dir;
        }
    }

    return bestDir;
}

//#######################################################################

// direction

DIR chooseSpiralDirection() {
    uint8_t px, py;
    switch (player_index)
    {
    case 1: px = positions.x1; py = positions.y1; break;
    case 2: px = positions.x2; py = positions.y2; break;
    case 3: px = positions.x3; py = positions.y3; break;
    case 4: px = positions.x4; py = positions.y4; break;
    default: return currentDir;
    }

    int fx = (px + dx[currentDir] + 64) % 64;
    int fy = (py + dy[currentDir] + 64) % 64;

    if (grid[fx][fy] == 0) {
        return currentDir; 
    }

    if (currentDir == Right || currentDir == Left) {
        int dyDir = (py + 1 + 64) % 64;
        if (grid[px][dyDir] == 0) {
            return Down;
        }
    }
    if (currentDir == Down) {
        if (goingRight) {
            int rx = (px + 1 + 64) % 64;
            if (grid[rx][py] == 0) {
                currentDir = Right;
                goingRight = true;
                return Right;
            }
        } else {
            int lx = (px - 1 + 64) % 64;
            if (grid[lx][py] == 0) {
                currentDir = Left;
                goingRight = false;
                return Left;
            }
        }
    }

    return currentDir;
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

// Receive player positions
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

    DIR safe = chooseDirection();
    currentDir = safe;
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

// void rcv_Finish()
// {
//     MSG_Finish msg_finish;
//     CAN.readBytes((uint8_t *)&msg_finish, sizeof(MSG_Finish));

//     Serial.printf("Received Finish packet\nPlayer ID: %u | Points: %u\nPlayer ID: %u | Points: %u\nPlayer ID: %u | Points: %u\nPlayer ID: %u | Points: %u\n",
//                   msg_finish.id1, msg_finish.point1, msg_finish.id2, msg_finish.point2, msg_finish.id3, msg_finish.point3, msg_finish.id4, msg_finish.point4);
// }

// Recive finish with LED part:
void rcv_Finish()
{
    MSG_Finish msg_finish;
    CAN.readBytes((uint8_t *)&msg_finish, sizeof(MSG_Finish));

    Serial.printf("Received Finish packet\n");
    Serial.printf("Player ID: %u | Points: %u\n", msg_finish.id1, msg_finish.point1);
    Serial.printf("Player ID: %u | Points: %u\n", msg_finish.id2, msg_finish.point2);
    Serial.printf("Player ID: %u | Points: %u\n", msg_finish.id3, msg_finish.point3);
    Serial.printf("Player ID: %u | Points: %u\n", msg_finish.id4, msg_finish.point4);

    // Identify our score
    uint8_t myPoints = 0;
    if (msg_finish.id1 == player_ID) myPoints = msg_finish.point1;
    else if (msg_finish.id2 == player_ID) myPoints = msg_finish.point2;
    else if (msg_finish.id3 == player_ID) myPoints = msg_finish.point3;
    else if (msg_finish.id4 == player_ID) myPoints = msg_finish.point4;

    // Find highest score among all
    uint8_t maxPoints = msg_finish.point1;
    if (msg_finish.point2 > maxPoints) maxPoints = msg_finish.point2;
    if (msg_finish.point3 > maxPoints) maxPoints = msg_finish.point3;
    if (msg_finish.point4 > maxPoints) maxPoints = msg_finish.point4;

    // Decide outcome
    bool win = (myPoints == maxPoints);

    leds.clear();

    uint32_t color = win ? leds.Color(0, 255, 0) : leds.Color(255, 0, 0);  // Green win, red lose

    // Fill matrix
    for (int y = 0; y < LED_HEIGHT; ++y) {
        for (int x = 0; x < LED_WIDTH; ++x) {
            leds.setPixelColor(XY(x, y), color);
        }
    }
    leds.show();

    Serial.println(win ? "🏆 YOU WIN!" : "💀 YOU LOST!");
}
