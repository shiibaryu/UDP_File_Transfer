#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#include "event.h"
#include "rudp.h"
#include "rudp_api.h"

/* Probability of packet loss */
#define DROP 0

/* RUDP Stated */
typedef enum {
    SYN_SENT = 0,
    OPENING,
    OPEN,
    FIN_SENT
} rudp_state_t;

typedef enum {
    false = 0,
    true
} bool_t;

struct data {
    void *item;
    int len;
    struct data *next;
};

struct sender_session {
  rudp_state_t status; /* Protocol state */
  u_int32_t seqno;
  struct rudp_packet *sliding_window[RUDP_WINDOW];
  int retransmission_attempts[RUDP_WINDOW];
  struct data *data_queue; /* Queue of unsent data */
  bool_t session_finished; /* Has the FIN we sent been ACKed? */
  void *syn_timeout_arg; /* Argument pointer used to delete SYN timeout event */
  void *fin_timeout_arg; /* Argument pointer used to delete FIN timeout event */
  void *data_timeout_arg[RUDP_WINDOW]; /* Argument pointers used to delete DATA timeout events */
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

/* Keeps state for potentially multiple active sockets */
struct rudp_socket_list {
  rudp_socket_t rsock;
  bool_t close_requested;
  int (*recv_handler)(rudp_socket_t, struct sockaddr_in *, char *, int);
  int (*handler)(rudp_socket_t, rudp_event_t, struct sockaddr_in *);
  struct session *sessions_list_head;
  struct rudp_socket_list *next;
};

/* Arguments for timeout callback function */
struct timeoutargs{
    rudp_socket_t fd;
    struct rudp_packet *packet;
    struct sockaddr_in *recipent;
};

/*Prototypes*/
void create_sender_session(struct rudp_sock_list *socket,u_int32_t seqno,struct sockaddr_in *to,struct data **data_queue);
void create_receiver_session(struct rudp_socket_list *socket, u_int32_t seqno, struct sockaddr_in *addr);
struct rudp_packet *create_rudp_packet(u_int16_t type, u_int32_t seqno, int len, char *payload);
int compare_sockaddr(struct sockaddr_in *s1,struct sockaddr_in *s2);
int timeout_callback(int retry_attempts,void *args);
int send_packet(bool_t is_ack, rudp_socket_t rsocket, struct rudp_packet *p, struct sockaddr_in *recipient);

/*Global variabes*/
bool_t rng_seeded = false;
struct rudp_socket_list *socket_list_head = NULL;

/* Creates a new sender session and appends it to the socket's session list */
void create_sender_session(struct rudp_socket_list *socket, u_int32_t seqno, struct sockaddr_in *to, struct data **data_queue) {
    struct session *new_session = malloc(sizeof(struct session));
    if(new_session == NULL){
        fprintf(stderr,"create_sender_session: Error allocating memory\n");
        return ;
    }
    new_session -> address = *to;
    new_session -> next = NULL;
    new_session -> receiver = NULL;

    struct sender_session *new_sender_session = malloc(sizeof(struct sender_session));
    if(new_sender_session == NULL){
        fprintf(stderr,"create_sender_session: Error allocating memory\n");
        return;
    }
    new_sender_session -> status = SYN_SENT;
    new_sender_session -> seqno = seqno;
    new_sender_session -> session_finished = false;
    new_sender_session -> data_queue = *data_queue;
    new_session -> sender = new_sender_session;

    int i;
    for(i=0;i<RUDP_WINDOW;i++){
        new_sender_session -> retransmission_attempts[i] = 0;
        new_sender_session -> data_timeout_arg[i] = 0;
        new_sender_session -> sliding_window[i] = NULL;
    } 
    new_sender_session -> syn_retransmit_attempts = 0;
    new_sender_session -> fin_retransmit_attempts = 0;

    if(socket->sessions_list_head == NULL){
        socket->sessions_list_head = new_session;
    }
    else{
        struct session *curr_session = socket -> sessions_list_head;
        while(curr_session -> next != NULL){
            curr_session = curr_session ->next;
        }
        curr_session -> next = new_session;
    }      
}

void create_receiver_session(struct rudp_socket_list *socket,u_in32_t seqno,sturct sockaddr_in *addr){
    struct session *new_session = malloc(sizeof(struct session));
    if(new_session==NULL){
        fprintf(stderr,"create_receiver_session: Error allocating memory\n");
        return;
    }

    new_session -> address = *addr;
    new_session -> next = NULL;
    new_session -> sender = NULL;

    struct receiver_session *new_receiver_session = malloc(sizeof(receiver_session));
    if(new_receiver_session == NULL){
        fprintf(stderr,"create_receiver_session: Error allocating memory\n");
        return;
    }

    new_receiver_session -> status = OPENING;
    new_receiver_session -> expected_seqno = seqno;
    new_receiver_session -> session_finished = false;
    new_session -> receiver = new_receiver_session;

    if(socket -> sessions_list_head == NULL){
        socket -> sessions_list_head == new_session;
    }
    else{
        struct session *curr_session = socket -> sessions_list_head;
        while(curr_session->next != NULL){
            curr_session = curr_session -> next;
        }
        curr_session->next = new_session;
    }
}
/* Allocates a RUDP packet and returns a pointer to it */
struct rudp_packet *create_rudp_packet(u_int16_t type, u_int32_t seqno, int len, char *payload) {
  struct rudp_hdr header;
  header.version = RUDP_VERSION;
  header.type = type;
  header.seqno = seqno;
  
