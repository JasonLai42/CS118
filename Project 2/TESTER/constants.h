#ifndef constants_h
#define constants_h

/* SERVER VALUES */
#define BACKLOG 10

/* UDP PACKET VALUES (in bytes) */
#define HEADER_LEN 12
#define MAX_DATASIZE 512
#define MAX_PACKETSIZE 524
#define SETUP_SIZE 1
#define TEARDOWN_SIZE 1

/* RDT VALUES */
// Sequence number wraps to 0 after hitting max
#define MAX_SEQNUM 25600
// Fixed retransmission timer in seconds (don't need to recalculate)
#define MAX_RTO 0.5
#define LASTCALL_TO 2
// Fixed window size; 10 packets
#define RWND_BYTES 5120
#define RWND 10

/* TRANSFER LOG CODES */
#define LOG_XX 0
#define LOG_SYN 1
#define LOG_SYNACK 2
#define LOG_FIN 3
#define LOG_ACK 4
#define LOG_DUPACK 5
#define LOG_SYNDUPACK 6

/* FLAGS */
// If ACK flag is false, the ACK number for the packet is 0
#define ACK_FALSE 0
#define ACK_TRUE 1
#define SYN_FALSE 0
#define SYN_TRUE 1
#define FIN_FALSE 0
#define FIN_TRUE 1

/* BOOL */
#define FALSE 0
#define TRUE 1

#endif
