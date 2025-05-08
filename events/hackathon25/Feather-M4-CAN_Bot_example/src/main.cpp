#include <Arduino.h>
#include <CAN.h>
#include "Hackathon25.h"

// Global variables
const uint32_t hardware_ID = (*(RoReg *)0x008061FCUL);
uint8_t player_ID = 0;
uint8_t game_ID = 0;


// Function prototypes
void send_Join();
void rcv_Player();
void rcv_Game();

// CAN receive callback
void onReceive(int packetSize) {
  if (packetSize) {
    switch(CAN.packetId()) {      
      case Player:
        Serial.println("CAN: Received Player packet");
        rcv_Player();
        break;
      case Game:
        Serial.println("CAN: Received Game packet");
        rcv_Game();
        break;
      default:
        Serial.println("CAN: Received unknown packet");
        break;
    }
  }
}

// CAN setup
bool setupCan(long baudRate) {
    pinMode(PIN_CAN_STANDBY, OUTPUT);
    digitalWrite(PIN_CAN_STANDBY, false);
    pinMode(PIN_CAN_BOOSTEN, OUTPUT);
    digitalWrite(PIN_CAN_BOOSTEN, true);

    if (!CAN.begin(baudRate)) {
        return false;
    }
    return true;
}

// Setup
void setup() {
    Serial.begin(115200);
    while (!Serial);

    
    Serial.println("Initializing CAN bus...");
    if (!setupCan(500000)) {
        Serial.println("Error: CAN initialization failed!");
        while (1);
    }
    Serial.println("CAN bus initialized successfully."); 

    CAN.onReceive(onReceive);

    delay(1000);
    send_Join();
}

// Loop remains empty, logic is event-driven via CAN callback
void loop() {}

// Send JOIN packet via CAN
void send_Join(){
    MSG_Join msg_join;
    msg_join.HardwareID = hardware_ID;

    CAN.beginPacket(Join);
    CAN.write((uint8_t*)&msg_join, sizeof(MSG_Join));
    CAN.endPacket();

    Serial.printf("JOIN packet sent (Hardware ID: %u)\n", hardware_ID);
}


// Receive player information
void rcv_Player(){
    MSG_Player msg_player;
    CAN.readBytes((uint8_t*)&msg_player, sizeof(MSG_Player));

    if(msg_player.HardwareID == hardware_ID){
        player_ID = msg_player.PlayerID;
        Serial.printf("Player ID recieved\n");
    }
    //  else {
    //     player_ID = 0;
    // }

    Serial.printf("Received Player packet | Player ID received: %u | Own Player ID: %u | Hardware ID received: %u | Own Hardware ID: %u\n", 
        msg_player.PlayerID, player_ID, msg_player.HardwareID, hardware_ID);
}

// Receive game information

void send_GameAck() {
    MSG_GameAck ack;
    ack.player_ID = player_ID;

    CAN.beginPacket(GameAck);
    CAN.write((uint8_t*)&ack, sizeof(MSG_GameAck));
    CAN.endPacket();

    Serial.printf("Sent GameAck for Player ID: %u\n", player_ID);
}

void rcv_Game() {
    MSG_Game gameMsg;
    CAN.readBytes((uint8_t*)&gameMsg, sizeof(MSG_Game));

    bool isSelected = (
        gameMsg.player1_ID == player_ID ||
        gameMsg.player2_ID == player_ID ||
        gameMsg.player3_ID == player_ID ||
        gameMsg.player4_ID == player_ID
    );
    Serial.printf("Game started with players: %u, %u, %u, %u\n",
        gameMsg.player1_ID, gameMsg.player2_ID, gameMsg.player3_ID, gameMsg.player4_ID);
    if (isSelected) {
        Serial.println("I am selected! Sending GameAck...");
        send_GameAck();
    } else {
        Serial.println("I am NOT part of this game.");
    }
}
