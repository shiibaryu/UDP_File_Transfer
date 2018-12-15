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

#define DROP 0

typedef enum{SYN_SENT = 0,OPENING,OPEN,FIN_SENT}rudp_state_t;

struct data {
    void *item;
    int len;
    struct data *next;
}

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

struct timeoutargs{
    rudp_socket_t fd;
    struct rudp_packet *packet;
    struct sockaddr_in *recipient;
}

bool_t rng_seeded = false;
struct rudp_socket_list *socket_list_head = NULL;


void create_sender_session(struct rudp_socket_list *socket, u_int32_t seqno, struct sockaddr_in *to, struct data **data_queue) {
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

rudp_socket_t rudp_socket(int port){
    if(rng_seeded == false){
        srand(time(NULL));
        rng_seeded = true;
    }
    int sockfd;
    struct sockaddr_in address;

    socketfd = socket(AF_INET,SOCK_DGRAM,0);
    if(sockfd < 0){
        perror("socket");
        return(rudp_socket_t)NULL;
    }

    memset(&address,0,sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(sockfd,(struct *)&address,sizeof(address))<0){
        perror("bind");
        return NULL;
    }

    rudp_socket_t socket = (rudp_socket_t)socketfd;

    struct rudp_socket_list *newsocket = malloc(sizeof(rudp_socket_list));
    if(new_socket == NULL){
        fprintf(stderr,"rudp_socket: Error allocating memory for socket list");
        return (rudp_socket_t)-1;
    }

    newsocket->rsock = socket;
    newsocket->close_requested = false;
    newsocket->sessions_list_head = NULL;
    newsocket->next = NULL;
    newsocket->handler = NULL;
    newsocket->recv_handler = NULL;

    if(socket_list_head == NULL){
        socket_list_head = new_socket;
    }
    else{
        struct rudp_socket_list *curr = socket_list_head;
        while(curr->next != NULL){
            curr = curr->next;
        }
        curr->next = new_socket;
    }
    if(event_fd(socketfd,receive_callback,(void*)socketfd,"rudp_callback")<0){
        fprintf(stderr,"Error registering receive callback function");
    }

    return socket;
}

int rudp_close(rudp_socket_t rsocket){
    struct rudp_socket_list *curr_socket = socket_list_head;
    while(curr_socket->next != NULL){
        if(curr_socket->rsock == rsocket){
            break;
        }
        curr_socket = curr_socket->next;
    }
    if(curr_socket->rsock == rsocket){
        curr_socket->close_requested = true;
        return 0;
    }
    return -1;
}

int send_packet(bool_t is_ack, rudp_socket_t rsocket, struct rudp_packet *p, struct sockaddr_in *recipient) {
    char type[5];
    short t = p -> header.type;
    if(t == 1){
        strcpy(type,"DATA");
    }
    else if(t == 2){
        strcpy(type,"ACK");
    }
    else if(t == 3){
        strcpy(type,"SYN");
    }
    else if(t == 4){
        strcpy(type,"FIN");
    }
    else{
        strcpy(type,"BAD");
    }

    printf("Sending %s packet to %s:%d seq number = %u on socket = %d\n",type, inet_ntoa(recipient->sin_addr),ntohs(recipient->sin_port), p->header.seqno,(int)rsocket);
    
    /*if(DROP != 0 && rand()%DROP == 1){
        printf("Dropped\n");
    }*/
    if (sendto((int)rsocket, p, sizeof(struct rudp_packet), 0, (struct sockaddr*)recipient, sizeof(struct sockaddr_in)) < 0) {
        fprintf(stderr,"rudp_sendto:sendto failed\n");
        return -1;            
        }
    }

    if(!isack){
    struct timeoutargs *timeargs = malloc(sizeof(struct timeoutargs));
    if(timeags == NULL){
        fprintf(stderr,"send_packet:Error allocating timeout args\n");
        return -1;
    }
    timeargs->packet == malloc(sizeof(rudp_packet));
    if(timeargs->packet == NULL){
        fprintf(stderr, "send_packet: Error allocating timeout args packet\n");
        return -1;
    }
    timeargs->fd = rsocket;
    memcpy(timeargs->packet,p.sizeof(struct rudp_packet));
    memcpy(timeargs->recipient,recipient,sizeof(struct sockaddr));

    struct timeval currentTime;
    gettimeofday(&currentTime,NULL);
    struct timeval delay;
    delay.tv_sec = RUDP_TIMEOUT/1000;
    delay.tv_sec = 0;
    struct timeval timeout_time;
    timeradd(&currentTime,&delay,&timeout_time);

    struct rudp_socket_list *curr_socket = socket_list_head;
    while(curr_socket != NULL){
        if(curr_scoket->rsock == timeargs->fd){
            break;
        }
        curr_socket = curr_socket->next;
    }
    if(curr_socket->rsock == timeargs->fd){
      bool_t session_found = false;
      struct session *curr_session = curr_socket->session_list_head;
      while(curr_session != NULL){
          if(compare_sockaddr(&curr_session->address,timeargs->recipient)==1){
              session_found = true;
              break;
          }
          curr_session = curr_session->next;
      }
      if(session_found){
          if(timeargs->packet->header.type == RUDP_SYN){
              curr_session->sender->syn_timeout_arg = timeargs;
          }
          else if(timeargs->packet->header.type == RUDP_FIN){
              curr_session->sender->sym_timeout_arg = timeargs;
          }
          else if(timeargs->packet->header.type == RUDP_DATA){
              int i;
              int index;
              for(i = 0;i<RUDP_WINDOW;i++){
                  if(curr_session->sender->sliding_window[i] != NULL && curr_session->sender->sliding_window[i]->header.seqno == timeargs->packet->header.seqno){
                      index = i;
                  }
              }
              curr_session->sender->data_timeout_arg[index] = timeargs;
        }
      }
    }
    event_timeout(timeout_time, timeout_callback, timeargs, "timeout_callback");
   }
   return 0;
}

