#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <string.h>


int main(int argc,char *argv[])
{
    struct sockaddr_in addr;
    struct sockaddr_in senderinfo;
    int sock;
    int fd;
    char buf[2048];
    int n;
    socklen_t addrlen;
    char senderstr[6];
    char *ack = "ack";



    if(argc!=2){
        fprintf(stderr,"Usage: %s outputfilename\n",argv[0]);
        return 1;
    }

    fd = open(argv[1],O_WRONLY, 0600);
    if(fd<0){
        perror("open");
        return 1;
    }

    sock = socket(AF_INET,SOCK_DGRAM,0);
    if(socket<0){
        perror("socket");
        return 1;
    }
 
    addr.sin_family = AF_INET;
    addr.sin_port = htons(12345);
    addr.sin_addr.s_addr = INADDR_ANY;

    /*memset(&addr,0,sizeof(addr));*/
    bind(sock,(struct sockaddr*)&addr,sizeof(addr));
    

    while(1){
        memset(buf,0,sizeof(buf));
        addrlen = sizeof(senderinfo);
        /*int recvfrom(int socket, void *__restrict__ buffer, size_t length, int flags,struct sockaddr *__restrict__ address, socklen_t *__restrict__ address_length);*/
        n = recvfrom(sock,buf,sizeof(buf),0,(struct sockaddr*)&senderinfo,&addrlen);
        write(fd,buf,n);
        inet_ntop(AF_INET,&senderinfo.sin_addr,senderstr,sizeof(senderinfo));

        printf("recvfrom : %s, port = %lu\n",senderstr,sizeof(senderstr));
        
        sendto(sock,ack,sizeof(ack),0,(struct sockaddr *)&senderinfo,addrlen);
        printf("Send Ack from here.\n");
    }

    close(sock);

    return 0;

}