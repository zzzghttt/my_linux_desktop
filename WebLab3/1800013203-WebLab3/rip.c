#include <stdio.h>
#include <stdlib.h>
#include "common.h"

//init location, return router numember.
int 
loc_init(char* router_loc_file)
{
    FILE *fp;
    char buf[BUF_SIZE];
    char *ptr, *id;
    int rt_num;
    if((fp = fopen(router_loc_file, "r")) == NULL)
        perror("open error");

    if(fread(buf, sizeof(char), BUF_SIZE, fp) < 0)
        perror("read error");

    rt_num = atoi(buf);
    
    location = (struct loc*)malloc(sizeof(struct loc)*rt_num);

    ptr = buf + 2;
    for(int i=0; i<rt_num; i++){
        int n = strcspn(ptr, ",");
        memmove(location[i].ip, ptr, sizeof(char)*n);
        ptr += n + 1;

        n = strcspn(ptr, ",");
        memmove(location[i].port, ptr, sizeof(char)*n);
        ptr += n + 1;

        id = strtok(ptr, "\n");
        location[i].id = atoi(id);
        ptr += strlen(id) + 1;

        // printf("id=%d, ip=%s, port=%s, iplen=%ld\n", 
        //         location[i].id, location[i].ip, location[i].port, strlen(location[i].ip));
    }
    return rt_num;
}

//return a given id router location
struct loc* 
get_loc(struct loc *location, int id, int rt_num)
{
    int i;
    for(i=0; i < rt_num; i++){
        if(location[i].id == id)
            break;
    }
    return &location[i];
}


void 
send_msg(char* ip, char* port, char *msg)
{
    int sockfd;
    struct sockaddr_in servaddr;
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(atoi(port));
    inet_pton(AF_INET, ip, &servaddr.sin_addr);
    if(connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0)
        perror("connect error");

    write(sockfd, msg, BUF_SIZE);
    close(sockfd);
}


void 
table_print(struct path *table, int rt_num)
{   
    int i = 0;
    while(i < rt_num){
        if(table[i].cost >= 0)
            printf("dest: %d, next: %d, cost: %d\n", table[i].dest, table[i].next, table[i].cost);
        i++;
    }
}
