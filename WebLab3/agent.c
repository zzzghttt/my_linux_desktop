#include "common.h"

void op_dv(char *msg){
    int i;
    for(i=0; i < rt_num; i++){
        send_msg(location[i].ip, location[i].port, msg);
    }
}

void 
show(char* ip, char* port, char *msg)
{
    int sockfd;
    struct sockaddr_in servaddr;
    char buf[BUF_SIZE];
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(atoi(port));
    inet_pton(AF_INET, ip, &servaddr.sin_addr);
    if(connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0)
        return;
        // perror("connect error");

    write(sockfd, msg, BUF_SIZE);
again:
    FD_ZERO(&rset);
    FD_SET(sockfd, &rset);
    switch(select((sockfd)+1, &rset, NULL, NULL, &re_timer)){
        case -1:
            break;
        case 0:
            goto again;
        default:
        if(FD_ISSET(sockfd, &rset)){
            read(sockfd, buf, BUF_SIZE);
            struct path* ptr = (struct path*)buf;
            table_print(ptr, rt_num);
            close(sockfd);
        }
        break;
    }
}

void eval(char *cmdline)
{
    //parse the cmdline.
    static char buf[MAXSIZE];
    char *ptr = buf;
    strncpy(ptr, cmdline, MAXSIZE);

    char msg[BUF_SIZE];
    rip_header_t *rip = (rip_header_t*)msg;
    struct path *ptr2 = (struct path*)(msg + sizeof(rip_header_t));
    struct loc *loc1;
    int id1, id2, weight;

    if(!strcmp(ptr,"dv")){
        rip->type = RIP_DV;
        // rip->length = 0;
        rip->id = 0;
        op_dv(msg);
    }
    else{
        int n = strcspn(ptr,":");
        buf[n] = '\0';
        if(!strcmp(ptr, "update")){
            ptr += n + 1;
            rip->type = RIP_UPDATE;
            // rip->length = strlen(ptr);
            rip->id = 0;

            id1 = atoi(ptr);
            if(id1 == 0){
                printf("Usage: update:id1,id2,weight\n");
                return;
            }
            ptr = strstr(ptr, ",") + 1;
            id2 = atoi(ptr);
            ptr = strstr(ptr, ",") + 1;
            weight = atoi(ptr);
            loc1 = get_loc(location, id1, rt_num);
            ptr2->dest = id2;
            ptr2->next = id2;
            ptr2->cost = weight;
            send_msg(loc1->ip, loc1->port, msg);
        }
        else if(!strcmp(ptr, "show")){
            ptr += n + 1;
            rip->type = RIP_SHOW;
            // rip->length = strlen(ptr);
            rip->id = 0;
            id1 = atoi(ptr);
            if(id1 == 0){
                printf("Usage: show:id1\n");
                return;
            }
            loc1 = get_loc(location, id1, rt_num);
            show(loc1->ip, loc1->port, msg);
        }
        else if(!strcmp(ptr, "reset")){
            ptr += n + 1;
            rip->type = RIP_RESET;
            // rip->length = strlen(ptr);
            rip->id = 0;
            id1 = atoi(ptr);
            if(id1 == 0){
                printf("Usage: reset:id1\n");
                return;
            }
            loc1 = get_loc(location, id1, rt_num);
            send_msg(loc1->ip, loc1->port, msg);
        }
    }
}

int agent()
{
    char cmdline[MAXSIZE];    /* cmdline for fgets */
    while (1) {
        if ((fgets(cmdline, MAXSIZE, stdin) == NULL) && ferror(stdin))
            perror("fgets error");
        
        /* Remove the trailing newline */
        cmdline[strlen(cmdline)-1] = '\0';
        
        /* Evaluate the command line */
        eval(cmdline);
        
        fflush(stdout);
        fflush(stderr);
    } 
    return 0;
}
/*
 * main():
 * Parse command-line arguments and call agent function
*/
int main(int argc, char **argv) 
{
    setvbuf(stdout, NULL, _IONBF, 0);
    char *router_loc_file;
    int router_id;

    if (argc != 2) {
        fprintf(stderr, "Usage: ./agent <router location file>\n");
        exit(-1);
    }
    router_loc_file = argv[1];

    rt_num = loc_init(router_loc_file);
    return agent();
}