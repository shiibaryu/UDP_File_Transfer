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
};

struct rudp_hdr{
    u_int16_t version;
    u_int16_t type;
    u_int32_t seqno;
};

struct rudp_packet{
    struct rudp_hdr header;
    char payload[RUDP_MAXPKTSIZE];
    int payload_length;
};

struct sender_session{
    rudp_state_t status;
    u_int32_t seqno;
    struct rudp_packet *sliding_window[RUDP_WINDOW];
    int retransmission_attempts[RUDP_WINDOW];
    struct data *data_queue;
    bool_t session_finished;
    void *syn_timeout_arg;
    void *fin_timeout_arg;
    void *data_timeout_arg[RUDP_WINDOW];
    int syn_retransmit_attempts;
    int fin_retransmit_attempts;
};

struct receiver_session{
    rudp_state_t status;
    u_int32_t expected_seqno;
    bool_t session_finished;
};

struct session{
    struct sender_session *sender;
    struct receiver_session *receiver;
    struct sockaddr_in address;
    struct session *next; 
};

struct rudp_socket_list{
    rudp_socket_t rsock;
    bool_t close_requested;
    int (*recv_handler)(rudp_socket_t,struct sockadddr_in *, char *,int);
    int (*handler)(rudp_socket_t,rudp_event_t,struct sockaddr_in *);
    struct session *sessions_list_head;
    struct rudp_socket_list *next;
};

struct timeoutargs{
    rudp_socket_t fd;
    struct rudp_packet *packet;
    struct sockaddr_in *recipient;
};



bool_t rng_seeded = false;
struct rudp_socket_list *socket_list_head = NULL;


void create_sender_session(struct rudp_socket_list *socket, u_int32_t seqno, struct sockaddr_in *to, struct data **data_queue) {
    struct session *new_session = malloc(sizeof(struct session));
    if(new_session == NULL){
        fprintf(stderr,"create_sender_session: Error allocating memory\n");
        return;
    }
    new_session->address = *to;
    new_session->next; = NULL;
    new_session->receiver = NULL;

    struct sender_session *new_sender_session = malloc(sizeof(struct sender_session));
    if(new_sender_session == NULL){
        fprintf(stderr,"create_sender_session: Error allocating memory\n");
        return;
    }
    new_sender_session->status = SYN_SENT;
    new_sender_session->seqno = seqno;
    new_sender_session->session_finished = false;
    new_sender_session->data_queue = *data_queue;

    int i;
    for(i = 0;i<RUDP_WINDOW;i++){
        new_sender_session->retransmission_attempts[i] = 0;
        new_sender_session->data_timeout_arg[i] =0;
        new_sender_session->sliding_window[i] = NULL;
    }
    new_sender_session->syn_retransmit_attempts = 0;
    new_sender_session->fin_retransmit_attempts = 0;

    if(socket->sessions_list_head == NULL){
        socket->sessions_list_head = new_session;
    }
    else{
        struct session *curr_sesssion = socket->sessions_list_head;
        while(curr_sesssion->next != NULL){
            curr_sesssion = curr_session->next;
        }
        socket->sessions_list_head = curr_sesssion;
    }
}

struct create_receiver_session(struct rudp_socket_list *curr_socket,u_int32_t seqno,struct sockaddr_in *to){
    struct session *new_session = malloc(sizeof(struct session));
    if(new_session = NULL){
        fprintf(stderr,"create_receiver_session: Error allocating memory\n");
        return;
    }
    new_session -> sender = NULL;
    new_session -> address = *to;
    new_session -> next = NULL;

    struct receiver_session *new_receiver_session = malloc(sizeof(receiver_session));
    if(new_receiver_session == NULL){
        fprintf(stderr,"create_receiver_session: Error allocating memory\n");
        return ;
    }
    new_receiver_session->status = OPENING;
    new_receiver_session->expected_seqno = seqno;
    new_receiver_session->session_finished = false;
    new_session->receiver = new_receiver_session;

