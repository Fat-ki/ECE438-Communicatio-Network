/* 
 * File:   sender_main.c
 * Author: 
 *
 * Created on 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>

#include <queue>
#include <iostream>
#include <algorithm> 
#include <cmath>

using namespace std;

#define MSS 1000
#define TIMEOUT 200000

enum data_type{
    DATA,
    ACK,
    FIN,
    FINACK
};

enum status_type{
    SLOW_START,
    CONGESTION_AVOIDANCE,
    FAST_RECOVERY
};

typedef struct{
    int data_size;
    int ack_num;
    int seq_num;
    char data[MSS];
    int data_type;
}packet;

struct sockaddr_in si_other;
int s, slen;

FILE *fp;
float cw = 1.0;
float sst = 64.0;
int dupACK_cnt = 0;
unsigned long long int seq_num = 0;
unsigned long long int bytes_to_send;
unsigned long long int num_tot_pkt;
unsigned long long int num_pkt_received = 0;
int current_status = SLOW_START;

queue<packet> send_buffer;
queue<packet> backup_buffer;


void diep(char *s) {
    perror(s);
    exit(1);
}
void transmit(packet* pkt){
    if (sendto(s, pkt, sizeof(packet), 0, (struct sockaddr*)&si_other, sizeof(si_other))== -1){
        cout << "No" << endl;
    }
}
void timeoutFunction(){
    sst = cw/2;
    cw = 1.0;
    dupACK_cnt = 0;
    current_status = SLOW_START;

    if (!backup_buffer.empty()){
        transmit(&backup_buffer.front());
    }
    
}
void dupACKFunction(){
    sst = cw/2;
    cw = sst+3;
    dupACK_cnt = 0;

    if (!backup_buffer.empty()){
        transmit(&backup_buffer.front());
    }
}
void toPacketandFillBuffer(){
    char buf[MSS];
    for (int i = 0; i < (cw - backup_buffer.size()); i++) {
        packet pkt;
        int size = min(bytes_to_send, (unsigned long long int)MSS);
        int file_size = fread(buf, sizeof(char), size, fp);
        // Prepare packet in the buffer
        if (file_size > 0){
            pkt.data_type = DATA;
            pkt.data_size = file_size;
            pkt.seq_num = seq_num;

            memcpy(pkt.data, &buf, file_size);
            send_buffer.push(pkt);
            backup_buffer.push(pkt);
            seq_num += file_size;
            bytes_to_send -= file_size;
        }
    }
    while(!send_buffer.empty()){
        packet pkt = send_buffer.front(); 
        transmit(&pkt);
        send_buffer.pop();
    }
}

void statusTransition(){
    switch(current_status){
        case SLOW_START:
            if(cw >= sst ){
                current_status = CONGESTION_AVOIDANCE;
                return;
            }
            if(dupACK_cnt >= 3){
                current_status = FAST_RECOVERY;
                dupACKFunction();
            }
            else if (dupACK_cnt == 0){
                cw++;
            }
            break;
        case CONGESTION_AVOIDANCE:
            if(dupACK_cnt >= 3){
                current_status = FAST_RECOVERY;
                dupACKFunction();
            }
            else if(dupACK_cnt == 0){
                cw += 1.0/cw;
            }
            break;
        case FAST_RECOVERY:
            if(dupACK_cnt == 0){
                cw = sst;
                current_status = CONGESTION_AVOIDANCE;
            }
            else{
                cw++;
            }
            break;
        default:
            break;
    }
}

void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    //Open the file
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Could not open file to send.");
        exit(1);
    }

	/* Determine how many bytes to transfer */
    slen = sizeof (si_other);

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        printf("socket");

    memset((char *) &si_other, 0, sizeof (si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(hostUDPport);
    if (inet_aton(hostname, &si_other.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }

    bytes_to_send = bytesToTransfer;
    num_tot_pkt = ceil(bytes_to_send / MSS);
    // set timeout
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = TIMEOUT;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

	/* Send data and receive acknowledgements on s*/
    toPacketandFillBuffer();
    
    while(num_pkt_received < num_tot_pkt){
        packet pkt;
        if ((recvfrom(s, &pkt, sizeof(packet), 0, NULL, NULL)) == -1){
            //time out
            if(!backup_buffer.empty()){
                cout << "Time out, resend packet number" << backup_buffer.front().seq_num << endl;
                timeoutFunction();
            }
        }
        else{
            if(pkt.data_type == ACK){
                //dup ACK
                if(pkt.ack_num == backup_buffer.front().seq_num){
                    dupACK_cnt++;
                    statusTransition();
                }
                //new ACK
                else if(pkt.ack_num > backup_buffer.front().seq_num){
                    dupACK_cnt = 0;
                    while (!backup_buffer.empty() && backup_buffer.front().seq_num < pkt.ack_num) {
                        num_pkt_received++;
                        statusTransition();
                        backup_buffer.pop();
                    }
                    toPacketandFillBuffer();
                }
            }
        }
    }

    // FIN
    packet end_pkt;
    end_pkt.data_type = FIN;    
    end_pkt.data_size = 0;
    transmit(&end_pkt);
    while(1){
        packet pkt;
        if ((recvfrom(s, &pkt, sizeof(packet), 0, NULL, NULL)) == -1){
            transmit(&end_pkt);
        }
        else{
            if(pkt.data_type == FINACK){
                break;
            }
        }
    }
    printf("Closing the socket\n");
    close(s);
    return;

}

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;
    unsigned long long int numBytes;
    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    udpPort = (unsigned short int) atoi(argv[2]);
    numBytes = atoll(argv[4]);

    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);

    return (EXIT_SUCCESS);
}


