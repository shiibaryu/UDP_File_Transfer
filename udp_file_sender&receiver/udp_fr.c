#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>



int
main(int argc,char *argv[])
{
    struct sockaddr_in addr;
    struct sockaddr_in senderinfo;
    int sock;
    int fd;
    char buf[2048];
    int n;
    socklen_t addrlen;

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
 
    addr.sin_family = AF_INET;
    addr.sin_port = htons(12345);
    addr.sin_addr.s_addr = INADDR_ANY;

    /*memset(&addr,0,sizeof(addr));*/
    

    bind(sock,(struct sockaddr*)&addr,sizeof(addr));

    addrlen = sizeof(senderinfo);
    /*int recvfrom(int socket, void *__restrict__ buffer, size_t length, int flags,struct sockaddr *__restrict__ address, socklen_t *__restrict__ address_length);*/
    n = recvfrom(sock,buf,sizeof(buf)-1,0,(struct sockaddr*)&senderinfo,&addrlen);
    
    write(fd,buf,n);
    printf("Got data from %u.\n",inet_ntoh(senderinfo.sin_addr.s_addr));

    close(sock);

    return 0;

}