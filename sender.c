#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<stdbool.h>
#include<unistd.h>
#include<fcntl.h>
#include<errno.h>
#include<arpa/inet.h>
#include<sys/time.h>

#define PORT_USED 8080
#define SIZE_CHUNK 5
#define TOTAL_DATA_SIZE 1024
#define MAX_RESPONSE_SIZE 1024
#define MAX_CHUNKS (MAX_RESPONSE_SIZE/SIZE_CHUNK)
#define SIZE_SEND 5


struct packet {
    int number_of_chunks;
    int sequence_number;
    char temp[SIZE_CHUNK]; 
};

struct ack {
    int ack_number;
};

void time_handling(int flag,int fileD) {
    struct timeval timehadle;
    if(!flag) {
        timehadle.tv_sec = 0;
        timehadle.tv_usec = 0;
    }
    else {
        timehadle.tv_sec = 0;
        timehadle.tv_usec = 100000;
    }

    if(setsockopt(fileD,SOL_SOCKET,SO_RCVTIMEO,&timehadle,sizeof(timehadle)) < 0) {
        perror("Error in setting socket timeout");
        exit(0);
    }
}

void nonBlocking(int fileD) {
    int rand = fcntl(fileD,F_GETFL,0);
    if(rand < 0) {
        perror("Error in F_GETFL");
        exit(0);
    } 
    if(fcntl(fileD,F_SETFL,rand | O_NONBLOCK) < 0) {
        perror("Error in F_SETFL O_NONBLOCK");
        exit(0);
    }
}

void handling_incoming_chunks(int recv_fileD) {
    struct packet packet_recv;
    struct ack ack_recv;
    struct sockaddr_in addr;
    int addr_len = sizeof(addr);
    int chunks_in_total = 0;
    int counter_for_ack = 0;
    int counter_for_packet = 0;
    int counter_for_chunks = 0;

    while(1) {
        int b_read = recvfrom(recv_fileD,&packet_recv,sizeof(packet_recv),0,(struct sockaddr *)&addr,&addr_len);
        if(b_read < 0) {
            if(errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("Error in receiving the data");
            }
            continue;
        }
        if(b_read > 0) {
            printf("Recevied chunk %d: %s\n",packet_recv.sequence_number,packet_recv.temp);
            // printf("Chunks in total: %d\n",chunks_in_total);
            if(chunks_in_total == 0) {
                chunks_in_total = packet_recv.number_of_chunks;
                printf("Total chunks to be received: %d\n",chunks_in_total);
            }
            ack_recv.ack_number = packet_recv.sequence_number;
            counter_for_ack = counter_for_ack + 1;

            if(counter_for_ack % 3 != 0) {
                if(sendto(recv_fileD,&ack_recv,sizeof(ack_recv),0,(struct sockaddr *)&addr,addr_len) < 0) {
                    perror("Error in sending the data");
                }
                else {
                    printf("ACK sent for the chunk %d\n",ack_recv.ack_number);
                    counter_for_packet = counter_for_packet + 1;
                }
            } 
            else {
                printf("Skipped ACK for the chunk %d\n",ack_recv.ack_number);
            }
            counter_for_chunks++;

            if(counter_for_packet == chunks_in_total) {
                printf("Received chunks of total %d\n.Now server will send the data.\n",chunks_in_total);
                break;
            }
        }
    }
}

