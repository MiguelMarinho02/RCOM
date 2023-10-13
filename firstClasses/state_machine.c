#include "state_machine.h"

struct state_machine state_machine;

void setStateMachine(enum mode mode){
    state_machine.mode = mode;
    state_machine.state = START;
}

void resetStateMachine(){
    state_machine.a = 0;
    state_machine.c= 0;
    state_machine.state=START;
    state_machine.bcc=0;
    state_machine.bcc2=0;
}

struct state_machine getStateMachine(){
    return state_machine;
}

enum state state_machine_get_mode(){
    return state_machine.state;
}

void stateMachine(unsigned char byte){
    switch (state_machine.state)
    {
    case START:
        if(byte == FLAG){
            state_machine.state=FLAG_RCV;
        }
        break;
    case FLAG_RCV:
        flag_rcv_process(byte);
        break;
    case A_RCV:
        a_rcv_process(byte);
        break;
    case C_RCV:
        c_rcv_process(byte);
        break;
    case BCC_RCV:
        bcc_rcv_process(byte);
        break;
    case DATA:
        //data_rcv_process(byte);
        break;
    case BCC2_RCV:
        //bcc2_rcv_process(byte);
        break;
    case DONE:
        printf("Returned state: %d\n", state_machine_get_mode());
        setStateMachine(state_machine.mode);
        break;
    default:
        break;
    }
}

void flag_rcv_process(unsigned char byte){
    if(state_machine.mode == SET_RES){
        if(byte == FLAG){
            return;
        }
        else if(byte == A_SENDER){
            state_machine.state = A_RCV;
            state_machine.a = byte;
        }
        else{
            state_machine.state = START;
        }
    }
    else if(state_machine.mode == UA_RES){
        if(byte == FLAG){
            return;
        }
        else if(byte == A_RECEIVER){
            state_machine.state = A_RCV;
            state_machine.a = byte;
        }
        else{
            state_machine.state = START;
        }
    }
    else{};
}

void a_rcv_process(unsigned char byte){
    if(state_machine.mode == SET_RES){
        if(byte == FLAG){
            state_machine.state = FLAG_RCV;
        }
        else if(byte == SET){
            state_machine.state = C_RCV;
            state_machine.c = byte;
        }
        else{
            state_machine.state = START;
        }
    }
    else if(state_machine.mode == UA_RES){
        if(byte == FLAG){
            state_machine.state = FLAG_RCV;
        }
        else if(byte == UA){
            state_machine.state = C_RCV;
            state_machine.c = byte;
        }
        else{
            state_machine.state = START;
        }
    }
    else{};
}

void c_rcv_process(unsigned char byte){
    if(state_machine.mode == SET_RES){
        if(byte == FLAG){
            state_machine.state = FLAG_RCV;
        }
        else if(byte == BCC(state_machine.a,state_machine.c)){
            state_machine.state = BCC_RCV;
            state_machine.bcc = BCC(state_machine.a,state_machine.c);
        }
        else{
            state_machine.state = START;
        }
    }
    else if(state_machine.mode == UA_RES){
        if(byte == FLAG){
            state_machine.state = FLAG_RCV;
        }
        else if(byte == BCC(state_machine.a,state_machine.c)){
            state_machine.state = BCC_RCV;
            state_machine.bcc = BCC(state_machine.a,state_machine.c);
        }
        else{
            state_machine.state = START;
        }
    }
    else{};
}

void bcc_rcv_process(unsigned char byte){
    if(state_machine.mode == SET_RES){
        if(byte == FLAG){
            state_machine.state = DONE;
        }
        else{
            state_machine.state = START;
        }
    }
    else if(state_machine.mode == UA_RES){
        if(byte == FLAG){
            state_machine.state = DONE;
        }
        else{
            state_machine.state = START;
        }
    }
    else{};
}
