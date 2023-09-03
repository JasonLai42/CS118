#ifndef segment_hpp
#define segment_hpp
#include "constants.h"


/* 12 BYTE HEADER FORMAT */
/* Optimal scheme:
 * 2 bytes for sequence number (25600)
 * 2 bytes for acknowledgement number
 * 1 byte for flags (ACK, SYN, FIN)
 * 2 bytes for payload size
 * Increase size for all to take up all 12 bytes,
 * unless we need additional fields
 */
typedef struct Header {
    unsigned short seq_num;
    unsigned short ack_num;
    unsigned short ACK;
    unsigned short SYN;
    unsigned short FIN;
    unsigned short data_length;
} Header;

/* SEGMENT FORMAT: HEADER + PAYLOAD */
class Segment {
public:
    Header header;
    char payload_buf[MAX_DATASIZE];
    
    Segment();
    Segment(short seq, short ack, short f_ack, short f_syn, short f_fin, short data_len);
    
    void set_payload(char* data);
};

Segment unpack_buf(unsigned char* seg_buf);

void pack_buf(unsigned char* seg_buf, Segment send_seg);

#endif
