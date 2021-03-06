#include <assert.h>
#include <liburing.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

#include "uring.h"

#define accept 0
#define read 1
#define write 2
#define prov_buf 3
#define uring_timer 4

#define Queue_Depth 256
#define MAX_CONNECTIONS 2048
#define MAX_MESSAGE_LEN 8192
char bufs[MAX_CONNECTIONS][MAX_MESSAGE_LEN] = {0};
int group_id = 8888;

struct io_uring ring;

static void msec_to_ts(struct __kernel_timespec *ts, unsigned int msec)
{
	ts->tv_sec = msec / 1000;
	ts->tv_nsec = (msec % 1000) * 1000000;
}

void init_io_uring()
{
    struct io_uring_params params;
    memset(&params, 0, sizeof(params));

    int ret = io_uring_queue_init_params(Queue_Depth, &ring, &params);
    assert(ret >= 0 && "io_uring_queue_init");
    
    if (!(params.features & IORING_FEAT_FAST_POLL)) {
        printf("IORING_FEAT_FAST_POLL not available in the kernel, quiting...\n");
        exit(0);
    }
    
    struct io_uring_probe *probe;
    probe = io_uring_get_probe_ring(&ring);
    if (!probe || !io_uring_opcode_supported(probe, IORING_OP_PROVIDE_BUFFERS)) {
        printf("Buffer select not supported, skipping...\n");
        exit(0);
    }
    free(probe);

    struct io_uring_sqe *sqe;
    struct io_uring_cqe *cqe;

    sqe = io_uring_get_sqe(&ring);
    io_uring_prep_provide_buffers(sqe, bufs, MAX_MESSAGE_LEN, MAX_CONNECTIONS, group_id, 0);

    io_uring_submit(&ring);
    io_uring_wait_cqe(&ring, &cqe);
    if (cqe->res < 0) {
        printf("cqe->res = %d\n", cqe->res);
        exit(1);
    }
    io_uring_cqe_seen(&ring, cqe);
}

struct io_uring *get_ring() {
    return &ring;
}

void submit_and_wait() {
    io_uring_submit_and_wait(&ring, 1);
}

void add_accept_request(int sockfd, http_request_t *request)
{
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring) ;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    io_uring_prep_accept(sqe, sockfd, (struct sockaddr*)&client_addr, &client_addr_len, 0);
    request->event_type = accept ;
    request->fd = sockfd ;
    io_uring_sqe_set_data(sqe, request);
}

void add_read_request(http_request_t *request)
{
    int clientfd = request->fd ;
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring) ;
    io_uring_prep_recv(sqe, clientfd, NULL, MAX_MESSAGE_LEN, 0);
    io_uring_sqe_set_flags(sqe, (IOSQE_BUFFER_SELECT | IOSQE_IO_LINK) );
    sqe->buf_group = group_id;

    request->event_type = read ;
    io_uring_sqe_set_data(sqe, request);

    struct __kernel_timespec ts;
    msec_to_ts(&ts, 500);
    sqe = io_uring_get_sqe(&ring);
    io_uring_prep_link_timeout(sqe, &ts, 0);
    http_request_t *timeout_req = get_request();
    assert(timeout_req && "malloc fault");
    timeout_req->event_type = uring_timer ;
    io_uring_sqe_set_data(sqe, timeout_req);
}

void add_write_request(int fd, void *usrbuf, size_t n, http_request_t *r)
{
    char *bufp = usrbuf;

    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring) ;
    http_request_t *request = r;
    request->event_type = write ;
    unsigned long len = strlen(bufp);

    io_uring_prep_send(sqe, fd, bufp, len, 0);
    io_uring_sqe_set_flags(sqe, IOSQE_IO_LINK);
    io_uring_sqe_set_data(sqe, request);

    struct __kernel_timespec ts;
    msec_to_ts(&ts, 1000);
    sqe = io_uring_get_sqe(&ring);
    io_uring_prep_link_timeout(sqe, &ts, 0);
    http_request_t *timeout_req = get_request();
    assert(timeout_req && "malloc fault");
    timeout_req->event_type = uring_timer ;
    io_uring_sqe_set_data(sqe, timeout_req);
}

void add_provide_buf(int bid) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    io_uring_prep_provide_buffers(sqe, bufs[bid], MAX_MESSAGE_LEN, 1, group_id, bid);
    http_request_t *req = get_request();
    assert(req && "malloc fault");
    req->event_type = prov_buf ;
    io_uring_sqe_set_data(sqe, req);
}

char *get_bufs(int bid) {
    return &bufs[bid];
}

