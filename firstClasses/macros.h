#define FLAG 0x7e

#define A_SENDER 0x03
#define A_RECEIVER 0x01
#define BCC(a,c) (a^c)

#define SET 0x03
#define UA 0x07
#define DIC 0x0b
#define RR(r) ((r == 0) ? 0x05 : 0x85)
#define REJ(r) ((r == 0) ? 0x01 : 0x81)
#define CTRL_S(r) ((r == 0) ? 0x00 : 0x40)
