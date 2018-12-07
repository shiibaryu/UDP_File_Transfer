#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

typedef enum{SYN_SENT = 0,OPENING,OPEN,FIN_SENT}rudp_state_t;

struct rudp_hdr{
    u_int16_t version;
    u_int16_t type;
    u_int32_t seqno;
}

struct rudp_packet{
    struct rudp_hdr header;
    char payload[RUDP_MAXPKTSIZE];
    int payload_length;
}

struct sender_session{

}

struct receiver_session{
    rudp_state_t status;
    u_int32_t expected_seqno;
    bool_t session_finished;
}

struct session{
    struct sender_session *sender;
    struct receiver_session *receiver;
    struct sockaddr_in address;
    struct session *next; 
}

struct rudp_socket_list{
    rudp_socket_t rsock;
    bool_t close_requested;
    int (*recv_handler)(rudp_socket_t,struct sockadddr_in *, char *,int);
    int (*handler)(rudp_socket_t,rudp_event_t,struct sockaddr_in *);
    struct session *sessions_list_head;
    struct rudp_socket_list *next;
}

struct rudp_socket_list *socket_list_head = NULL;


void create_receiver_session(struct sockaddr_in *addr,u_int32_t seqno,struct rudp_socket_list *socket){
    struct session *new_session = malloc(sizeof(struct session));
    if(new_session = NULL){
        fprintf(stderr,"create_receive_session: Error allocating memory\n");
        return;
    }
    new_session->address = *addr;
    new_session->sender = NULL;
    new_session->next = NULL;

    struct receiver_session *new_receiver_session = malloc(sizeof(receiver_session));
    if(new_receiver_session == NULL){
        fprintf(stderr,"create_receiver_session: Error allocating memory\n");
        return;
    }
    new_receiver_session->status = OPENING;
    new_receiver_session->expected_seqno = seqno;
    new_receiver_session->session_finished = false;
    new_session -> receiver = new_receiver_session;

    if(socket->sessions_list_head == NULL){
       socket->sessions_list_head == new_session;
    }
    else{
        struct session *curr_session = socket->sessions_list_head;
        while(curr_session->next != NULL){
            curr_session = curr_session -> next;
        }
        curr_session -> next = new_session;
    }    
}

struct rudp_packet *create_rudp_packet(u_int16_t type,u_int32_t seqno,int len,char *payload){
    struct rudp_hdr header;
    header.version = RUDP_VERSION;
    header.type = type;
    header.seqno = seqno;

    struct rudp_packet *packet = malloc(sizeof(struct rudp_packet));
    if(packet == NULL){
        fprintf(stderr,"created_rudp_packet: Error allocating memory for packet\n");
        return NULL;
    }
    packet -> header = header;
    packet -> payload_length = len;
    memset(&packet->payload,0,RUDP_MAXPKTSIZE);
    if(payload != NULL){
        memcpy(&packet->payload,payload,len);
    }
    return packet;
}

int receive_callback(int file,void *arg){
    char buf[sizeof(struct rudp_packet)];
    struct sockaddr_in sender;
    size_t sender_length = sizeof(struct sockaddr_in);
    
    recvfrom(file,&buf,sizeof(struct rudp_packet),0,(struct sockaddr*)&sender,sender_length);

    struct rudp_packet *received_packet = malloc(sizeof(struct rudp_packet));
    if(received_packet == NULL){
        fprintf(stderr,"receive_callback: Error allocating packet");
        return -1;
    }
    memcpy(received_packet,&buf,sizeof(struct rudp_packet));

    struct rudp_hdr rudpheader = received_packet -> header;
    char type[5];
    short t = rudpheader.type;  
    if(t == 1){
        strcpy(type,"DATA");
    }
    else if(t == 2){
        strcpy(type,"ACK");
    }
    else if(t == 4){
        strcpy(type,"SYN");
    }
    else if(t == 5){
        strcpy(type,"FIN");
    }
    else{
        strcpy(type,"BAD");
    }

    printf("Received %s from %s:%d seq number = %u on socket = %d \n",type,
    inet_ntoa(sender.sin_addr),ntohs(sender.sin_port),rudpheader.seqno,file);

    if(socket_list_head == NULL){
        fprintf(stderr,"Error: attempt to receive on invalid socket.No sockets in the list.\n");
        return -1;
    }
    else{
        struct rudp_socket_list *curr_socket = socket_list_head;
        while(curr_socket != NULL){
            if((int)curr_socket->rsock == file){
                break;
            }
            curr_socket = curr_socket -> next;
        }
        if((int)curr_socket->rsock == file){
            if(curr_socket->sessions_list_head == NULL){
                if(rudpheader.type == RUDP_SYN){
                    u_int32_t seqno = rudpheader.seqno + 1;
                    create_receiver_session(&receiver,seqno,curr_socket);
                    /*respond with an ack*/
                    struct rudp_packet *p = create_rudp_packet(RUDP_ACK,seqno,0,NULL);
                    send_packet(true,(rudp_packet_t)file,p,&sender);
                    free(p);
                }
                else{
                 /* No sessions exist and we got a non-SYN, so ignore it */   
                }
            }
            

        }
    }
}