void handling_outgoing_chunks_helper(struct sockaddr_in *send_addr,int send_fileD,struct packet *send_packet) {
    socklen_t send_len = sizeof(*send_addr);
    if(sendto(send_fileD,send_packet,sizeof(*send_packet),0,(struct sockaddr *)send_addr,send_len) < 0) {
        if(errno != EWOULDBLOCK && errno != EAGAIN) {
            perror("Error in sending the chunks");
            exit(0);
        }
    }
    else {
        printf("Chunk %d sent with chunk data: %s\n",send_packet->sequence_number,send_packet->temp);
    }
}
int receving_ack(struct sockaddr_in *recv_addr,int recv_fileD,struct ack *recv_ack);
void handling_outgoing_chunks(struct sockaddr_in *send_addr,int send_fileD,char *response,int len_response) {
    int final_chunks_in_total = (len_response + SIZE_CHUNK - 1) / SIZE_CHUNK;
    int counter_chunks = 0;
    int upcoming_sequence_num = 0;
    int acknowleged_array[MAX_CHUNKS] = {0};

    printf("Total number of chunks to be sent: %d\n",((len_response + SIZE_CHUNK -1) / SIZE_CHUNK));

    struct packet send_packet[MAX_RESPONSE_SIZE];
    struct ack send_ack;

    for(int i = 0;i < final_chunks_in_total;i++) {
        send_packet[i].number_of_chunks = final_chunks_in_total;
        send_packet[i].sequence_number = i;
        memcpy(send_packet[i].temp,response+i*SIZE_CHUNK,SIZE_CHUNK);
        send_packet[i].temp[SIZE_CHUNK] = '\0';
    } 

    struct timeval present_time;
    struct timeval retrans_time;

    gettimeofday(&retrans_time,NULL);

    for(;counter_chunks < final_chunks_in_total;) {
        for(;upcoming_sequence_num < counter_chunks+SIZE_SEND && upcoming_sequence_num < final_chunks_in_total; upcoming_sequence_num++) {
            if(!acknowleged_array[upcoming_sequence_num]) {
                handling_outgoing_chunks_helper(send_addr,send_fileD,&send_packet[upcoming_sequence_num]);
                gettimeofday(&retrans_time,NULL);
            }
        }
        for(;receving_ack(send_addr,send_fileD,&send_ack);) {
            if(send_ack.ack_number >= counter_chunks && send_ack.ack_number < upcoming_sequence_num) {
                acknowleged_array[send_ack.ack_number] = 1;
                for(;counter_chunks < final_chunks_in_total && acknowleged_array[counter_chunks];counter_chunks++) {

                }
            }
        }
        gettimeofday(&present_time,NULL);
        int diff_time_sec = present_time.tv_sec - retrans_time.tv_sec;
        int diff_time_usec = present_time.tv_usec - retrans_time.tv_usec;

        if((diff_time_sec*1000000) + (diff_time_usec) > 100000)  {
            printf("Timeout occurred.Retransmitting the not acked chunks.\n");
            upcoming_sequence_num = counter_chunks;
        }
        usleep(1000);
    }
    printf("All chunks are sent and acknowledged.\n");
}

int receving_ack(struct sockaddr_in *recv_addr,int recv_fileD,struct ack *recv_ack) {
    socklen_t recv_len = sizeof(*recv_addr);
    int len_ack_read = recvfrom(recv_fileD,recv_ack,sizeof(*recv_ack),0,(struct sockaddr*)recv_addr,&recv_len);
    if(len_ack_read > 0) {
        printf("Recieved ack_num for chunk %d\n",recv_ack->ack_number);
        return 1;
    }
    else if(errno != EAGAIN && errno != EWOULDBLOCK) {
        perror("Error in recv");
    }
    return 0;
}

int main() {
    int fileD;
    struct sockaddr_in client_addr;

    fileD = socket(AF_INET,SOCK_DGRAM,0);
    if(fileD < 0) {
        perror("Error in socket");
        exit(0);
    }
    nonBlocking(fileD);

    client_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(PORT_USED);

    bool flag_send_receive = false;

    while(1) {
        if(flag_send_receive == false) {
            printf("-------SENDING-------\n");
            time_handling(0,fileD);
            char response[MAX_RESPONSE_SIZE];
            printf("Enter the data to send to receiver: ");
            fgets(response,MAX_RESPONSE_SIZE,stdin);
            response[strcspn(response,"\n")] = 0;

            if(strcmp(response,"exit") == 0) {
                break;
            }

            time_handling(1,fileD);
            handling_outgoing_chunks(&client_addr,fileD,response,strlen(response));
            flag_send_receive = true;
        }
        else if(flag_send_receive == true) {
            printf("------RECEIVING------\n");
            handling_incoming_chunks(fileD);
            flag_send_receive = false;
        }
    } 
    close(fileD);
    return 0;
}