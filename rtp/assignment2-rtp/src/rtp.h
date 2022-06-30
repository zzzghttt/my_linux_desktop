#ifndef RTP_H
#define RTP_H

#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>

#define RTP_START 0
#define RTP_END   1
#define RTP_DATA  2
#define RTP_ACK   3

#define BUFFER_SIZE 2048
#define FILE_SIZE ~(1<<31)
#define DATASIZE 1461
#define FRAMESIZE 1472


typedef struct __attribute__ ((__packed__)) RTP_header {
    uint8_t type;       // 0: START; 1: END; 2: DATA; 3: ACK
    uint16_t length;    // Length of data; 0 for ACK, START and END packets
    uint32_t seq_num;
    uint32_t checksum;  // 32-bit CRC
} rtp_header_t;


typedef struct RTP_control_block {
    uint32_t            window_size;
    // TODO: you can add your RTP-related fields here
    int                 sockfd;
    struct sockaddr*    receiver_addr;
    socklen_t           receiver_len;
    uint32_t            seq_num;
} rcb_t;

// window state
struct window_state{
    int sz; //window size
    int left; // window left border
    int right;  // right border
    uint32_t Sf; //the first ack/pkg num should receive
    uint32_t Sn; //the next pkg num will send/receive
};

struct window_state *window;    //sender window
struct window_state *r_window;  //receiver window
char** cache;  //sender cache
char** r_cache;//receiver cache

static struct timeval re_timer = {0, 500};
static fd_set rset1;
static fd_set rset2;
static fd_set rset3;
static fd_set rset4;

static rcb_t* rcb = NULL;

// different from the POSIX
int rtp_socket(uint32_t window_size);

int rtp_listen(int sockfd, int backlog, struct sockaddr *addr, socklen_t *addrlen);

int rtp_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

int rtp_bind(int sockfd, struct sockaddr *addr, socklen_t addrlen);

int rtp_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

int rtp_close(int sockfd);

int rtp_sendto(int sockfd, const void *msg, int len, int flags, const struct sockaddr *to, socklen_t tolen);

int rtp_recvfrom(int sockfd, void *buf, int len, int flags, struct sockaddr *from, socklen_t *fromlen);

int send_ack(int sockfd, uint32_t seqnum, const struct sockaddr *to, socklen_t tolen);

int send_end(int sockfd, uint32_t seqnum, const struct sockaddr *to, socklen_t tolen);

void cache_add(char** cache, char *frame);

void cache_de(char** cache, uint32_t i);

char * cache_get(char** cache, uint32_t seqnum);

int check_cksum(rtp_header_t *rtp, char* buffer, int recv_bytes);
#endif //RTP_H
