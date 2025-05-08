#include <Arduino.h>
#include <CAN.h>
#include "Hackathon25.h"

// Global variables
const uint32_t hardware_ID = (*(RoReg *)0x008061FCUL);
uint8_t player_ID = 0;
uint8_t game_ID = 0;
MSG_State positions;

// Function prototypes
void send_Join();
void rcv_Player();

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

// Send JOIN packet via CAN
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

// Receive player information
void rcv_Player()
{
  MSG_Player msg_player;
  CAN.readBytes((uint8_t *)&msg_player, sizeof(MSG_Player));

  if (msg_player.HardwareID == hardware_ID)
  {
    player_ID = msg_player.PlayerID;
    Serial.printf("Player ID recieved\n");
  }
  //  else {
  //     player_ID = 0;
  // }

  Serial.printf("Received Player packet | Player ID received: %u | Own Player ID: %u | Hardware ID received: %u | Own Hardware ID: %u\n",
                msg_player.PlayerID, player_ID, msg_player.HardwareID, hardware_ID);
}

// Receive player information
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

  // positions = msg_state;
  Serial.printf("Received Positions\n");
}
