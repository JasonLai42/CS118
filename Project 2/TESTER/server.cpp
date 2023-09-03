#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>

#include "constants.h"
#include "segment.hpp"
#include "transferlog.hpp"
#include "helperfunc.hpp"


int main(int argc, char** argv) {
    int sockfd; // listen on sockfd
    struct sockaddr_in server_addr; // my addr
    struct sockaddr_in client_addr; // connector addr
    unsigned int sin_size = sizeof(struct sockaddr_in);
    
    /* READ IN SERVER PORT NUMBER FROM STDIN */
    char* p;
    int serverport;
    if(argc != 2) {
        fprintf(stderr, "%s", "Provide a valid <PORT>\n");
        exit(1);
    }
    else {
        serverport = (int)strtol(argv[1], &p, 10);
        if(errno != 0 || *p != '\0' || serverport > INT_MAX) {
            fprintf(stderr, "%s", "Failed to set serverport from argument\n");
            exit(1);
        }
    }
    
    /* CREATE A UDP SOCKET (SOCK_DGRAM FOR UDP) */
    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }
    
    /* FOR DEBUGGING: lets us rerun server after killing it */
    // REMOVE LATER
    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval , sizeof(int));
    
    /* SET ADDRESS INFO */
    memset(&client_addr, 0, sizeof(client_addr));
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(serverport); // short, network byte order
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // long, network byte order
    
    /* BIND THE SOCKET */
    if(bind(sockfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)) < 0) {
        perror("bind");
        exit(1);
    }
    
    /* DATA TRANSFER */
    ssize_t recv_len;
    unsigned char rec_buf[MAX_PACKETSIZE];
    unsigned char send_buf[MAX_PACKETSIZE];

    /* FILE CREATION VARIABLES */
    int connection_num = 1;
    FILE* fp;

    /* RDT AND PIPELINE */
    unsigned short seq_num;
    unsigned short ack_num;
    double RTO_start;
    double RTO_elapsed;
    unsigned short expected_seq;
    Segment last_sent;
    Segment last_fin;

    /* FLOW CONTROL */
    // Distinguish duplicate SYNs
    int CLIENT_SEQ_SET = FALSE;
    // Enable ability to accept other segments besides SYN
    int CONN_EST = FALSE;
    // Go into teardown mode: retransmit FINACK, FIN, ACK only
    int PROC_TEARDOWN = FALSE;
    // Count attempts to send FIN to client; after 4 retransmissions we close the connection
    // To detect client closed is by ICMP connect() and recv()
    int FINs_sent = 0;

    /* CONNECTION */
    while(1) {
        // FIN retransmission timer
        if(PROC_TEARDOWN && (RTO_elapsed > MAX_RTO)) {
            print_timeout(last_fin.header.seq_num);

            memset(&send_buf, 0, MAX_PACKETSIZE);
            pack_buf(send_buf, last_fin);
            if(sendto(sockfd, (const char*)send_buf, MAX_PACKETSIZE, 0, (struct sockaddr*)&client_addr, sin_size) < 0) {
                perror("sendto");
                exit(1);
            }
            print_resend(last_fin.header.seq_num, last_fin.header.ack_num, LOG_FIN);

            RTO_start = start_timer();
            RTO_elapsed = get_elapsed(RTO_start);

            FINs_sent++;
            if(FINs_sent >= 5) {
                fclose(fp);
                connection_num++;

                CLIENT_SEQ_SET = FALSE;
                CONN_EST = FALSE;
                PROC_TEARDOWN = FALSE;
                FINs_sent = 0;
            }
        }

        memset(&rec_buf, 0, MAX_PACKETSIZE);
        if(!(PROC_TEARDOWN)) {
            // Blocking if we're not waiting on timeout for FIN
            if((recv_len = recvfrom(sockfd, (char*)rec_buf, MAX_PACKETSIZE, 0, (struct sockaddr*)&client_addr, &sin_size)) < 0) {
                perror("recvfrom");
                exit(1);
            }
        }
        else {
            // Nonblocking since we're in teardown process and waiting on timeout for FIN server sent
            if((recv_len = recvfrom(sockfd, (char*)rec_buf, MAX_PACKETSIZE, MSG_DONTWAIT, (struct sockaddr*)&client_addr, &sin_size)) < 0) {
                RTO_elapsed = get_elapsed(RTO_start);
                continue;
            }
        }

        // Reconstruct segment from character buffer
        Segment c_REC_seg = unpack_buf(rec_buf);
        
        /* IF RECEIVED SYN */
        if(c_REC_seg.header.SYN == SYN_TRUE) {
            print_recv(c_REC_seg.header.seq_num, c_REC_seg.header.ack_num, LOG_SYN);

            // Sequence number mismatch, we've seen this SYN before so resend the SYNACK and log a DUP-ACK
            if(CLIENT_SEQ_SET && (c_REC_seg.header.seq_num != expected_seq)) {
                memset(&send_buf, 0, MAX_PACKETSIZE);
                pack_buf(send_buf, last_sent);
                if(sendto(sockfd, (const char*)send_buf, MAX_PACKETSIZE, 0, (struct sockaddr*)&client_addr, sin_size) < 0) {
                    perror("sendto");
                    exit(1);
                }
                print_send(seq_num, ack_num, LOG_SYNDUPACK);
                continue;
            }

            // Send SYNACK
            // Set number fields
            seq_num = gen_seq();
            ack_num = wrap_seq_ack(c_REC_seg.header.seq_num + c_REC_seg.header.data_length);
            
            // Construct segment
            Segment s_SYNACK_seg(seq_num, ack_num, ACK_TRUE, SYN_TRUE, FIN_FALSE, SETUP_SIZE);

            // Pack segment into buffer
            memset(&send_buf, 0, MAX_PACKETSIZE);
            pack_buf(send_buf, s_SYNACK_seg);
            
            if(sendto(sockfd, (const char*)send_buf, MAX_PACKETSIZE, 0, (struct sockaddr*)&client_addr, sin_size) < 0) {
                perror("sendto");
                exit(1);
            }
            print_send(seq_num, ack_num, LOG_SYNACK);

            // No timer for SYNACK, Client will timeout and resend SYN

            expected_seq = ack_num;
            last_sent = s_SYNACK_seg;

            // Increment seq_num after SYNACK is sent since we don't increment on ACK
            seq_num = wrap_seq_ack(seq_num + 1);

            // Open a file for this connection
            char filename[16];
            sprintf(filename, "%d.file", connection_num);
            // Open file; flags: ab = O_WRONLY | O_CREAT | O_APPEND
            if((fp = fopen(filename, "ab")) == NULL) {
                perror("fopen");
                exit(1);
            }

            // We have received a sequence number from a client so expected_seq is now valid for this session
            CLIENT_SEQ_SET = TRUE;
            // We can now receive data
            CONN_EST = TRUE;
        }
        /* CONNECTION ESTABLISHED (misleading, we haven't actually received ACK for SYNACK) */
        else if(CONN_EST) {
            /* IF RECEIVED DATA PACKET OR CLIENT DATA SYN ACK */
            if(!(PROC_TEARDOWN) && (c_REC_seg.header.FIN == FIN_FALSE)) {
                if(c_REC_seg.header.ACK == ACK_FALSE) {
                    print_recv(c_REC_seg.header.seq_num, c_REC_seg.header.ack_num, LOG_XX);
                }
                else {
                    print_recv(c_REC_seg.header.seq_num, c_REC_seg.header.ack_num, LOG_ACK);
                }

                if(c_REC_seg.header.seq_num != expected_seq) {
                    memset(&send_buf, 0, MAX_PACKETSIZE);
                    pack_buf(send_buf, last_sent);
                    if(sendto(sockfd, (const char*)send_buf, MAX_PACKETSIZE, 0, (struct sockaddr*)&client_addr, sin_size) < 0) {
                        perror("sendto");
                        exit(1);
                    }
                    print_send(seq_num, ack_num, LOG_DUPACK);

                    continue;
                }

                // Write data to file
                if(fwrite((rec_buf + 12), 1, c_REC_seg.header.data_length, fp) != c_REC_seg.header.data_length) {
                    perror("fwrite");
                    exit(1);
                }

                // The server only increase its sequence number for syn/fin, so it will not increase seq num for ack only packets
                // seq_num = wrap_seq_ack(seq_num + 1);
                ack_num = wrap_seq_ack(ack_num + c_REC_seg.header.data_length);
        
                Segment s_ACK_seg(seq_num, ack_num, ACK_TRUE, SYN_FALSE, FIN_FALSE, 0);

                memset(&send_buf, 0, MAX_PACKETSIZE);
                pack_buf(send_buf, s_ACK_seg);

                if(sendto(sockfd, (const char*)send_buf, MAX_PACKETSIZE, 0, (struct sockaddr*)&client_addr, sin_size) < 0) {
                    perror("sendto");
                    exit(1);
                }
                print_send(seq_num, ack_num, LOG_ACK);

                expected_seq = ack_num;
                last_sent = s_ACK_seg;

            }
            /* IF RECEIVED FIN */
            else if(c_REC_seg.header.FIN == FIN_TRUE) {
                print_recv(c_REC_seg.header.seq_num, c_REC_seg.header.ack_num, LOG_FIN);

                if(c_REC_seg.header.seq_num != expected_seq) {
                    memset(&send_buf, 0, MAX_PACKETSIZE);
                    pack_buf(send_buf, last_sent);
                    if(sendto(sockfd, (const char*)send_buf, MAX_PACKETSIZE, 0, (struct sockaddr*)&client_addr, sin_size) < 0) {
                        perror("sendto");
                        exit(1);
                    }
                    print_send(last_sent.header.seq_num, last_sent.header.ack_num, LOG_DUPACK);
                    
                    continue;
                }

                // seq_num = 0;
                ack_num = (ack_num + c_REC_seg.header.data_length);
            
                Segment s_ACK_seg(seq_num, ack_num, ACK_TRUE, SYN_FALSE, FIN_FALSE, 0);

                memset(&send_buf, 0, MAX_PACKETSIZE);
                pack_buf(send_buf, s_ACK_seg);
            
                if(sendto(sockfd, (const char*)send_buf, MAX_PACKETSIZE, 0, (struct sockaddr*)&client_addr, sin_size) < 0) {
                    perror("sendto");
                    exit(1);
                }
                print_send(seq_num, ack_num, LOG_ACK);

                expected_seq = ack_num;
                last_sent = s_ACK_seg;

                // seq_num = wrap_seq_ack(seq_num + 1);
                // ack_num = wrap_seq_ack(ack_num + 1);
            
                Segment s_FIN_seg(seq_num, 0, ACK_FALSE, SYN_FALSE, FIN_TRUE, TEARDOWN_SIZE);

                memset(&send_buf, 0, MAX_PACKETSIZE);
                pack_buf(send_buf, s_FIN_seg);
            
                if(sendto(sockfd, (const char*)send_buf, MAX_PACKETSIZE, 0, (struct sockaddr*)&client_addr, sin_size) < 0) {
                    perror("sendto");
                    exit(1);
                }
                RTO_start = start_timer();
                RTO_elapsed = get_elapsed(RTO_start);

                print_send(seq_num, 0, LOG_FIN);

                last_fin = s_FIN_seg;

                // Signals that we wait for the final ACK before shutting down the connection
                PROC_TEARDOWN = TRUE;
                FINs_sent++;
            }
            /* IF RECEIVED FINAL ACK */
            else if(PROC_TEARDOWN && (c_REC_seg.header.ACK == ACK_TRUE)) {
                print_recv(c_REC_seg.header.seq_num, c_REC_seg.header.ack_num, LOG_ACK);

                // Close the file for this connection and increment the counter
                fclose(fp);
                connection_num++;

                // Connection broken
                CLIENT_SEQ_SET = FALSE;
                CONN_EST = FALSE;
                PROC_TEARDOWN = FALSE;
                FINs_sent = 0;
            }
        }
        
    }
    
    close(sockfd);
    return 0;
}
