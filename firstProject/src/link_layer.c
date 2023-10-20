// Link layer protocol implementation

#include "link_layer.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source
#define SUPERVISION_SIZE 5
#define MAX_INFO_SIZE 206 //RANDOM NUMBER + 6

volatile int STOP = FALSE;

volatile int alarmEnabled = FALSE;

LinkLayer parameters;

int fd;

enum type type;

struct termios oldtio;
struct termios newtio;

int frameNumber = 0;

void alarmHandler(int signal) // Defines the alarm
{
    alarmEnabled = TRUE;
}

int writeInfo(const unsigned char *buffer,int bufSize){
    unsigned char buf[MAX_INFO_SIZE];
    
    buf[0] = FLAG;
    buf[1] = A_SENDER;
    buf[2] = CTRL_S(frameNumber);
    buf[3] = BCC(buf[1],buf[2]);
    //printf("A=0x%02X,C=0x%02X,BCC1=0x%02X\n",buf[1],buf[2],buf[3]);

    unsigned char bcc2 = buffer[0];
    for(int i = 0; i < bufSize; i++){
        bcc2 ^= buffer[i];
    }

    int count = 4;
    for(int i = 0; i < bufSize; i++){ 
        if(buffer[i] == FLAG || buffer[i] == ESC){ //stuffing FLAG and ESC
            buf[count] = 0x7d;
            count++;
            buf[count] = BCC(buffer[i],0x20);
        }
        else{
            buf[count] = buffer[i];
        }
        count++;
    }

    if(bcc2 == FLAG || bcc2 == ESC){
        buf[count] = 0x7d;
        count++;
        buf[count] = BCC(bcc2,0x20);
    }
    else{
        buf[count] = bcc2;
    }

    buf[count + 1] = FLAG;

    write(fd, buf, count + 2);   //6 is the number of header bytes like flag,a,c,etc...
    return 0;
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    if(connectionParameters.role ==  LlTx){type = TRANSMITER;}
    else{type = READER;}

    parameters = connectionParameters;

    fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(connectionParameters.serialPort);
        return -1;
    }

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        return -1;
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = connectionParameters.baudRate | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 1; // Inter-character timer unused
    newtio.c_cc[VMIN] = 0;  // Blocking read until N chars received

    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        return -1;
    }

    printf("New termios structure set\n");

    if(type == TRANSMITER){
        unsigned char buf[SUPERVISION_SIZE] = {FLAG,A_SENDER,SET,A_SENDER^SET,FLAG};
        int bytes = write(fd, buf, SUPERVISION_SIZE);
        alarm(parameters.timeout);
        setStateMachine(UA_RES,TRANSMITER);
        int counter = 0;
        while(STOP == FALSE){
            unsigned char buf_2[1] = {0};
            bytes = read(fd,buf_2,1);
            if(bytes > 0){
                stateMachine(buf_2[0]);
            }
            if(state_machine_get_state() == DONE){
                STOP = TRUE;
                /*struct state_machine state_machine = getStateMachine();
                printf("A=0x%02X,C=0x%02X,BCC1=0x%02X\n",state_machine.a,state_machine.c,state_machine.bcc);*/
                alarm(0);
            }
            else if(counter == parameters.nRetransmissions){
                alarm(0);
                return -1;
            }
            else if(alarmEnabled == TRUE){  
                alarmEnabled = FALSE;
                bytes = write(fd, buf, SUPERVISION_SIZE);
                //printf("A=0x%02X,C=0x%02X,BCC1=0x%02X\n",buf[1],buf[2],buf[3]);
                counter++;
                alarm(parameters.timeout);
            }
        } 
        
    }
    else{
        unsigned char buf[1] = {}; 
        unsigned char bufs[SUPERVISION_SIZE] = {};
        
        setStateMachine(SET_RES,READER);
        while (STOP == FALSE)
        {   
            int bytes = read(fd, buf, 1);
            if(bytes > 0){
                //printf("byte sent =0x%02x\n",buf[0]);
                stateMachine(buf[0]);
            }
            if (state_machine_get_state() == DONE){
                bufs[0] = FLAG;
                bufs[1] = A_SENDER;
                bufs[2] = UA;
                bufs[3] = BCC(A_SENDER,UA);
                bufs[4] = FLAG;
                            
                //printf("A=0x%02x,C=0x%02x,BCC=0x%02x\n", bufs[1],bufs[2],bufs[3]);
                
                write(fd, bufs, SUPERVISION_SIZE);
                STOP = TRUE;
                
            }
        }
    }

    return 1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    resetStateMachine();
    setStateMachine(RR_REC,TRANSMITER);
    STOP = FALSE;
    writeInfo(buf,bufSize); //the 4 number is also passed

    alarm(parameters.timeout);
    int counter = 0;

    while(!STOP){ //this will be inside another loop so we can send multiple data, but for now it will stay like this
       unsigned char buf_2[1] = {0};
       int bytes = read(fd,buf_2,1);
       if(bytes > 0){
        stateMachine(buf_2[0]);
       }

       if(state_machine_get_state() == DONE){
          struct state_machine state_machine = getStateMachine();
          if(state_machine.mode == RJ_REC){
            alarm(0);
            if(state_machine.c == REJ(0)){
                frameNumber = 0;
            }
            else if(state_machine.c == REJ(1)){
                frameNumber = 1;
            }
            else{}
            resetStateMachine();
            setStateMachine(RR_REC,TRANSMITER);
            writeInfo(buf,bufSize);
            alarm(parameters.timeout);
            counter = 0;
            continue;
          }else{
            STOP = TRUE;
            if(state_machine.c == RR(0) && frameNumber != 0){
                frameNumber = 0;
            }
            else if(state_machine.c == RR(1) && frameNumber != 1){
                frameNumber = 1;
            }
            else{
                //repeated frame, I do nothing I guess
            }
            alarm(0);
          }
       }
       else if(counter == parameters.nRetransmissions){
        alarm(0);
        return -1;
       }
       else if(alarmEnabled == TRUE){  
          alarmEnabled = FALSE;
          writeInfo(buf,bufSize);
          counter++;
          alarm(parameters.timeout);
       }
    }
    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    resetStateMachine();
    setStateMachine(I_REC,READER);
    STOP = FALSE;

    while (STOP == FALSE)
    {
        unsigned char buf[1] = {0};
        int bytes = read(fd, buf, 1);
        if(bytes > 0){
            //printf("byte received =0x%02x\n",buf[0]);
            stateMachine(buf[0]);
        }
        //printf("state = %d\n",state_machine_get_state());
        if (state_machine_get_state() == DONE){
            struct state_machine state_machine = getStateMachine();
            if(state_machine.mode == DISC_REC){
                STOP = TRUE;
                break;
            }
            unsigned char bufs[SUPERVISION_SIZE] = {};
            bufs[0] = FLAG;
            bufs[1] = A_SENDER;

            if(state_machine.mode != RJ_REC){ //Success upon reading
                if(state_machine.c == CTRL_S(0)){
                    if(frameNumber == 0){ //not repeated packet
                        memcpy(packet,state_machine.data,sizeof(char));
                    }
                    bufs[2] = RR(1);
                    frameNumber = 1;
                }else{
                    if(frameNumber == 1){ //not repeated packet
                        memcpy(packet,state_machine.data,sizeof(char));
                    }
                    bufs[2] = RR(0);
                    frameNumber = 1;
                }
            }
            else{
                if(state_machine.c == CTRL_S(0)){  //in case it was rejected
                    bufs[2] = REJ(0);
                }else{
                    bufs[2] = REJ(1);
                }
            }
            bufs[3] = BCC(bufs[1],bufs[2]);
            bufs[4] = FLAG;
                        
            //printf("A=0x%02x,C=0x%02x,BCC=0x%02x\n", bufs[1],bufs[2],bufs[2]);
            
            write(fd, bufs, SUPERVISION_SIZE);
            
            STOP = TRUE;
        }
    }
    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    if(type == TRANSMITER){
        resetStateMachine();
        setStateMachine(DISC_REC,TRANSMITER);
        unsigned char buf[SUPERVISION_SIZE] = {FLAG,A_SENDER,DISC,A_SENDER^DISC,FLAG};
        int bytes = write(fd, buf, SUPERVISION_SIZE);
        //printf("A=0x%02X,C=0x%02X,BCC1=0x%02X\n",buf[1],buf[2],buf[3]);
        STOP = FALSE;
        int counter = 0;
        alarm(parameters.timeout);
        
        while(STOP == FALSE){
            unsigned char buf_2[1] = {0};
            bytes = read(fd,buf_2,1);
            if(bytes > 0){
                stateMachine(buf_2[0]);
            }
            if(state_machine_get_state() == DONE){
                STOP = TRUE;
                //printf("A=0x%02X,C=0x%02X,BCC1=0x%02X\n",state_machine.a,state_machine.c,state_machine.bcc);
                buf[0] = FLAG;
                buf[1] = A_SENDER;
                buf[2] = UA;
                buf[3] = BCC(A_SENDER, UA);
                buf[4] = FLAG;
                write(fd, buf, SUPERVISION_SIZE);
                //printf("A=0x%02X,C=0x%02X,BCC1=0x%02X\n",buf[1],buf[2],buf[3]);
                alarm(0);
            }
            else if(counter == parameters.nRetransmissions){
                alarm(0);
                return -1;
            }
            else if(alarmEnabled == TRUE){  
                alarmEnabled = FALSE;
                bytes = write(fd, buf, SUPERVISION_SIZE);
                //printf("A=0x%02X,C=0x%02X,BCC1=0x%02X\n",buf[1],buf[2],buf[3]);
                counter++;
                alarm(parameters.timeout);
            }
        }
    }
    else{

        unsigned char bufs[SUPERVISION_SIZE] = {0};
        bufs[0] = FLAG;
        bufs[1] = A_RECEIVER;
        bufs[2] = DISC;
        bufs[3] = BCC(A_RECEIVER,DISC);
        bufs[4] = FLAG;
                    
        //printf("A=0x%02x,C=0x%02x,BCC=0x%02x", bufs[1],bufs[2],bufs[3]);
        
        write(fd, bufs, SUPERVISION_SIZE);
               

        //FETCH UA to finish this
        STOP = FALSE;
        resetStateMachine();
        setStateMachine(UA_RES,READER);
        while (STOP == FALSE)
        {
            unsigned char buf[1] = {0};
            int bytes = read(fd, buf, 1);
            if(bytes > 0){
                //printf("byte received =0x%02x\n",buf[0]);
                stateMachine(buf[0]);
            }
            if (state_machine_get_state() == DONE){
                STOP = TRUE;   
            }
        }
    }

    sleep(1);
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);
    return 1;
}
