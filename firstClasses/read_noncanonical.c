// Read from serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include "state_machine.h"

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 5

#define FLAG 0x7e
#define A 0x01
#define UA 0x07

#define PACKET_SIZE 100


volatile int STOP = FALSE;

int main(int argc, char *argv[])
{
    // Program usage: Uses either COM1 or COM2
    const char *serialPortName = argv[1];

    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    // Open serial port device for reading and writing and not as controlling tty
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

    // Loop for input
    unsigned char buf[1] = {}; 
    unsigned char bufs[BUF_SIZE] = {};
    
    setStateMachine(SET_RES);
    while (STOP == FALSE)
    {
        
        int bytes = read(fd, buf, 1);
        if(bytes > 0){
            printf("byte sent =0x%02x\n",buf[0]);
            stateMachine(buf[0]);
        }
        if (state_machine_get_state() == DONE){
            bufs[0] = FLAG;
            bufs[1] = A;
            bufs[2] = UA;
            bufs[3] = A ^ UA;
            bufs[4] = FLAG;
                        
            printf("A=0x%02x,", bufs[1]);
            
            printf("C=0x%02x,", bufs[2]);

            printf("BCC=0x%02x,", bufs[3]);
            
            int bytes2 = write(fd, bufs, BUF_SIZE);
            
            printf("Bytes written = %d\n", bytes2);
            
            STOP = TRUE;
              
        }
    }

    printf("Now we go to data transfer\n");

    resetStateMachine();
    setStateMachine(I_REC);
    STOP = FALSE;

    while (STOP == FALSE)
    {
        
        int bytes = read(fd, buf, 1);
        if(bytes > 0){
            printf("byte received =0x%02x\n",buf[0]);
            stateMachine(buf[0]);
        }
        printf("state = %d\n",state_machine_get_state());
        if (state_machine_get_state() == DONE){
            struct state_machine state_machine = getStateMachine();
            bufs[0] = FLAG;
            bufs[1] = A_RECEIVER;
            if(state_machine.c == CTRL_S(0)){
                bufs[2] = RR(1);
            }else{
                bufs[2] = RR(0);
            }
            bufs[3] = BCC(bufs[1],bufs[2]);
            bufs[4] = FLAG;
                        
            printf("A=0x%02x,", bufs[1]);
            
            printf("C=0x%02x,", bufs[2]);

            printf("BCC=0x%02x,", bufs[3]);
            
            int bytes2 = write(fd, bufs, BUF_SIZE);
            
            printf("Bytes written = %d\n", bytes2);
            
            STOP = TRUE;
              
        }
    }



    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}