    if(curr_socket->session_list_head == NULL){
        curr_socket->session_list_head = new_session;
    }
    else{
        struct session *curr_session = curr_socket->session_list_head;
        while(curr_session->next != NULL){
            curr_session = curr_session->next;
        }
        curr_session->next = new_session;
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

int compare_sockaddr(struct sockaddr_in *s1,struct sockaddr_in *s2){
    char sender[16];
    char recipient[16];
    strcpy(sender,inet_ntoa(s1->sin_addr));
    strcpy(recipient,inet_ntoa(s2->sin_addr));

    return ((s1->sin_addr == s2->sin_family)&&(s1->sin_port == s2->sin_family)&&(strcmp(sender,recipient)));
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
        struct rudp_socket_list *curr = socket_list_head;
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
            else{
                bool_t session_found = false;
                struct session *curr_session = curr -> session_list_head;
                struct session *last_in_list;
                while(curr_session != NULL){
                    if(compare_sockaddr(to,&curr_session->address)==1){
                        bool_t data_is_queued = false;
                        bool_t we_must_queue = true;
                        if(curr_session -> sender == NULL){
                            seqno = rand();
                            create_sender_session(curr,seqno,to,&data_item);
                            struct rudp_packet *p = create_rudp_packet(RUDP_SYN,seqno,0,NULL);
                            send_packet(false,rsock,p,to);
                            free(p);
                            new_session_created = false;
                            break;
                            }
                        if(curr_session->sender->data_queue != NULL){
                            data_is_queued = true;
                        }
                        if(curr_session->sender->status == OPEN && data_is_queued == true){
                            int i;
                            for(i=0;i<RUDP_WINDOW;i++){
                                if(curr_session->sender->sliding_window[i] == NULL){
                                    curr_session->sender->seqno = curr_session->sender->seqno + 1;
                                    struct rudp_packet *datap = create_rudp_packet(RUDP_DATA,curr_session->sender->seqno,len,data);
                                    curr_session->sender->sliding_window[i] = datap;
                                    curr_session->sender->retransmission_attempts[i] = 0;
                                    send_packet(false,rsock,datap,to);
                                    we_must_queue = false;
                                    break;
                                }
                        }
                    }
                    if(we_must_queue == true){
                        if(curr_session->sender->data_queue == NULL){
                            curr_session->sender->data_queue = data_item;
                        }
                        else{
                            struct data *curr_socket = curr_session->sender->data_queue;
                            while(curr_socket->next != NULL){
                                curr_socket = curr_socket -> next;
                            }
                            curr_socket->next = data_item;
                        }
                    }
                    session_found = true;
                    new_session_created = false;
                    break;
                    }
                    if(curr_session->next == NULL){
                        last_in_list = curr_session;
                    }
                    curr_session = curr_session -> next;
                }
                if(!session_found){
                    seqno = rand();
                    create_sender_session(curr,seqno,to,&data_item);
                }
            }
        }
        else{
            fprintf(stderr,"Error: attempt to send on invalid socket. Socket not found\n");
            return -1;
        }
    }
    if(new_session_created == true){
        struct rudp_packet *p = create_rudp_packet(RUDO_SYN,seqno,0,NULL);
        send_packet(false,rsocket,p,to);
        free(p);
        }
    return 0;
}

