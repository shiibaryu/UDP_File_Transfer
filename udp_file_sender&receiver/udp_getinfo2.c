#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/stat.h>
#include <fcntl.h>

int
main(int argc, char *argv[])
{
    int sock;
    struct addrinfo hints, *res;
    int n;
    int err;
    int fd;
    char *service = "12345";

    if(argc != 3){
        fprintf(stderr,"USAGE : %s dst\n",argv[0]);
        return 1;
    }

    fd = open(argv[2],O_RDONLY,0600);
    if(fd<0){
        perror("open");
        return 1;
    }

    memset(&hints,0,sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    err = getaddrinfo(argv[1],service,&hints,&res);
    if(err != 0){
        printf("getaddrinfo : %s\n",gai_strerror(err));
        return 1;
    }

    sock = socket(res->ai_family,res->ai_socktype,0);
    if(sock < 0){
        perror("socket");
        return 1;
    }

    {
        const char *ipverstr;
        switch(res->ai_family){
            case AF_INET:
                ipverstr = "IPv4";
                break;
            case AF_INET6:
                ipverstr = "IPv6";
                break;
            default:
                ipverstr = "unknown";
                break;
        }
        printf("%s\n",ipverstr);
    }

    n = sendto(sock,fd,sizeof(fd),0,res->ai_addr,res->ai_addrlen);
    if(n<1){
        perror("sendto");
        return 1;
    }

    close(sock);

    freeaddrinfo(res);

    return 0;
}

