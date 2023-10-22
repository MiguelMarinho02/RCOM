#include "state_machine.h"

struct state_machine state_machine;

int escapeDetected = 0; //true or false

void setStateMachine(enum mode mode, enum type type){
    state_machine.mode = mode;
    state_machine.state = START;
    state_machine.curIndx = 0;
    state_machine.type = type;
}

void resetStateMachine(){
    state_machine.a = 0;
    state_machine.c= 0;
    state_machine.state=START;
    state_machine.bcc=0;
    state_machine.bcc2=0;
    state_machine.curIndx = 0;
    memset(state_machine.data,0,DATA_SIZE);
}

struct state_machine getStateMachine(){
    return state_machine;
}

enum state state_machine_get_state(){
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
        data_rcv_process(byte);
        break;
    case BCC2_RCV:
        break;
    case DONE:
        setStateMachine(state_machine.mode,state_machine.type);
        break;
    default:
        break;
    }
}

void flag_rcv_process(unsigned char byte){
    if(byte == FLAG){
        return;
    }
    if(state_machine.mode == DISC_REC){
        if(state_machine.type == READER && byte == A_SENDER){
            state_machine.state = A_RCV;
            state_machine.a = byte;
        }
        else if(state_machine.type == TRANSMITER && byte == A_RECEIVER){
            state_machine.state = A_RCV;
            state_machine.a = byte;
        }
        else{
            state_machine.state = START;
        }
    }
    else{
        if(byte == A_SENDER){
            state_machine.state = A_RCV;
            state_machine.a = byte;
        }
        else{
            state_machine.state = START;
        }
    }
}

void a_rcv_process(unsigned char byte){
    if(byte == FLAG){
        state_machine.state = FLAG_RCV;
        return;
    }
    if(state_machine.mode == SET_RES){
        if(byte == SET){
            state_machine.state = C_RCV;
            state_machine.c = byte;
        }
        else{
            state_machine.state = START;
        }
    }
    else if(state_machine.mode == UA_RES){
        if(byte == UA){
            state_machine.state = C_RCV;
            state_machine.c = byte;
        }
        else{
            state_machine.state = START;
        }
    }
    else if(state_machine.mode == RR_REC){
        if(byte == RR(0) || byte == RR(1)){
            state_machine.state = C_RCV;
            state_machine.c = byte;
        }
        else if(byte == REJ(0) ||  byte == REJ(1)){
            state_machine.state = C_RCV;
            state_machine.c = byte;
            state_machine.mode = RJ_REC;
        }
        else{
            state_machine.state = START;
        }
    }
    else if(state_machine.mode == I_REC){
        if(byte == CTRL_S(0) || byte == CTRL_S(1)){
            state_machine.state = C_RCV;
            state_machine.c = byte;
        }
        else if(byte == DISC){
            state_machine.state = C_RCV;
            state_machine.c = byte;
            state_machine.mode = DISC_REC;
        }
        else{
            state_machine.state = START;
        }
    }
    else if(state_machine.mode == DISC_REC){
        if(byte == DISC){
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

void bcc_rcv_process(unsigned char byte){
    if(state_machine.mode == SET_RES || state_machine.mode == UA_RES || state_machine.mode == RR_REC || state_machine.mode == DISC_REC){
        if(byte == FLAG){
            state_machine.state = DONE;
        }
        else{
            state_machine.state = START;
        }
    }
    else if(state_machine.mode == I_REC){
        if(byte == FLAG){
            state_machine.state = START;
        }
        else{
            state_machine.state = DATA;
            if(byte == 0x7d){
                escapeDetected = 1;
            }
            else{
                state_machine.data[0] = byte;
                state_machine.curIndx++;
            }
        }
    }
    else{};
}

void data_rcv_process(unsigned char byte){
    if(state_machine.mode == I_REC){
        if(state_machine.curIndx >= DATA_SIZE){
            state_machine.state = START;
            memset(state_machine.data,0,DATA_SIZE);
        }
        else if(byte == FLAG){ 
            state_machine.state = BCC2_RCV;
            bcc2_rcv_process();
        }
        else{  //reverse engineer the stuffing process
            if(escapeDetected == 1){
                state_machine.data[state_machine.curIndx] = BCC(byte,0x20);
                state_machine.curIndx++;
                escapeDetected = 0;
            }
            else{
                if(byte == 0x7d){
                    escapeDetected = 1;
                }
                else{
                    state_machine.data[state_machine.curIndx] = byte;
                    state_machine.curIndx++;
                }
            }
        }

    }
}

void bcc2_rcv_process(){
    if(state_machine.mode == I_REC){
        unsigned char bcc2 = 0x00;

        for(int i = 0; i < state_machine.curIndx - 1; i++){
            bcc2 = BCC(bcc2,state_machine.data[i]);
        }
        
        if(bcc2 != state_machine.data[state_machine.curIndx - 1]){
            state_machine.mode = RJ_REC;
            return;
        }

        state_machine.state = DONE;
        state_machine.curIndx--;
    }
}