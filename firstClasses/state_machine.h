#include "macros.h"
#include <stdio.h>
#include <stddef.h>
#include <string.h>

#define DATA_SIZE 1000

enum state{START,FLAG_RCV,A_RCV,C_RCV,BCC_RCV,DATA,BCC2_RCV,DONE};
enum mode{SET_RES,UA_RES,DISC_RES,RR_REC,I_REC}; //add more

struct state_machine
{
    enum mode mode;
    enum state state;
    unsigned char a;
    unsigned char c;
    unsigned char bcc;
    unsigned char data[DATA_SIZE];
    int curIndx;
    unsigned char bcc2;
};

struct state_machine getStateMachine(); 

enum state state_machine_get_state();

void setStateMachine(enum mode mode);

void stateMachine(unsigned char byte);

void resetStateMachine();

void flag_rcv_process(unsigned char byte);

void a_rcv_process(unsigned char byte);

void c_rcv_process(unsigned char byte);

void bcc_rcv_process(unsigned char byte);

void data_rcv_process(unsigned char byte);

void bcc2_rcv_process(unsigned char byte);

