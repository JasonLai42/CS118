#ifndef transferlog_hpp
#define transferlog_hpp


void print_recv(unsigned short seq_num, unsigned short ack_num, int code);

void print_send(unsigned short seq_num, unsigned short ack_num, int code);

void print_resend(unsigned short seq_num, unsigned short ack_num, int code);

void print_timeout(unsigned short seq_num);

void print_dropped(unsigned short seq_num, unsigned short ack_num, unsigned short ACK, unsigned short SYN);

#endif
