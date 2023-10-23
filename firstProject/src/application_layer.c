// Application layer protocol implementation

#include "application_layer.h"

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayer connectionParameters;
    connectionParameters.baudRate = baudRate;
    memcpy(connectionParameters.serialPort,serialPort);
    connectionParameters.role = role;
    connectionParameters.nRetransmissions = nTries;
    connectionParameters.timeout = timeout;

    if(llopen(connectionParameters) < 0){
        exit(-1);
    }

    if(role == LlTx){
        FILE *file = fopen(filename, "rb");
        if(file == NULL){
            perror("File not found");
            exit(-1);
        }

        int fileBegin = ftell(file);
        fseek(file,0L,SEEK_END);
        long int fileSize = ftell(file)-fileBegin;
        fseek(file,fileBegin,SEEK_SET);



    }
    else{
        unsigned char *packet[MAX_PAYLOAD_SIZE];
        FILE *outputFile = fopen(filename, "wb+");
        while(1){
            memset(packet,0,MAX_PAYLOAD_SIZE);
            int packet_size = llread(packet);
            if(packet_size == 0){
                break;
            }

            if (fwrite(packet, 1, packet_size, outputFile) != packet_size) {
                perror("Failed to write to the file");
                fclose(outputFile);
                llclose(0);
            }

        }
    }
}
