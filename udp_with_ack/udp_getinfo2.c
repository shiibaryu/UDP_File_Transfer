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

void 
print_my_port_num(int sock){
    struct sockaddr_in s;
    socklen_t sz = sizeof(s);
    getsockname(sock,(struct sockaddr *)&s,&sz);
    printf("%d\n",ntohs(s.sin_port));
}

int
main(int argc, char *argv[])
{
    int sock;
    struct sockaddr_in addr;
    struct sockaddr_in senderinfo;
    int n;
    int err;
    int fd;
    char buf[2048];
    int recv;
    socklen_t senderinfolen;

    if(argc != 3){
        fprintf(stderr,"USAGE : %s dst\n",argv[0]);
        return 1;
    }

    fd = open(argv[2],O_RDONLY,0600);
    if(fd<0){
        perror("open");
        return 1;
    }

    sock = socket(AF_INET,SOCK_DGRAM,0);
    if(sock < 0){
        perror("socket");
        return 1;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(12345);    
    inet_pton(AF_INET,argv[1],&addr.sin_addr.s_addr);

    n = sendto(sock,(char*)&fd,sizeof(fd),0,(struct sockaddr*)&addr,sizeof(addr));
    if(n<1){
        perror("sendto");
        return 1;
    }
    print_my_port_num(sock);
    
    memset(buf,0,sizeof(buf));
    senderinfolen = sizeof(senderinfo);
    recv = recvfrom(sock,buf,sizeof(buf),0,(struct sockaddr*)&senderinfo,&senderinfolen);
    printf("Got Ack from sender.\n");

    close(sock);


    return 0;
}

