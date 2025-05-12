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

// Function prototypes
void send_Join();
void send_Name();
void move(DIR direction);
void rcv_Player();
void rcv_Game();
void rcv_state();
void rcv_Error();
void rcv_die();
void rcv_Finish();

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
			Serial.println("CAN: Received Die packet");
			rcv_die();
			break;
		case Finish:
			Serial.println("CAN: Received Finish packet");
			rcv_Finish();
			break;
		case Error:
			Serial.println("CAN: Received Error packet");
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

	player_info.playing = 0;
	CAN.onReceive(onReceive);

	delay(1000);
	send_Join();
}

// Loop remains empty, logic is event-driven via CAN callback
void loop() {}

// SEND --------------------------------------------------------------------------

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

// RECEIVE --------------------------------------------------------------------------

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

// reset game variables
void init_game()
{
	currentDir = Right;
	goingRight = true;
	for (int i = 0; i < 64; i++)
	{
		for (int j = 0; j < 64; j++)
		{
			grid[i][j] = 0;
		}
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
		init_game();
		Serial.println("I am selected! Sending GameAck...");
		set_players(gameMsg);
		player_info.playing = 1;
		send_GameAck();
	}
	else
	{
		player_info.playing = 0;
		Serial.println("I am NOT part of this game.");
		Serial.printf("your ID: %d\n", player_ID);
	}
}

// Receive player positions
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
	move(safe);
	Serial.printf("Received Positions\n");
}

// handle received errors
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

// set trace of dead player to 0
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

// Receive die message
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

// Receive finish message
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
	Serial.printf("WINNER %d\n", player_info.id[win], scores[win]);
}

// ALGORITHM --------------------------------------------------------------------------

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

FloodResult advancedFloodScore(uint8_t sx, uint8_t sy)
{
	bool visited[64][64] = {false};
	std::queue<std::pair<uint8_t, uint8_t>> q;
	q.push({sx, sy});
	visited[sx][sy] = true;
	int size = 1;
	int exits = 0;

	while (!q.empty())
	{
		auto [x, y] = q.front();
		q.pop();
		for (int d = 1; d <= 4; ++d)
		{
			int nx = (x + dx[d] + 64) % 64;
			int ny = (y + dy[d] + 64) % 64;
			if (!visited[nx][ny])
			{
				if (grid[nx][ny] == 0)
				{
					visited[nx][ny] = true;
					q.push({nx, ny});
					size++;
				}
				else
				{
					exits++;
				}
			}
		}
	}
	return {size, exits};
}

// Score a direction using lookahead, openness, flood fill, trap check
int scoreDirection(DIR dir, uint8_t px, uint8_t py, uint8_t selfIndex)
{
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
	for (int i = 1; i <= 3; ++i)
	{
		cx = (cx + dx[dir] + 64) % 64;
		cy = (cy + dy[dir] + 64) % 64;
		if (grid[cx][cy] != 0)
			break;
		score += 10;
	}

	// Free neighbors around next step
	int free_neighbors = 0;
	for (int d = 1; d <= 4; ++d)
	{
		int ax = (nx + dx[d] + 64) % 64;
		int ay = (ny + dy[d] + 64) % 64;
		if (grid[ax][ay] == 0)
			free_neighbors++;
	}
	score += 5 * free_neighbors;

	FloodResult flood = advancedFloodScore(nx, ny);
	score += flood.size;

	// Penalize tunnels
	if (flood.exitCount <= 1 && flood.size < 30)
	{
		score -= 5000;
		Serial.printf("DIR %d leads into closed tunnel (exits: %d, size: %d)\n", dir, flood.exitCount, flood.size);
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

	// Try: current, right, left
	DIR options[3] = {
		currentDir,
		static_cast<DIR>(currentDir % 4 + 1),
		static_cast<DIR>(currentDir == 1 ? 4 : currentDir - 1)};

	int bestScore = -100000;
	DIR bestDir = currentDir;
	for (DIR dir : options)
	{
		int score = scoreDirection(dir, px, py, player_index);
		Serial.printf("Direction %d has score %d\n", dir, score);
		if (score > bestScore)
		{
			bestScore = score;
			bestDir = dir;
		}
	}
	return bestDir;
}
