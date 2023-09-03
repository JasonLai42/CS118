#include "transferlog.hpp"
#include "constants.h"
#include <stdio.h>
#include <string.h>


void print_recv(unsigned short seq_num, unsigned short ack_num, int code) {
    if(code == LOG_XX) {
        fprintf(stdout, "RECV %u %u\n", seq_num, ack_num);
    }
    else if(code == LOG_SYN) {
        fprintf(stdout, "RECV %u %u SYN\n", seq_num, ack_num);
    }
    else if(code == LOG_SYNACK) {
        fprintf(stdout, "RECV %u %u SYN ACK\n", seq_num, ack_num);
    }
    else if(code == LOG_FIN) {
        fprintf(stdout, "RECV %u %u FIN\n", seq_num, ack_num);
    }
    else if(code == LOG_ACK) {
        fprintf(stdout, "RECV %u %u ACK\n", seq_num, ack_num);
    }

    return;
}

void print_send(unsigned short seq_num, unsigned short ack_num, int code) {
    if(code == LOG_XX) {
        fprintf(stdout, "SEND %u %u\n", seq_num, ack_num);
    }
    else if(code == LOG_SYN) {
        fprintf(stdout, "SEND %u %u SYN\n", seq_num, ack_num);
    }
    else if(code == LOG_SYNACK) {
        fprintf(stdout, "SEND %u %u SYN ACK\n", seq_num, ack_num);
    }
    else if(code == LOG_FIN) {
        fprintf(stdout, "SEND %u %u FIN\n", seq_num, ack_num);
    }
    else if(code == LOG_ACK) {
        fprintf(stdout, "SEND %u %u ACK\n", seq_num, ack_num);
    }
    else if(code == LOG_DUPACK) {
        fprintf(stdout, "SEND %u %u DUP-ACK\n", seq_num, ack_num);
    }
    else if(code == LOG_SYNDUPACK) {
        fprintf(stdout, "SEND %u %u SYN DUP-ACK\n", seq_num, ack_num);
    }

    return;
}

void print_resend(unsigned short seq_num, unsigned short ack_num, int code) {
    if(code == LOG_XX) {
        fprintf(stdout, "RESEND %u %u\n", seq_num, ack_num);
    }
    else if(code == LOG_SYN) {
        fprintf(stdout, "RESEND %u %u SYN\n", seq_num, ack_num);
    }
    else if(code == LOG_SYNACK) {
        fprintf(stdout, "RESEND %u %u SYN ACK\n", seq_num, ack_num);
    }
    else if(code == LOG_FIN) {
        fprintf(stdout, "RESEND %u %u FIN\n", seq_num, ack_num);
    }
    else if(code == LOG_ACK) {
        fprintf(stdout, "RESEND %u %u ACK\n", seq_num, ack_num);
    }

    return;
}

void print_timeout(unsigned short seq_num) {
    fprintf(stdout, "TIMEOUT %u\n", seq_num);

    return;
}

void print_dropped(unsigned short seq_num, unsigned short ack_num, unsigned short ACK, unsigned short SYN) {
    if(ACK == ACK_TRUE) {
        if(SYN == SYN_TRUE) {
            fprintf(stdout, "RECV %u %u SYN ACK\n", seq_num, ack_num);
        }
        else {
            fprintf(stdout, "RECV %u %u ACK\n", seq_num, ack_num);
        }
    }
    else {
        if(SYN == SYN_TRUE) {
            fprintf(stdout, "RECV %u %u SYN\n", seq_num, ack_num);
        }
        else {
            fprintf(stdout, "RECV %u %u\n", seq_num, ack_num);
        }
    }

    return;
}