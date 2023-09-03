#include "segment.hpp"
#include <stdio.h>
#include <string.h>


Segment::Segment() {}

Segment::Segment(short seq, short ack, short f_ack, short f_syn, short f_fin, short data_len) {
    header.seq_num = seq;
    header.ack_num = ack;
    header.ACK = f_ack;
    header.SYN = f_syn;
    header.FIN = f_fin;
    header.data_length = data_len;
    memset(payload_buf, 0, MAX_DATASIZE);
}

void Segment::set_payload(char* data) {
    for(int i = 0; i < header.data_length; i++) {
        payload_buf[i] = data[i];
    }

    return;
}

Segment unpack_buf(unsigned char* seg_buf) {
    short seq_num = (seg_buf[1] << 8) | seg_buf[0];
    short ack_num = (seg_buf[3] << 8) | seg_buf[2];
    short ACK = (seg_buf[5] << 8) | seg_buf[4];
    short SYN = (seg_buf[7] << 8) | seg_buf[6];
    short FIN = (seg_buf[9] << 8) | seg_buf[8];
    short data_len = (seg_buf[11] << 8) | seg_buf[10];
    
    Segment rec_seg(seq_num, ack_num, ACK, SYN, FIN, data_len);
    
    return rec_seg;
}

void pack_buf(unsigned char* seg_buf, Segment send_seg) {
    int buf_pos = 0;
    int field_size = sizeof(unsigned short);
    
    memcpy(seg_buf, &send_seg.header.seq_num, field_size);
    buf_pos += field_size;
    memcpy((seg_buf + buf_pos), &send_seg.header.ack_num, field_size);
    buf_pos += field_size;
    memcpy((seg_buf + buf_pos), &send_seg.header.ACK, field_size);
    buf_pos += field_size;
    memcpy((seg_buf + buf_pos), &send_seg.header.SYN, field_size);
    buf_pos += field_size;
    memcpy((seg_buf + buf_pos), &send_seg.header.FIN, field_size);
    buf_pos += field_size;
    memcpy((seg_buf + buf_pos), &send_seg.header.data_length, field_size);
    buf_pos += field_size;
    memcpy((seg_buf + buf_pos), send_seg.payload_buf, send_seg.header.data_length);

    return;
}
