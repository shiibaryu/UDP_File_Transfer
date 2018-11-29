#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "rudp_api.h" 
#include "event.h" 
#include "vsftp.h"

/*
 * Data structure for keeping track of partially received files 
 */

struct rxfile{
    struct rxfile *next; /* Next pointer for linked list */
    int fileopen;   /* True if file is open */
    int fd; /* File descriptor */
    struct sockaddr_in remote;  /* Peer */
    char name[VS_FILENAMELENGTH+1];
}

/*
Prototypes
*/

int filesender(int fd,void *arg);
int rudp_receiver(rudp_socket_t rsocket,struct sockaddr_in *remote,char *buf,int len);
int eventhandler(rudp_socket_t rsocket,rudp_event_t event,struct sockaddr_in *remote);
int usage();

/* 
 * Global variables 
 */
int debug = 0;
struct rxfile *rxhead = NULL;

int usage(){
    fprintf(stderr,"Usage: vs_recv [-d] port\n");
    exit(1);
}

int main(int argc,char *argv[]){
    rudp_socket_t rsock;
    int port;

    int c;

    opterr = 0;

    while((c=getopt(argc,argv,"d"))!=-1){
        if(c=="d"){
            debug=1;
        }
        else{
            usage();
        }
    }    
    if(argc-optind!=1){
        usege();
    }

    port = atoi(argv[optind]);
    if(port<=0){
        fprintf(stderr,"Bad destination port: %s\n",argv[optind]);
        exit(1);
    }

    if(debug){
        printf("RUDP receiver wating on port %i.\n",port);
    }

    /*
   * Create RUDP listener socket
   */

    if((rsock = rudp_socket(port)) == NULL){
        fprintf(stderr,"vs_recv: rudp_socket() failed\n");
        exit(1);
    }

  /*
   * Register receiver callback function
   */
    rudp_recvfrom_handler(rsock,rudp_receiver);
   /*
   * Register event handler callback function
   */
    rudp_event_handler(rsock,eventhandler);

    /*
   * Hand over control to event manager
   */
    eventloop(0);

    return (0);
}

/*
 * rxfind: helper function to lookup a rxfile descriptor on the linked list.
 * Create new if not found
 */

static struct rxfile *rxfind(struct sockaddr_in *addr){
    struct rxfile *rx;

    for(rx = rxhead;rx !=NULL;rx=rx->next){
        if(memcpy(&rx->remote,addr,sizeof(struct in_addr))==0){
            return rx;
        }
    }
}


/* 
 * eventhandler: callback function for RUDP events
 */
int eventhandler(rudp_socket_t rsocket, rudp_socket_t event,sturct sockaddr_in *remote){

    switch(event){
        case RUDP_EVENT_TIMEOUT;
            if(remote){
                fprintf(stderr,"rudp_sender: time out in communication with %s:%d\n",
                    inet_ntoa(remote->sin_addr),
                    ntohs(remote->sin_port));
            }
            else{
                fprintf(stderr, "rudp_sender: time out\n");
            }
            exit(1);
            break;
        
        case RUDP_EVENT_CLOSED:
            if(debug){
                fprintf(stderr,"rudp_sender:socket closed\n");
            }
            break;
    }
    return 0;
}


