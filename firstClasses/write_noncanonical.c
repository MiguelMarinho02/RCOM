// Write to serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include "state_machine.h"
#include <unistd.h>

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 5
#define FLAG 0x7e
#define ADDR_SENDER 0x03
#define ADDR_REC 0x01
#define SET 0x03
#define UA 0x07
#define TRIES 5
#define PACKET_SIZE 100

volatile int STOP = FALSE;

volatile int alarmEnabled = FALSE;

void alarmHandler(int signal)
{
    alarmEnabled = TRUE;
}

int closePort(int fd, struct termios *oldtio){

    sleep(1);
    if (tcsetattr(fd, TCSANOW, oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);
    return 0;
}

int writePackageInfo(int fd, unsigned char buffer[], int size, int frameNumber){
    unsigned char buf[206];
    
    buf[0] = FLAG;
    buf[1] = A_SENDER;
    buf[2] = CTRL_S(frameNumber);
    buf[3] = BCC(buf[1],buf[2]);
    printf("A=0x%02X,C=0x%02X,BCC1=0x%02X,",buf[1],buf[2],buf[3]);

    unsigned char bcc2 = buffer[0];
    for(int i = 0; i < size; i++){
        bcc2 ^= buffer[i];
    }

    int count = 4;
    for(int i = 0; i < size; i++){ 
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
    printf("BCC2=0x%02X\n",buf[count]);
    buf[count + 1] = FLAG;

    write(fd, buf, count + 2);   //6 is the number of header bytes like flag,a,c,etc...
    return 0;
}

int main(int argc, char *argv[])
{
    // Program usage: Uses either COM1 or COM2
    const char *serialPortName = argv[1];
    (void)signal(SIGALRM, alarmHandler);

    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    // Open serial port device for reading and writing, and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);

    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 1; // Inter-character timer unused
    newtio.c_cc[VMIN] = 0;  // Blocking read until 5 chars received

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    // Create string to send
    unsigned char bcc = SET ^ ADDR_SENDER;
    unsigned char buf[BUF_SIZE] = {FLAG,ADDR_SENDER,SET,bcc,FLAG};

    int bytes = write(fd, buf, BUF_SIZE);
    printf("%d bytes written\n", bytes);
    printf("A=0x%02X,C=0x%02X,BCC1=0x%02X\n",buf[1],buf[2],buf[3]); 
    alarm(3);

    int counter = 1;

    setStateMachine(UA_RES,TRANSMITER);
    while(STOP == FALSE){
       unsigned char buf_2[BUF_SIZE] = {};
       bytes = read(fd,buf_2,1);
       if(bytes > 0){
        stateMachine(buf_2[0]);
       }
       if(state_machine_get_state() == DONE){
          struct state_machine state_machine = getStateMachine();
          STOP = TRUE;
          printf("A=0x%02X,C=0x%02X,BCC1=0x%02X\n",state_machine.a,state_machine.c,state_machine.bcc);
          alarm(0);
       }
       else if(counter == TRIES){
        alarm(0);
        closePort(fd,&oldtio);
        exit(0);
       }
       else if(alarmEnabled == TRUE){  
          alarmEnabled = FALSE;
          bytes = write(fd, buf, BUF_SIZE);
          printf("A=0x%02X,C=0x%02X,BCC1=0x%02X\n",buf[1],buf[2],buf[3]);
          counter++;
          printf("Try %d\n", counter);
          alarm(3);
       }
    }    

    printf("Now we go to data transfer\n");

    resetStateMachine();
    setStateMachine(RR_REC,TRANSMITER);
    STOP = FALSE;
    unsigned char info_buf[PACKET_SIZE] = {0x7e,0x7e,0x7d,0x40}; //this is given by app layer I think
    //this is also fucked because of bcc2. Lets say we are in the middle of the data field. If the bcc of the previous numbers is somehow equal to the actual bcc2, it will skip the other half of the data.
    //Currently have no idea on how to solve this. Maybe I need to ESC it?
    writePackageInfo(fd,info_buf,4,0); //the 4 number is also passed
    int frameNumber = 0;

    alarm(3);
    counter = 0;

    while(!STOP){ //this will be inside another loop so we can send multiple data, but for now it will stay like this
       unsigned char buf_2[BUF_SIZE] = {};
       bytes = read(fd,buf_2,1);
       if(bytes > 0){
        stateMachine(buf_2[0]);
       }

       if(state_machine_get_state() == DONE){
          struct state_machine state_machine = getStateMachine();
          STOP = TRUE;
          printf("A=0x%02X,C=0x%02X,BCC1=0x%02X\n",state_machine.a,state_machine.c,state_machine.bcc);
          if(state_machine.c == RR(0)){
            frameNumber = 0;
          }
          else{
            frameNumber = 1;
          }
          alarm(0);
       }
       else if(counter == TRIES){
        alarm(0);
        closePort(fd,&oldtio);
        exit(0);
       }
       else if(alarmEnabled == TRUE){  
          alarmEnabled = FALSE;
          writePackageInfo(fd,info_buf,4,frameNumber);
          counter++;
          printf("Try %d\n", counter);
          alarm(3);
       }
    }

    //Now we will disconnect
    printf("Now we go to data disconnect\n");
    resetStateMachine();
    setStateMachine(DISC_REC,TRANSMITER);
    buf[0] = FLAG;
    buf[1] = A_SENDER;
    buf[2] = DISC;
    buf[3] = BCC(A_SENDER, DISC);
    buf[4] = FLAG;
    bytes = write(fd, buf, BUF_SIZE);
    printf("A=0x%02X,C=0x%02X,BCC1=0x%02X\n",buf[1],buf[2],buf[3]);
    STOP = FALSE;
    
    while(STOP == FALSE){
       unsigned char buf_2[BUF_SIZE] = {};
       bytes = read(fd,buf_2,1);
       if(bytes > 0){
        stateMachine(buf_2[0]);
       }
       //printf("state = %d\n",state_machine_get_state());
       if(state_machine_get_state() == DONE){
          struct state_machine state_machine = getStateMachine();
          STOP = TRUE;
          printf("A=0x%02X,C=0x%02X,BCC1=0x%02X\n",state_machine.a,state_machine.c,state_machine.bcc);
          buf[0] = FLAG;
          buf[1] = A_SENDER;
          buf[2] = UA;
          buf[3] = BCC(A_SENDER, UA);
          buf[4] = FLAG;
          write(fd, buf, BUF_SIZE);
          printf("A=0x%02X,C=0x%02X,BCC1=0x%02X\n",buf[1],buf[2],buf[3]);
          alarm(0);
       }
       else if(counter == TRIES){
        alarm(0);
        closePort(fd,&oldtio);
        exit(0);
       }
       else if(alarmEnabled == TRUE){  
          alarmEnabled = FALSE;
          bytes = write(fd, buf, BUF_SIZE);
          printf("A=0x%02X,C=0x%02X,BCC1=0x%02X\n",buf[1],buf[2],buf[3]);
          counter++;
          printf("Try %d\n", counter);
          alarm(3);
       }
    }

    // Wait until all bytes have been written to the serial port
    sleep(1);

    // Restore the old port settings
    closePort(fd,&oldtio);

    return 0;
}