int receive_callback(int file,void *arg){
     char buf[sizeof(rudp_packet)];
     struct sockaddr_in sender;
     size_t sender_length = sizeof(struct rudp_packet);

     recvfrom(file,&buf,sizeof(struct rudp_packet),0,(struct sockaddr*)sender,sender_length);

     struct rudp_packet *received_packet = malloc(sizeof(struct rudp_packet));
     if(received_packet == NULL){
         fprintf(stderr,"received_packet: Error allocating packet\n");
         return -1;
     }
     memcpy(received_packet,&buf,sizeof(rudp_packet));

     struct rudp_hdr rudphdr = received_packet -> header;
     char type[5];
     short t = rudphdr.type;

     if(t == 1){
         strcpy(type,"DATA");
     };
     else if(t == 2){
         strcpy(type,"ACK");
     };
     else if(t == 3){
         strcpy(type,"SYN");
     }
     else if(t == 4){
         strcpy(type,"FIN");
     }
     else{
         strcpy(type,"Error");
     }

     printf("Received %s packet from %s: %d seq number = %u on socket = %d\n ",type,inet_ntoa(sender.sin_addr),ntohs(sender.sin_port),rudphdr.seqno,file);

     if(socket_list_head == NULL){
         fprintf(stderr,"Error: attempt to receive on invalid socket. No socket in the list\n");
         return -1;
     }
     else{
         while(curr_socket->next != NULL){
             if((int)curr_socket->rsock == file){
                 break;
             }
             curr_socket = curr_socket -> next;
         }
         /*sender側のsyn*/
         if((int)curr_socket->rsock == file){
            if(curr_socket->session_list_head == NULL){
                if(rudphdr.type == RUDP_SYN){
                    u_int32_t seqno = rudphdr.seqno + 1;
                    create_receiver_session(curr_socket,seqno,&sender);
                    struct rudp_packet *p = create_rudp_packet(RUDP_SYN,seqno,0,NULL);
                    send_packet(true,(rudp_socket_t)file,p,&sender);
                    free(p);
                }
                else{
                    printf("No session extis");
                }
            }
            else{
                boo_t session_found = false;
                struct session *curr_session = curr_socket -> session_list_head;
                struct session *last_session;
                while(curr_session != NULL){
                    if(curr_session->next == NULL){
                        last_session = curr_session;
                }
                if(compare_sockaddr(&curr_session->address,&sender) == 1){
                    session_found = true;
                    break;
                }
                curr_session = curr_session ->next;
                }
                /*receiver側がsynをもらった時のsynの返信*/
                if(session_found = false){
                    if(rudphdr.type == RUDP_SYN){
                        u_int32_t seqno = rudphdr.seqno + 1;
                        create_receiver_session(curr_socket,seqno,&sender);
                        struct rudp_packet *p = create_rudp_packet(RUDP_SYN,seqno,0,NULL);
                        send_packet(true,(rudp_state_t)file,p,&sender);
                        free(p);
                        }
                    else{
                        printf("No session exist");
                    }
                }
                else{
                    /*sender側がsynをもらった時のack*/
                    if(rudphdr.type == RUDP_SYN){
                        if(curr_socket->receiver == NULL || curr_session->receiver->status == OPENING){
                            struct receiver_session *new_receiver_session = malloc(sizeof(receiver_session));
                            if(new_receiver_session == NULL{
                                fprintf(stderr,"received_callbakc: Error allocating receiver session\n");
                                return -1;
                            }
                            new_receiver_session->expected_seqno = rudphdr.seqno + 1;
                            new_receiver_session->status = OPENING;
                            new_receiver_session->session_finished = false;
                            curr_session->receiver = new_receiver_session;

                            u_int32_t seqno = curr_session->receiver->expected_seqno;
                            struct rudp_packet *p = create_rudp_packet(RUDP_ACK,seqno,0,NULL);
                            send_packet(true,(rudp_socket_t)file,p,&sender);
                            free(p);
                        }
                        else{
                        }
                    }
                    /*sender側がsynを送って初めてackをもらった時のdata転送*/
                    if(rudphdr.type == RUDP_ACK){
                        u_int32_t ack_sqn = received_packet->header.seqno;
                        if(curr_session->sender->status == SYN_SENT){
                            u_int32_t syn_sqn = curr_session->sender->seqno;
                            if((ack_sqn-1)==syn_sqn){
                                event_timeout_delete(timeout_callback,curr_session->sender->syn_timeout_arg);
                                struct timeoutargs *t = (struct timeoutargs *)curr_session->sender->syn_timeout_arg;
                                free(t->packet);
                                free(t->recipient);
                                free(t);
                                curr_session->sender->status = OPEN;
                                while(curr_session->sender->data_queue != NULL){
                                    if(curr_session->sender->sliding_window[RUDP_WINDOW-1] != NULL){
                                        break;
                                    }
                                    else{
                                        int index;
                                        int i;
                                        for(i = RUDP_WINDOW-1 ;i>=0;i-){
                                            if(curr_session->sender->sliding_window[i] == NULL){
                                                index = i;
                                            }
                                        }
                                        u_int32_t seqno = ++syn_sqn;
                                        int len = curr_session->sender->data_queue->len;
                                        char *payload = curr_session->sender->data_queue->item;
                                        struct rudp_packet *datap = create_rudp_packet(RUDP_DATA,seqno,len,payload);
                                        curr_session->sender->seqno = +1;
                                        curr_session->sender->sliding_window[index] = datap;
                                        curr_session->sender->retransmission_attempts[index] = 0;
                                        struct data *temp = curr_session->sender->data_queue;
                                        curr_session->sender->data_queue = curr_session->sender->data_queue->next;
                                        free(temp->item);
                                        free(temp);

                                        send_packet(false,(rudp_socket_t)file,datap,&sender);
                                    }
                                }
                            }
                        }
                        /*sender側のDataに対するack*/
                        else if(curr_session->sender->status == OPEN){
                            if(curr_session->sender->sliding_window[0] != NULL){
                                if(curr_session->sender->sliding_window[0]->header.seqno == (rudphdr.seqno-1)){
                                    event_timeout_delete(timeout_callback,curr_session->sender->data_timeout_arg[0]);
                                    struct timeoutargs *args = (struct timeoutargs *)curr_session->sender->data_timeout_arg[0];
                                    free(args->packet);
                                    free(args->recipient);
                                    free(args);
                                    free(curr_session->sender->sliding_window[0]);

                                    if(RUDP_WINDOW == 1){
                                        curr_session->sender->sliding_window[0] = NULL;
                                        curr_session->sender->retransmission_attempts[0] = 0;
                                        curr_session->sender->data_timeout_arg[0] = NULL;
                                    }
                                    else{
                                        int i;
                                        for(i=0;i < RUDP_WINDOW -1;i++){
                                            curr_session->sender->sliding_window[i] = curr_session->sender->sliding_window[i+1];
                                            curr_session->sender->retransmission_attempts[i] = curr_session->sender->retransmission_attempts[i+1];
                                            curr_session->sender->data_timeout_arg[i] = curr_session->sender->data_timeout_arg[i+1];

                                            if(i == RUDP_WINDOW-2){
                                                curr_session->sender->sliding_window[i+1] = NULL;
                                                curr_session->sender->retransmission_attempts[i+1] = 0;
                                                curr_session->sender->data_timeout_arg[i+1]=NULL;
                                            }                                            
                                        }
                                    }

                                    while(curr_session->sender->data_queue != NULL){
                                        if(curr_session->sender->sliding_window[RUDP_WINDOW-1] != NULL){
                                            break;
                                        }
                                        else{
                                            int index;
                                            int i;
                                            for(i=RUDP_WINDOW-1;i>=0;i-){
                                                if(curr_session->sender->sliding_window[i]= NULL){
                                                    index = i;
                                                }
                                            }
                                            curr_session->sender->seqno = curr_session->sender->seqno + 1;
                                            u_int32_t seqno = curr_session->sender->seqno;
                                            int len = curr_session->sender->data_queue->len;
                                            char *payload = curr_session->sender->data_queue->item;
                                            struct rudp_packet *datap = create_rudp_packet(RUDP_DATA,seqno,len,payload);
                                            curr_session->sender->sliding_window[index] = datap;
                                            curr_session->sender->retransmission_attempts[index] = 0;
                                            struct data *temp = curr_session->sender->data_queue;
                                            curr_session->sender->data_queue = curr_session->sender->data_queue->next;
                                            free(temp->item);
                                            free(temp);
                                            send_packet(false,(rudp_socket_t)file,datap,&sender);
                                        }
                                    }
                                    if(curr_socket->close_requested){
                                        struct session *head_session = curr_socket->session_list_head;
                                        while(head_session != NULL){
                                            if(head_session->sender->session->session_finished == false){
                                                if(head_session->sender->data_queue == NULL && 
                                                   head_session->sender->sliding_window[0] == NULL &&
                                                   head_session->sender->status == OPEN){
                                                   header_session->sender->seqno = header_session->sender->seqno +1;
                                                   struct rudp_packet *p = create_rudp_packet(RUDP_FIN,head_session->sender->seqno,0,NULL);
                                                   send_packet(false,(rudp_socket_t)file,p,&head_session->address);
                                                   free(p);
                                                   head_session->sender->status = FIN_SENT;
                                                   }
                                                }
                                                head_session = head_session->next;
                                            }
                                        }
                                    }
                                }
                            }
                        else if(curr_session->sender->status = FIN_SENT){
                            if((curr_session->sender->seqno + 1)== received_packet->header.seqno){
                                event_timeout_delete(timeout_callback,curr_session->sender->fin_timeout_arg_arg);
                                struct timeoutargs *t = curr_session->sender->fin_timeout_arg;
                                free(t->packet);
                                free(t->recipient);
                                free(t);
                                curr_session->sender->session_finished = true;
                                if(curr_socket->close_requested){
                                    struct session *head_session = curr_socket->session_list_head;
                                    bool_t all_done = true;
                                    while(head_session != NULL){
                                        if(head_session->sender->session_finished == false){
                                            all_done = false;
                                        }
                                        else if(head_session->receiver != NULL && head_session->sender->session_finished == false){
                                            all_done = false;
                                        }
                                        else{
                                            free(head_session->sender);
                                            if(head_session->receiver){
                                                free(head_session->receiver);
                                            }
                                        }
                                        struct session *temp = head_session;
                                        head_session = head_session->next;
                                        free(temp);
                                    }
                                    if(all_done){
                                        if(curr_socket->header != NULL){
                                            curr_socket->handler((rudp_socket_t)file,RUDP_EVENT_ClOSED,&sender);
                                            event_fd_delete(receive_callback,(rudp_socket_t)file);
                                            close(file);
                                            free(curr_socket);
                                        }
                                    }
                                }
                            }
                            else{
                                
                            }
                        }
                    }
                    else if(rudphdr.type == RUDP_DATA){
                        if(curr_socket->receiver->status==OPENING){
                            if(rudphdr.seqno==curr_session->receiver->expected_seqno){
                                curr_session->receiver->status = OPEN;
                            }
                        }
                        if(rudphdr.seqno == curr_session->receiver->expected_seqno){
                            u_int32_t seqno = rudphdr.seqno + 1;
                            curr_sesion->receiver->expected_seqno = seqno;
                            struct rudp_packet *p = create_rudp_packet(RUDP_ACK,seqno,0,NULL);
                            send_packet(true,(rudp_socket_t)file,p,&sender);
                            free(p);

                            if(curr_socket->recv_handler != NULL){
                                curr_socket->recv_handler((rudp_socket_t)file,&sender,(void*)&received_packet->payload,received_packet->payload_length);
                            }
                        }
                        else if(SEQ_GEQ(rudphdr.seqno,(curr_session->receiver->expected_seqno-RUDP_WINDOW))&&
                                SEQ_LT(rudphdr.seqno,curr_session->receiver->expected_seqno)){
                                    u_int32_t seqno = rudphdr.seqno + 1;
                                    struct rudp_packet *p = create_rudp_packet(RUDP_ACK,seqno,0,NULL);
                                    send_packet(true,(rudp_socket_t)file,p,&sender);
                                    free(p);
                        }
                    }
                    else if(rudphdr.type == RUDP_FIN){
                        if(cuurr_session->receiver->statis == OPEN){
                            if(rudphdr.seqno == curr_session->receiver->expected_seqno){
                                u_int32_t seqno = curr_session->receiver->expected_seqno + 1;
                                struct rudp_packet *p = create_rudp_packet(RUDP_ACK,seqno,NULL,0);
                                send_packet(true,(rudp_socket_t)file,p,&sender);
                                free(p);
                                curr_session->receiver->session_finished = true;

                                if(curr_socket->close_requested){
                                    struct session *head_session = curr_socket->session_list_head;
                                    int all_done = true;
                                    while(head_session != NULL){
                                        if(head_session->receiver->session_finished == false){
                                            all_done = false;
                                        }
                                        else if(head_session->receiver != NULL && head_session->receiver->session_finished == false){
                                            all_done = false;
                                        }
                                        else{
                                            free(head_session->sender);
                                            if(head_session->receiver){
                                                free(head_session->receiver);
                                            }
                                        }
                                        if(all_done == true){
                                            if(curr_socket->handler != NULL){
                                                curr_socket->handler((rudp_socket_t),RUDP_EVENT_ClOSED,&sender);
                                                event_fd_delete(receive_callback,(rudp_socket_t)file);
                                                close(file);
                                                free(curr_socket);
                                            }
                                        }
                                    }
                                }
                            }
                            else{

                            }
                        }
                    }
                }
            }
        }
    }

    free(received_packet);
    return 0;
 }
