#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "rudp_send.h"
#include "rudp.h"
#include "rudp_api.h"


#define MAXPEERS 32
#define S_FILENAMELENGTH 128
#define S_MAXDATA  128


int usage();
void send_file();

struct sftp {
    u_int32_t s_type;
    union{
        char s_filename[S_FILENAMELENGTH];
        u_int8_t s_data[S_MAXDATA];
    }s_info;
};

int debug = 0;
struct sockaddr_in peers[MAXPEERS];
int npeers = 0;



int usage(){
    fprintf(stderr,"Usage: send [-d]\n");
    exit(1);
}

int main(int argc,char* argv[]){
    int c;
    int i;
    struct in_addr *addr;
    struct hostent* hp;
    char *hoststr;
    int port;
    
    while((c = getopt(argc,argv,"d"))!= -1){
        if(c == "d"){
            debug = 1;
        }
        else{
            usage();
        }   
    }
    for(i=optind; i < argc; i++ ){
        if(strchr(argv[i],":")==NULL){
            break;
        }
        hoststr = (char *)malloc(strlen(argv[i]+1));
        if(hoststr == NULL){
            fprintf(stderr, "memory allocation:malloc failed\n");
            exit(1);
        }
        strcpy(hoststr,argv[i]);
        port = atoi(strchr(hoststr,":") + 1);
        if(port <= 0){
            fprintf(stderr,"Bad Destination port:%d\n",
            atoi(strchr(hoststr,":") + 1));
            exit(1);
        }
        memset((char*)&peers[npeers],0,sizeof(struct sockaddr_in));
        peers[npeers].sin_family = AF_INET;
        peers[npeers].sin_port = htons(port);
        addr->s_addr = hoststr;
        memcpy(&peers[npeers].sin_addr,addr,sizeof(struct in_addr));
        npeers++;
        free(hoststr);
    }

    if(npeers == 0){
        usage();
    }
    if(optind >= argc){
        usage();
    }
    for(i=optind;i<argc;i++){
        send_file(argv[i++]);
    }
    /*eventloop(0);*/
    return 0;
}

void send_file(char *filename){
    int fd;
    struct sftp sft;
    int sft_len;
    rudp_socket_t rsock;
    int p;
    char *file;
    int filelen;
    int final_size;
    

    fd = open(filename,O_RDONLY);
    if(fd < 0){
        perror("rudp_sender: open");
        exit(-1);
    }
    rsock = rudp_socket(0);
    if(rsock == NULL){
        fprintf(stderr,"rudp_send: rudp_socket() failed\n");
        exit(1);
    }
    /*rudp_event_handler(rsock,eventhandler);*/

    sft.s_type = htonl(12345);
    file = filename;
    if(strrchr(file,"/") != NULL){
        file = strrchr(file,"/") + 1;
    }

    filelen = strlen(file);
    strncpy(sft.s_info.s_filename,file,filelen);
    /*  vslen = sizeof(vs.vs_type) + namelen;*/

    for(p=0;p<npeers;p++){
        if(debug){
        printf("rudp_send: send begin \"%s\" (%d bytes)to%s:%d\n",filename,filelen,inet_ntoa(peers[p].sin_addr),htons(peers[p].sin_port));
        }
        if(rudp_sendto(rsock,(char *)&sft,sizeof(sft),&peers[p])<0){
            fprintf(stderr,"rudp_sender:send failure\n");
            rudp_close(rsock);
            return;
    }
  }
  /*event_fd(file,filesender,rcosk,"filesender");*/
}





