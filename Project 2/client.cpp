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
#include <deque>

#include "constants.h"
#include "segment.hpp"
#include "transferlog.hpp"
#include "helperfunc.hpp"


int main(int argc, char** argv) {
    int sockfd; // listen on sockfd
    struct sockaddr_in server_addr; // connector addr
    struct hostent* server;
    unsigned int sin_size = sizeof(struct sockaddr_in);
    
    /* READ IN HOSTNAME, PORT NUMBER, FILENAME FROM STDIN */
    char* p;
    int clientport;
    char* hostname;
    char* filename;
    if(argc != 4) {
        fprintf(stderr, "%s", "Provide valid <HOSTNAME/IP> <PORT> <FILENAME>\n");
        exit(1);
    }
    else {
        // Hostname/IP
        hostname = argv[1];
        
        // Client Port
        clientport = (int)strtol(argv[2], &p, 10);
        if(errno != 0 || *p != '\0' || clientport > INT_MAX) {
            fprintf(stderr, "Failed to set serverport from argument %s\n", argv[2]);
            exit(1);
        }
        
        // Filename
        filename = argv[3];
    }
    
    /* CREATE A UDP SOCKET (SOCK_DGRAM) */
    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }
    
    /* GET SERVER'S DNS ENTRY */
    server = gethostbyname(hostname);
    if(server == NULL) {
        fprintf(stderr, "Failed to get DNS of host %s\n", hostname);
        exit(1);
    }
    
    /* SET ADDRESS INFO */
    // Don't use bzero or bcopy; not ISO and maybe deprecated
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(clientport); // short, network byte order
    memcpy((char*)&server_addr.sin_addr.s_addr, (char*)server->h_addr, server->h_length);
    
    /* DATA TRANSFER */
    ssize_t recv_len;
    unsigned char rec_buf[MAX_PACKETSIZE];
    unsigned char send_buf[MAX_PACKETSIZE];

    /* CONTROL FLOW VARIABLES */
    // Flag for indicating connection is up
    int CONN_EST = FALSE;
    // Flag for triggering connection teardown
    int SEND_FIN = FALSE;
    // To know when it's ok to just worry about the FINACK
    int ALL_DATA_ACKD = FALSE;
    // To know when to wait on server FIN
    int WAIT_FOR_FIN = FALSE;
    // Holds start time of RTO for packet loss
    double RTO_start;
    double RTO_elapsed;
    // Variables to handle pipelined data transfer
    int window_space = RWND;
    unsigned short last_rec_ack;
    std::deque <Segment> unacked_segments;
    std::deque <unsigned short> expected_ACKs;
    
    /* OPEN FILE FOR READING */
    FILE* fp;
    if((fp = fopen(filename, "r")) == NULL) {
        perror("fopen");
        exit(1);
    }
    int FILE_BUFFER_SIZE = sizeof(char) * (MAX_DATASIZE + 1);
    char* file_buf = (char*) malloc(FILE_BUFFER_SIZE);
    if(file_buf == NULL) {
        perror("malloc");
        exit(1);
    }
    
    /*********************************************************************************************************************************/

    /* CONNECTION SETUP */
    // 1. Send initial SYN or retransmitted SYN
    // Generate random sequence number
    unsigned short seq_num = gen_seq();
    unsigned short ack_num = 0;
    // Construct segment
    Segment c_SYN_seg(seq_num, ack_num, ACK_FALSE, SYN_TRUE, FIN_FALSE, SETUP_SIZE);
    // Pack segment into buffer
    memset(&send_buf, 0, MAX_PACKETSIZE);
    pack_buf(send_buf, c_SYN_seg);
    if(sendto(sockfd, (const char*)send_buf, MAX_PACKETSIZE, 0, (struct sockaddr*)&server_addr, sin_size) < 0) {
        perror("sendto");
        exit(1);
    }
    RTO_start = start_timer();
    RTO_elapsed = get_elapsed(RTO_start);
    print_send(seq_num, ack_num, LOG_SYN);
    // Increment sequence number for the next expected segment
    seq_num = wrap_seq_ack(seq_num + 1);
    // Pipeline window management
    unacked_segments.push_back(c_SYN_seg);
    expected_ACKs.push_back(seq_num);
    window_space--;

    // Three-way Handshake
    while(!(CONN_EST)) {
        // 1. Send initial SYN or retransmitted SYN
        if(RTO_elapsed >= MAX_RTO) {
            print_timeout(unacked_segments.front().header.seq_num);

            memset(&send_buf, 0, MAX_PACKETSIZE);
            pack_buf(send_buf, unacked_segments.front());
            if(sendto(sockfd, (const char*)send_buf, MAX_PACKETSIZE, 0, (struct sockaddr*)&server_addr, sin_size) < 0) {
                perror("sendto");
                exit(1);
            }
            RTO_start = start_timer();
            RTO_elapsed = get_elapsed(RTO_start);

            print_resend(unacked_segments.front().header.seq_num, unacked_segments.front().header.ack_num, LOG_SYN);
        }

        // 2. Wait for SYNACK and handle SYNACK or SYN loss
        // Enable nonblocking with MSG_DONTWAIT, or blocking with MSG_WAITALL
        memset(&rec_buf, 0, MAX_PACKETSIZE);
        if((recv_len = recvfrom(sockfd, (char*)rec_buf, MAX_PACKETSIZE, MSG_DONTWAIT, (struct sockaddr*)&server_addr, &sin_size)) < 0) {
            RTO_elapsed = get_elapsed(RTO_start);
            continue;
        }
        // Reconstruct segment from buffer
        Segment s_SYNACK_seg = unpack_buf(rec_buf);
        // Set ack_num to server's sequence number fields
        ack_num = wrap_seq_ack(s_SYNACK_seg.header.seq_num + s_SYNACK_seg.header.data_length);

        // If received SYNACK
        if(s_SYNACK_seg.header.SYN == SYN_TRUE && s_SYNACK_seg.header.ACK == ACK_TRUE) {
            print_recv(s_SYNACK_seg.header.seq_num, s_SYNACK_seg.header.ack_num, LOG_SYNACK);
            // Used to check for DUP-ACK
            last_rec_ack = s_SYNACK_seg.header.ack_num;

            if(expected_ACKs.front() == s_SYNACK_seg.header.ack_num) {
                unacked_segments.pop_front();
                expected_ACKs.pop_front();
                window_space++;
            }
            else {
                continue;
            }

            // 3. Send ACK for SYN and first payload
            memset(file_buf, 0, FILE_BUFFER_SIZE);
            short bytes_read = fread(file_buf, 1, MAX_DATASIZE, fp);
            if((bytes_read != MAX_DATASIZE) && ferror(fp)) {
                perror("fread");
                exit(1);
            }
        
            Segment c_FIRST_seg(seq_num, ack_num, ACK_TRUE, SYN_FALSE, FIN_FALSE, bytes_read);
            c_FIRST_seg.set_payload(file_buf);

            memset(&send_buf, 0, MAX_PACKETSIZE);
            pack_buf(send_buf, c_FIRST_seg);

            if(sendto(sockfd, (const char*)send_buf, MAX_PACKETSIZE, 0, (struct sockaddr*)&server_addr, sin_size) < 0) {
                perror("sendto");
                exit(1);
            }
            RTO_start = start_timer();
            RTO_elapsed = get_elapsed(RTO_start);

            print_send(seq_num, ack_num, LOG_ACK);

            seq_num = wrap_seq_ack(seq_num + bytes_read);
            // The server only increase its sequence number for syn/fin, so it will not increase seq num for ack only packets
            // ack_num = wrap_seq_ack(ack_num + 1);

            unacked_segments.push_back(c_FIRST_seg);
            expected_ACKs.push_back(seq_num);
            window_space--;

            // SYNACK received and ACK sent, connection established
            CONN_EST = TRUE;

            // ACK packet contained all the data we had, start the teardown process
            if(feof(fp)) {
                SEND_FIN = TRUE;
            }
        }
    }

    /*********************************************************************************************************************************/

    /* SEND THE REST OF THE FILE */
    /* Window will hold up to 10 packets
     * Technically, window allows up to 5120 Bytes of data
     * So there could be more than 10 packets in the window if some are not MSS
     * But we always send as much data in a packet as possible (512 Bytes)
     * So we should always be limited to 10 packets in the window
     */
    // Send out 9 more segments (or as many as you can if file is < 4608 Bytes)
    while(!(SEND_FIN) && !(feof(fp)) && (window_space > 0)) {
        memset(file_buf, 0, FILE_BUFFER_SIZE);
        short bytes_read = fread(file_buf, 1, MAX_DATASIZE, fp);
        if((bytes_read != MAX_DATASIZE) && ferror(fp)) {
            perror("fread");
            exit(1);
        }

        Segment c_FILE_seg(seq_num, ack_num, ACK_FALSE, SYN_FALSE, FIN_FALSE, bytes_read);
        c_FILE_seg.set_payload(file_buf);

        memset(&send_buf, 0, MAX_PACKETSIZE);
        pack_buf(send_buf, c_FILE_seg);

        if(sendto(sockfd, (const char*)send_buf, MAX_PACKETSIZE, 0, (struct sockaddr*)&server_addr, sin_size) < 0) {
            perror("sendto");
            exit(1);
        }
        print_send(seq_num, ack_num, LOG_XX);

        seq_num = wrap_seq_ack(seq_num + bytes_read);
        // ack_num = wrap_seq_ack(ack_num + 1);

        unacked_segments.push_back(c_FILE_seg);
        expected_ACKs.push_back(seq_num);
        window_space--;

        if(feof(fp)) {
            SEND_FIN = TRUE;
        }
    }

    // If there is more data left after sending 10 packets, 
    // start receiving ACKs for segments, moving the window, and sending any additional segments
    while(!(SEND_FIN)) {
        if(RTO_elapsed >= MAX_RTO) {
            print_timeout(unacked_segments.front().header.seq_num);

            // Flag to set timer for the first segment in window
            int SET_TIMER = TRUE;
            std::deque<Segment>::iterator it; 
            for(it = unacked_segments.begin(); it != unacked_segments.end(); it++) {
                Segment temp = *it;
                memset(&send_buf, 0, MAX_PACKETSIZE);
                pack_buf(send_buf, temp); 
                if(sendto(sockfd, (const char*)send_buf, MAX_PACKETSIZE, 0, (struct sockaddr*)&server_addr, sin_size) < 0) {
                    perror("sendto");
                    exit(1);
                }
                if(temp.header.ACK == ACK_FALSE) {
                    print_resend(temp.header.seq_num, temp.header.ack_num, LOG_XX);
                }
                else {
                    print_send(temp.header.seq_num, temp.header.ack_num, LOG_DUPACK);
                }

                if(SET_TIMER) {
                    RTO_start = start_timer();
                    RTO_elapsed = get_elapsed(RTO_start);
                    
                    SET_TIMER = FALSE;
                }
            }
        }

        memset(&rec_buf, 0, MAX_PACKETSIZE);
        if((recv_len = recvfrom(sockfd, (char*)rec_buf, MAX_PACKETSIZE, MSG_DONTWAIT, (struct sockaddr*)&server_addr, &sin_size)) < 0) {
            RTO_elapsed = get_elapsed(RTO_start);
            continue;
        }
        // Reconstruct segment from character buffer
        Segment s_REC_seg = unpack_buf(rec_buf);
        
        // If received ACK for segment, we can sent more segments
        if(s_REC_seg.header.ACK == ACK_TRUE) {
            print_recv(s_REC_seg.header.seq_num, s_REC_seg.header.ack_num, LOG_ACK);

            // If we've seen this ack_num before, it's a DUP-ACK, we already popped it from the window, keep waiting for timeout
            if(s_REC_seg.header.ack_num == last_rec_ack) {
                RTO_elapsed = get_elapsed(RTO_start);
                continue;
            }
            else {
                last_rec_ack = s_REC_seg.header.ack_num;
            }

            // Cumulative ACK, if we get an out-of-order ACK before timeout, it means earlier ACKs were lost, but server got the segments
            // For the segments that were ACK'd pop them from the window
            int ACK_UNFOUND = TRUE;
            while(ACK_UNFOUND) {
                if(expected_ACKs.front() == s_REC_seg.header.ack_num) {
                    ACK_UNFOUND = FALSE;
                }
                expected_ACKs.pop_front();
                unacked_segments.pop_front();
                window_space++;
            }

            // We have a new least significant unacked packet, so we update the timer
            RTO_start = start_timer();
            RTO_elapsed = get_elapsed(RTO_start);
        }

        while(!(feof(fp)) && (window_space > 0)) {
            memset(file_buf, 0, FILE_BUFFER_SIZE);
            short bytes_read = fread(file_buf, 1, MAX_DATASIZE, fp);
            if((bytes_read != MAX_DATASIZE) && ferror(fp)) {
                perror("fread");
                exit(1);
            }

            Segment c_FILE_seg(seq_num, ack_num, ACK_FALSE, SYN_FALSE, FIN_FALSE, bytes_read);
            c_FILE_seg.set_payload(file_buf);

            memset(&send_buf, 0, MAX_PACKETSIZE);
            pack_buf(send_buf, c_FILE_seg);

            if(sendto(sockfd, (const char*)send_buf, MAX_PACKETSIZE, 0, (struct sockaddr*)&server_addr, sin_size) < 0) {
                perror("sendto");
                exit(1);
            }
            // Start timer if all previous packets were ACK'd and this is the first packet in the window i.e. maximum window space
            if(window_space == RWND) {
                RTO_start = start_timer();
                RTO_elapsed = get_elapsed(RTO_start);
            }

            print_send(seq_num, ack_num, LOG_XX);

            seq_num = wrap_seq_ack(seq_num + bytes_read);
            // ack_num = wrap_seq_ack(ack_num + 1);

            unacked_segments.push_back(c_FILE_seg);
            expected_ACKs.push_back(seq_num);
            window_space--;

            if(feof(fp)) {
                SEND_FIN = TRUE;
            }
        }
    }

    // /*********************************************************************************************************************************/

    /* CONNECTION TEARDOWN */
    // If last data packet sent (DO_TEARDOWN set), check window space and send initial FIN
    // Handle any ACK's for data or FIN
    while(CONN_EST) {
        if(!(WAIT_FOR_FIN) && (RTO_elapsed >= MAX_RTO)) {
            print_timeout(unacked_segments.front().header.seq_num);

            int SET_TIMER = TRUE;
            std::deque<Segment>::iterator it; 
            for(it = unacked_segments.begin(); it != unacked_segments.end(); it++) {
                Segment temp = *it;
                memset(&send_buf, 0, MAX_PACKETSIZE);
                pack_buf(send_buf, temp); 
                if(sendto(sockfd, (const char*)send_buf, MAX_PACKETSIZE, 0, (struct sockaddr*)&server_addr, sin_size) < 0) {
                    perror("sendto");
                    exit(1);
                }
                if(temp.header.ACK == ACK_FALSE) {
                    if(temp.header.FIN == FIN_FALSE) {
                        print_resend(temp.header.seq_num, temp.header.ack_num, LOG_XX);
                    }
                    else {
                        print_resend(temp.header.seq_num, temp.header.ack_num, LOG_FIN);
                    }
                }
                else {
                    print_send(temp.header.seq_num, temp.header.ack_num, LOG_DUPACK);
                }

                if(SET_TIMER) {
                    RTO_start = start_timer();
                    RTO_elapsed = get_elapsed(RTO_start);
                    
                    SET_TIMER = FALSE;
                }
            }
        }

        if(SEND_FIN && (window_space > 0)) {
            Segment c_FIN_seg(seq_num, 0, ACK_FALSE, SYN_FALSE, FIN_TRUE, TEARDOWN_SIZE);

            memset(&send_buf, 0, MAX_PACKETSIZE);
            pack_buf(send_buf, c_FIN_seg);

            if(sendto(sockfd, (const char*)send_buf, MAX_PACKETSIZE, 0, (struct sockaddr*)&server_addr, sin_size) < 0) {
                perror("sendto");
                exit(1);
            }
            if(window_space == RWND) {
                RTO_start = start_timer();
                RTO_elapsed = get_elapsed(RTO_start);
            }

            print_send(seq_num, 0, LOG_FIN);

            // Increment the seq_num here, after we send FIN, so that all ACKs we from here forth are same seq_num
            seq_num = wrap_seq_ack(seq_num + 1);
            // ack_num = wrap_seq_ack(ack_num + 1);

            unacked_segments.push_back(c_FIN_seg);
            expected_ACKs.push_back(seq_num);
            window_space--;

            SEND_FIN = FALSE;
        }

        // Receive ACKs and wait for FIN
        memset(&rec_buf, 0, MAX_PACKETSIZE);
        if(!(WAIT_FOR_FIN)) {
            if((recv_len = recvfrom(sockfd, (char*)rec_buf, MAX_PACKETSIZE, MSG_DONTWAIT, (struct sockaddr*)&server_addr, &sin_size)) < 0) {
                RTO_elapsed = get_elapsed(RTO_start);
                continue;
            }
        }
        else {
            if((recv_len = recvfrom(sockfd, (char*)rec_buf, MAX_PACKETSIZE, 0, (struct sockaddr*)&server_addr, &sin_size)) < 0) {
                perror("recvfrom");
                exit(1);
            }
        }
        // Reconstruct segment from character buffer
        Segment s_REC_seg = unpack_buf(rec_buf);

        // If received ACK for segment, we increment the window space
        if(s_REC_seg.header.ACK == ACK_TRUE) {
            print_recv(s_REC_seg.header.seq_num, s_REC_seg.header.ack_num, LOG_ACK);

            if(s_REC_seg.header.ack_num == last_rec_ack) {
                RTO_elapsed = get_elapsed(RTO_start);
                continue;
            }
            else {
                last_rec_ack = s_REC_seg.header.ack_num;
            }

            int ACK_UNFOUND = TRUE;
            while(ACK_UNFOUND) {
                if(expected_ACKs.front() == s_REC_seg.header.ack_num) {
                    ACK_UNFOUND = FALSE;
                    if(unacked_segments.front().header.FIN == FIN_TRUE) {
                        WAIT_FOR_FIN = TRUE;
                    }
                }
                expected_ACKs.pop_front();
                unacked_segments.pop_front();
                window_space++;
                if(unacked_segments.front().header.FIN == FIN_TRUE) {
                    ALL_DATA_ACKD = TRUE;
                }
            }

            RTO_start = start_timer();
            RTO_elapsed = get_elapsed(RTO_start);
        }
        // If received server's FIN, we can send ACK and start the two second timer till shutdown
        else if(s_REC_seg.header.FIN == FIN_TRUE) {
            print_recv(s_REC_seg.header.seq_num, s_REC_seg.header.ack_num, LOG_FIN);

            if(ALL_DATA_ACKD) {
                // seq_num = 0;
                ack_num = wrap_seq_ack(ack_num + 1);
                Segment c_ACK_seg(seq_num, ack_num, ACK_TRUE, SYN_FALSE, FIN_FALSE, 0);

                memset(&send_buf, 0, MAX_PACKETSIZE);
                pack_buf(send_buf, c_ACK_seg);

                if(sendto(sockfd, (const char*)send_buf, MAX_PACKETSIZE, 0, (struct sockaddr*)&server_addr, sin_size) < 0) {
                    perror("sendto");
                    exit(1);
                }
                print_send(seq_num, ack_num, LOG_ACK);

                // We've received ACKs for all data packets and FIN, break loop and enter lastcall
                CONN_EST = FALSE;
            }
        }
    }

    // Wait two seconds
    // ACK all incoming FINS; drop all other packets
    double lastcall = start_timer();
    double elapsed = get_elapsed(lastcall);
    while(elapsed < LASTCALL_TO) {
        // Enable nonblocking with MSG_DONTWAIT, or blocking with MSG_WAITALL
        memset(&rec_buf, 0, MAX_PACKETSIZE);
        if((recv_len = recvfrom(sockfd, (char*)rec_buf, MAX_PACKETSIZE, MSG_DONTWAIT, (struct sockaddr*)&server_addr, &sin_size)) < 0) {
            elapsed = get_elapsed(lastcall);
            continue;
        }
        // Reconstruct segment from character buffer
        Segment s_REC_seg = unpack_buf(rec_buf);
        
        // ACK received FINS
        if(s_REC_seg.header.FIN == FIN_TRUE) {
            print_recv(s_REC_seg.header.seq_num, s_REC_seg.header.ack_num, LOG_FIN);
            
            // Sequence number already incremented after the client's FIN was sent
            // seq_num = 0;
            // Increment ack_num since we are expecting ack_num+1 from server's sequence number for each FIN received
            // ack_num = wrap_seq_ack(ack_num + 1);
            Segment c_ACK_seg(seq_num, ack_num, ACK_TRUE, SYN_FALSE, FIN_FALSE, 0);

            memset(&send_buf, 0, MAX_PACKETSIZE);
            pack_buf(send_buf, c_ACK_seg);
            
            if(sendto(sockfd, (const char*)send_buf, MAX_PACKETSIZE, 0, (struct sockaddr*)&server_addr, sin_size) < 0) {
                perror("sendto");
                exit(1);
            }
            print_send(seq_num, ack_num, LOG_DUPACK);
        }
        // Print recv dropped packets
        else {
            print_dropped(s_REC_seg.header.seq_num, s_REC_seg.header.ack_num, s_REC_seg.header.ACK, s_REC_seg.header.SYN);
        }

        elapsed = get_elapsed(lastcall);
    }


    fclose(fp);
    free(file_buf);
    close(sockfd);
    return 0;
}
