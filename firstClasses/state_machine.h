enum{
    ESTABLISHMENT,DATATRANSFER,TERMINATION
} operationType;

enum state{START,FLAG_RCV,A_RCV,C_RCV,BCC_RCV,DATA,BCC2_RCV,DONE};
enum machineType{READER,WRITER};

struct state_machine
{
    enum machineType;
    enum state;
    unsigned char a;
    unsigned char c;
    unsigned char bcc;
    unsigned char data[1000];
    unsigned char bcc2;
};

void setStateMachine(struct state_machine &state_machine, enum machineType mt, enum state state, unsigned char a_byte, unsigned char c_byte, unsigned char &data_array, int data_size);


