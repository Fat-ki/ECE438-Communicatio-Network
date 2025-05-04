/* 
 * File:   receiver_main.c
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
#include <iostream>
#include <queue>

using namespace std;

#define MSS 1000

typedef struct{
    int data_size;
    int ack_num;
    int seq_num;
    char data[MSS];
    int data_type;
}packet;

enum data_type{
    DATA,
    ACK,
    FIN,
    FINACK
};

class Compare {
public:
    bool operator()(packet a, packet b)
    {
        return a.seq_num > b.seq_num;
    }
};

struct sockaddr_in si_me, si_other;

priority_queue<packet, vector<packet>, Compare> pq;
int s, slen;
void diep(char *s) {
    perror(s);
    exit(1);
}

void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
    
    slen = sizeof (si_other);


    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        printf("socket");

    memset((char *) &si_me, 0, sizeof (si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("Now binding\n");
    if (bind(s, (struct sockaddr*) &si_me, sizeof (si_me)) == -1)
        printf("bind");
    
    
    
	/* Now receive data and send acknowledgements */    
    FILE* fp = fopen(destinationFile, "wb");  
    if (fp == NULL){
        printf("Cannot open the destination file");
    } 

    int ack_num = 0;
    while(1){
        packet pkt;
        int recvbytes;
        if((recvbytes = recvfrom(s, &pkt, sizeof(packet), 0, (sockaddr*)&si_other, (socklen_t*)&slen)) == -1){
            printf("recvfrom()");
        }
        else{
            packet ack;
            if(pkt.data_type == DATA){
                if(pkt.seq_num > ack_num){
                    pq.push(pkt);
                }
                else{
                    ack_num += pkt.data_size;
                    fwrite(pkt.data, sizeof(char), pkt.data_size, fp);
                    while (!pq.empty() && pq.top().seq_num == ack_num){
                        fwrite(pq.top().data, sizeof(char), pq.top().data_size, fp);
                        ack_num += pq.top().data_size;
                        pq.pop();
                    }
                }
                //send ack
                ack.data_type = ACK;
                ack.ack_num = ack_num;
                if (sendto(s, &ack, sizeof(packet), 0, (sockaddr*)&si_other, (socklen_t)sizeof (si_other))==-1){
                    cout<<"No"<<endl;
                }
            }
            else if(pkt.data_type == FIN){
                ack.data_type = FINACK;
                if (sendto(s, &ack, sizeof(packet), 0, (sockaddr*)&si_other, (socklen_t)sizeof (si_other))==-1){
                    cout<<"No"<<endl;
                }
                break;
            }
            
        }
    }
    close(s);
	printf("%s received.", destinationFile);
    return;
}

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int) atoi(argv[1]);

    reliablyReceive(udpPort, argv[2]);
}

