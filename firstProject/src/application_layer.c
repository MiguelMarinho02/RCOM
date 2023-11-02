// Application layer protocol implementation

#include "application_layer.h"

void getControlPacket(int type, unsigned char *packet, long int fileSize, const char *filename, int *size) {
    // Calculate the size of fileSize and filename in bytes
    size_t L1 = sizeof(fileSize);
    size_t L2 = strlen(filename);

    // Store these sizes in the packet
    packet[0] = type;
    packet[1] = 0;
    packet[2] = (unsigned char)L1; // Cast L1 to unsigned char

    int cont = 3; // Start from the next position

    // Store fileSize in little-endian format
    for (size_t i = 0; i < L1; i++) {
        unsigned char byte = (unsigned char)((fileSize >> (8 * i)) & 0xFF);
        packet[cont] = byte;
        cont++;
    }

    packet[cont] = 1; // Indicates type filename
    cont++;
    packet[3] = (unsigned char)L2; // Cast L2 to unsigned char
    cont++;

    // Copy the filename into the packet
    for (size_t i = 0; i < L2; i++) {
        packet[cont] = filename[i];
        cont++;
    }

    *size = cont;
}

void buildDataPacket(unsigned char *controlPacket,unsigned char *packet,int size){
    controlPacket[0] = 1;
    controlPacket[1] = size / 256;
    controlPacket[2] = size % 256;

    for (size_t i = 0; i < size; i++) {
        controlPacket[3+i] = packet[i];
    }
}


void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayer connectionParameters;
    connectionParameters.baudRate = baudRate;
    strcpy(connectionParameters.serialPort,serialPort);
    connectionParameters.role = strcmp(role, "tx") ? LlRx : LlTx;
    connectionParameters.nRetransmissions = nTries;
    connectionParameters.timeout = timeout;

    if(llopen(connectionParameters) < 0){
        exit(-1);
    }

    if(connectionParameters.role == LlTx){
        FILE *file = fopen(filename, "rb");
        unsigned char packet[MAX_PAYLOAD_SIZE];
        if(file == NULL){
            perror("File not found");
            exit(-1);
        }

        struct stat file_st;
        if (stat(filename, &file_st) != 0) {
            printf("Error in stat\n");
            exit(1);
        }
        size_t fileSize = file_st.st_size;

        int packetSize = 0;
        getControlPacket(2,packet,fileSize,filename,&packetSize);
        llwrite(packet,packetSize);  
        int bytesLeft = fileSize;

        while (bytesLeft >= 0) { 
                int dataSize = bytesLeft > (long int) MAX_PAYLOAD_SIZE ? MAX_PAYLOAD_SIZE : bytesLeft;
                fread(packet, sizeof(unsigned char), dataSize, file);
                unsigned char controlPacket[MAX_PAYLOAD_SIZE + 3];
                buildDataPacket(controlPacket,packet,dataSize);
                if(llwrite(controlPacket, dataSize+3) == -1) {
                    printf("Exit: error in data packets\n");
                    exit(-1);
                }

                /*for(int i = 0; i < dataSize; i++){
                    printf("%d,",packet[i]);
                }

                printf("\n\n");*/
                
                bytesLeft -= (long int) MAX_PAYLOAD_SIZE; 
        }

        fclose(file);    
        getControlPacket(3,packet,fileSize,filename,&packetSize);
        llwrite(packet,packetSize); 
    }
    else{
        unsigned char packet[MAX_PAYLOAD_SIZE];
        unsigned char controlPacket[MAX_PAYLOAD_SIZE + 3];
        FILE *outputFile = fopen(filename, "wb+");
        int packet_size;
        while((packet_size = llread(controlPacket)) > 0){
            if(controlPacket[0] == 2){
                continue;;
            }
            if(controlPacket[0] == 3){
                break;
            }
            if(controlPacket[0] == 1){
                /*for(int i = 0; i < packet_size; i++){
                printf("%d,",packet[i]);
                }

                printf("\n\n");*/

                memcpy(packet,controlPacket+3,packet_size-3);

                if (fwrite(packet, 1, packet_size-3, outputFile) != packet_size-3) {
                    perror("Failed to write to the file");
                    fclose(outputFile);
                    llclose(0);
                }
            }

        }

        fclose(outputFile);
    }

    llclose(0);
}
