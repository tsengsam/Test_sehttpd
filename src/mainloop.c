#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <liburing.h>

//#include "http.h"
//#include "logger.h"
//#include "timer.h"
#include "memory_pool.h"
#include "uring.h"

/* the length of the struct epoll_events array pointed to by *events */
#define MAXEVENTS 1024
#define LISTENQ 1024

#define accept 0
#define read 1
#define write 2
#define prov_buf 3
#define uring_timer 4

static int open_listenfd(int port)
{
    int listenfd, optval = 1;

    /* Create a socket descriptor */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    /* Eliminate "Address already in use" error from bind. */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *) &optval,
                   sizeof(int)) < 0)
        return -1;

    /* Listenfd will be an endpoint for all requests to given port. */
    struct sockaddr_in serveraddr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_ANY),
        .sin_port = htons((unsigned short) port),
        .sin_zero = {0},
    };
    if (bind(listenfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
        return -1;

    /* Make it a listening socket ready to accept connection requests */
    if (listen(listenfd, LISTENQ) < 0)
        return -1;

    return listenfd;
}

/* TODO: use command line options to specify */
#define PORT 8081
#define WEBROOT "./www"

int main()
{
    int listenfd = open_listenfd(PORT); 
    init_memorypool();
    init_io_uring();

    http_request_t *req = get_request();
    add_accept_request(listenfd, req);

    //struct io_uring *ring = get_ring();

    while(1)
    {
        printf("QQ\n");
        submit_and_wait();
        printf("hi\n");
        //struct io_uring_cqe *cqe ;
        //unsigned head;
        //unsigned count = 0;
        printf("QQ\n");
        break;
    }
    return 0;
}
