#include "http.h"
#include "memory_pool.h"

struct io_uring *get_ring();
void init_io_uring();
void submit_and_wait();
void add_accept_request(int sockfd, http_request_t *request);
void add_read_request(http_request_t *request);
void add_write_request(int fd, void *usrbuf, size_t n, http_request_t *r);
void add_provide_buf(int bid);
char *get_bufs(int bid);