int rudo_sendto(rudp_socket_t rsock,void *data,int len,struct sockaddr_in *to){
    if(len < 0 || len > RUDP_MAXPKTSIZE){
        fprintf(stderr,"rudp_sendto Error: attempting to send with invalid packet size\n");
        return -1;
    }
    if(rsock < 0){
        fprintf(stderr,"rudp_sendto Error: attempting to send with rudp_socket"):
        return -1;
    }
    if(to == NULL){
        fprintf(stderr,"rudp_sendto Error: attempting to send an invalid address\n");
        return -1;
    }

    bool_t new_session_created = true;
    u_int32_t seqno = 0;
    if(socket_list_head == NULL){
        fprintf(stderr,"Error: attempt to send on invalid socket.No socket in the list.");
        return -1;
        }
    else{
        struct rudp_socket_list *curr = session_list_head;
        while(curr != NULL){
            if(curr->rsock == rsock){
                break;
            }
            curr = curr->next;
        }
        if(curr->rsock == rsock){
            struct data *data_item = malloc(sizeof(struct data));
            if(data_item == NULL){
                fprintf(stderr,"rudp_sendto: Error allocating data queue\n");
                return -1;
            }
            data_item->item = malloc(len);
            if(data_item->item == NULL){
                fprintf(stderr,"rudp_sendto: Error allocating data queue item");
                return -1;
            }
            memcpy(data_item->item,data,len);
            data_item->len = len;
            data_item->next = NULL;

            if(curr -> session_list_head == NULL){
                seqno = rand();
                /*void create_sender_session(struct rudp_socket_list *socket, u_int32_t seqno, struct sockaddr_in *to, struct data **data_queue) {*/
                create_sender_sesssion(curr,seqno,to,&data_item);
            }
            
        }

    }
    
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
        fprintf(stderr,"Error: attempt to receive on invalid socket. No sockets in the list.\n");
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
            else{
                /*他のセッションあった場合*/
                bool_t session_found = false;
                struct session *curr_session = curr_socket -> session_list_head;
                struct session *last_session;
                while(curr_session != NULL){
                    if(curr_session->next == NULL){
                        last_session = curr_session;
                    }
                    if(compare_sockaddress(&curr_session->address,&sender) == 1){
                        session_found = true;
                        break;
                    }
                    curr_session = curr_session->next;
                }
                if(session_found == false){
                    if(rudpheader.type == RUDP_ACK){
                        u_int32_t seqno = rudptype.seqno + 1;
                        create_receiver_session(curr_socket,seqno,&sender);
                        struct rudp_packet *p  = create_rudp_packet(RUDP_ACK,seqno,0,NULL);
                        send_packet(true,(rudp_sock_t) file,p,&sender);
                        free(p);
                    }
                    else{
                    }
                }
                else{
                    if(rudpheader.type = RUDP_SYN){
                        if(curr_session->receiver == NULL || curr_session ->receiver->status = OPENING ){}
                    }
                }
            }
        }
    }
}


