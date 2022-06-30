#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define RIP_DV      0
#define RIP_UPDATE  1
#define RIP_RESET   2
#define RIP_SHOW    3
#define RIP_RTR     4 // router to router msg

#define IP_LEN      16
#define PORT_LEN    8
#define FILESIZE    1024 
#define BUF_SIZE    512
#define MAXNUM      16
#define MAXSIZE     1024

#define MIN(a, b) a>b ? b:a

static int rt_num;
int neighbor[MAXNUM]; //hash table; >0: neighbor; <0: not neighbor

typedef struct __attribute__ ((__packed__)) RIP_header {
    int         type;       // 0: dv; 1: update; 2: reset; 3: show
    // uint16_t    length;    // Length of data; 0 for dv
    int         id;         //the sender id; 0 for agent
} rip_header_t;

struct DV{
    int id;
    int vector[MAXNUM];
};
struct DV dv[MAXNUM];

struct path{
    int dest;
    int next;
    int cost;
};

static struct timeval re_timer = {0, 500};
static fd_set rset;

struct router{
    int id;
    struct path *table; //each router's table
};
struct router rt_arry[MAXNUM]; //all router table (hash table)

struct loc{
    char  ip[IP_LEN];
    char  port[PORT_LEN];
    int    id; 
};
struct loc *location;   //router location

//rip.c
int loc_init(char* router_loc_file);
// struct path* table_init(char* loc, char* conf, int id, int rt_num);

// void update(int sockfd, struct path *table);
void send_msg(char* ip, char* port, char *msg);
struct loc* get_loc(struct loc *location, int id, int rt_num);
void table_print(struct path *table, int rt_num);