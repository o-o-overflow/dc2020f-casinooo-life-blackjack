
#include <string.h>
#define DEP_VERS 1
#define NETWORK_SHM_KEY 550000
#define NETWORK_SHARED_SIZE 0x1000
#define NETWORK_PLAYER_OUT_SIZE 0x400
#define NETWORK_PLAYER_IN_SIZE 0x200
#define ZOMBIE_KEY 19001337
#define ZOMBIE_SIZE 0x100

#define MEMSTART_X 3453
#define MEMSTART_Y 31
#define MEMSTART_PLAYER_Y 31

#define INIT 0x2000
#define ACK 0x2000
#define BET 0x4000
#define BANK_RESULT 0x4000
#define DEAL 0x6000
#define BANK_REQUEST 0x6000
#define HIT 0x8000
#define STAY 0xA000
#define RESULT 0xC000
#define RELOAD 0xC000
#define DOWN 0xC000
#define ERROR 0xE000

std::string msgType(bool incoming, int value);


std::string msgType(bool toDealer, int value){
    switch (value) {
        case 1:
        case INIT: // and ACK
            return toDealer ? "INIT" : "ACK";
        case 2:
        case BET:
            return toDealer ? "BET" : "BANK_RESULT" ;
        case 3:
        case DEAL:
            return toDealer ? "BANK_REQUEST" : "DEALT";
        case 4:
        case HIT:
            return "HIT";
        case 5:
        case STAY:
            return "STAY";
        case 6:
        case RESULT:
            return toDealer ? "ZOMBIE" : "RESULT";
        case 7:
        case ERROR:
            return "ERROR";
    }
    return "???";
}