  struct rudp_packet *packet = malloc(sizeof(struct rudp_packet));
  if(packet == NULL) {
    fprintf(stderr, "create_rudp_packet: Error allocating memory for packet\n");
    return NULL;
  }
  packet->header = header;
  packet->payload_length = len;
  memset(&packet->payload, 0, RUDP_MAXPKTSIZE);
  if(payload != NULL) {
    memcpy(&packet->payload, payload, len);
    }
  
  return packet;
}

/* Returns 1 if the two sockaddr_in structs are equal and 0 if not */
int compare_sockaddr(struct sockaddr_in *s1,struct sockaddr_in *s2){
    char sender[16];
    char recipent[16];

    strcpy(sender,inet_ntoa(s1->sin_addr));
    strcpy(recipent,inet_ntoa(s2->sin_addr));

    return((s1->sin_family == s2 -> sin_family)&&(strcmp(sender,recipient)==0)&&(s1->sin_port == s2->sin_port));
}

/*Create and returns a RUDP socket*/
rudp_socket_t rudp_socket(int port){
    if(rng_seeded == false){
        /*乱数の元にtimeを使って、起動した時間で違った乱数を*/
        srand(time(NULL));
        rng_seeded = true;
    }
    int sockfd;
    struct sockaddr_in address;

    sockfd = socket(AF_INET,SOCK_DGRAM,0);
    if(sockfd < 0){
        perror("socket");
        return (rudp_socket_t)NULL;
    }

    memset(&address,0,sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    if(bind(sockfd,(struct sockaddr *)&address,sizeof(address))<0){
        perror("bind");
        return NULL;
    }

    rudp_socket_t socket = (rudp_socket_t)sockfd;

    /* Create new socket and add to list of sockets */
    struct rudp_socket_list *new_socket = malloc(sizeof(struct rudp_socket_list));
    if(new_socket == NULL){
        fprintf(stderr,"rudp_socket: Error allocating memory for socket list\n");
        return (rudp_socke_t) -1;
    }

    new_socket->rsock = socket;
    new_socket->close_requested = false;
    new_socket->sessions_list_head = NULL;
    new_socket->next = NULL;
    new_socket->handler = NULL;
    new_socket->recv_handler = NULL;

    if(socket_list_head = NULL){
        socket_list_head = new_socket;
    }
    else{
        struct rudp_socket_list *curr = socket_list_head;
        while(curr->next != NULL){
            curr = curr->next;
        }
        curr->next = new_socket;
    }
    /* Register callback event for this socket descriptor */
    if(event_fd(sockfd,receive_callback,(void*)sockfd,"receive_callback")<0){
        fprintf(stderr, "Error registering receive callback function");
    }

    return socket;
}

/* Callback function executed when something is received on fd */
int receive_callback(int socket,void *arg){
    char buf[sizeof(struct rudp_packet)];
    struct sockaddr_in sender;
    size_t sender_length = sizeof(struct sockaddr_in);
    /*int recvfrom(int socket,&buf,sizeof(bufferの長さ)),flags,sender_address,&sender_legth*/
    recvfrom(socket,&buf,sizeof(struct rudp_packet),0,(struct sockaddr *)&sender,&sender_length);

    struct rudp_packet *received_packet = malloc(sizeof(struct rudp_packet));
    if(received_packet == NULL){
        fprintf(stderr,"received_callback: Error allocating packet\n");
        return -1;
    }
    memcpy(received_packet,&buf,sizeof(struct rudp_packet));

    struct rudp_hdr rudpheader = received_packet -> header;
    char type[5];
    short t = rudpheader.type;
    if(t == 1){
        strcpy(type,"DATA");
    }
    else if(t == 2 ){
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

    printf("Received %s packet from %s:%d seq number=%u on socket=%d\n",type, 
       inet_ntoa(sender.sin_addr), ntohs(sender.sin_port),rudpheader.seqno,socket);

    if(socket_list_head == NULL){
        fprintf(stderr,"Error: attempt to receive on invalid socket. No sockets in the list\n");
        return -1;
    }
    else{
     /* We have sockets to check */
    struct rudp_socket_list *curr_socket = socket_list_head;
    while(curr_socket != NULL){
        if((int)curr_socket->rsock == socket){
            break;
        }
        curr_socket = curr_socket ->next;
      }
    if((int)curr_socket->rsock == socket){
    /* We found the correct socket, now see if a session already exists for this peer */
        if((int)curr_socket->sessions_list_head == NULL){
            /* The list is empty, so we check if the sender has initiated the protocol properly (by sending a SYN) */
            if(rudpheader.type == RUDP_SYN){
                /*SYNが届いているので新しいセッションを作る*/
                u_int32_t seqno = rudp_header.seqno + 1;
                create_receiver_session(curr_socket,seqno,&sender);

                /*ACKを返す*/
                struct rudp_packet *p = create_rudp_packet(RUDP_ACK,seqno,0,NULL);
                send_packet(true,(rudp_socket_t)socket,p,&sender);
                free(p);
            }
            else{

            }
        }
        else{
            /*some session exit to be checked*/
            bool_t session_found = false;
            struct session *curr_session = curr_socket -> session_list_head;
            struct sesion *last_session;
            while(curr_session != NULL){
                if(curr_session->next==NULL){
                    last_session = curr_session;
                }
                if(compare_sockaddr(&sender,&curr_session -> adress)==1){
                    session_found = true;
                    break;
                }

                curr_session = curr_session -> snext;
            }
        }
    }
  }
}