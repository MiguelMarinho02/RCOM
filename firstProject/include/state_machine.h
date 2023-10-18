#include "macros.h"
#include <stdio.h>
#include <stddef.h>
#include <string.h>

#define DATA_SIZE 200

enum state{START,FLAG_RCV,A_RCV,C_RCV,BCC_RCV,DATA,BCC2_RCV,DONE};
enum mode{SET_RES,UA_RES,DISC_REC,RR_REC,I_REC,RJ_REC};
enum type{READER,TRANSMITER};

struct state_machine
{
    enum mode mode;
    enum state state;
    enum type type;
    unsigned char a;
    unsigned char c;
    unsigned char bcc;
    unsigned char data[DATA_SIZE];
    int curIndx;
    unsigned char bcc2;
};

struct state_machine getStateMachine(); 

enum state state_machine_get_state();

void setStateMachine(enum mode mode, enum type type);

void stateMachine(unsigned char byte);

void resetStateMachine();

void flag_rcv_process(unsigned char byte);

void a_rcv_process(unsigned char byte);

void c_rcv_process(unsigned char byte);

void bcc_rcv_process(unsigned char byte);

void data_rcv_process(unsigned char byte);

void bcc2_rcv_process(unsigned char byte);

