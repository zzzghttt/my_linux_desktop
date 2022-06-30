#include "common.h"

static int router_id;
static struct path *rt_table;
pthread_mutex_t mutex;

void sort_table()
{
    char buf[BUF_SIZE];
    int tmp;
    int len = sizeof(struct path);
    for(int j=rt_num; j > 1; j--){
        tmp = rt_table[0].dest;
        for(int i=1; i<j; i++){
            if(tmp > rt_table[i].dest){
                memmove(buf, &rt_table[i], len);
                memmove(&rt_table[i], &rt_table[i-1], len);
                memmove(&rt_table[i-1], buf, len);
            }
            else{
                tmp = rt_table[i].dest;
            }
        }
    }
}

//init a given id routing rt_table and return it.
void
table_init(char* loc, char* conf, int id)   
{
    FILE *fp1, *fp;
    char buf1[BUF_SIZE], buf[BUF_SIZE];
    char *ptr = buf;

    if((fp = fopen(conf, "r")) == NULL)
        perror("open error");
    if(fread(buf, sizeof(char), BUF_SIZE, fp) < 0)
        perror("read error");
    
    rt_table = (struct path*)malloc(sizeof(struct path)*rt_num);
    memset(rt_table, 0, sizeof(struct path)*rt_num);
    memset(neighbor, -1, sizeof(int)*MAXNUM);

    rt_table[0].dest = id;
    rt_table[0].next = id;
    rt_table[0].cost = 0;

    int n = atoi(ptr);
    int i = 1;
    int j = 1;
    ptr = strstr(ptr, "\n") + 1;
    for(i; i <= n; i++){
        if(atoi(ptr) != id){
            ptr = strstr(ptr, "\n") + 1;
            continue;
        }
        ptr = strstr(ptr, ",") + 1;
        rt_table[j].dest = atoi(ptr);
        rt_table[j].next = atoi(ptr);
        ptr = strstr(ptr, ",") + 1;
        rt_table[j].cost = atoi(ptr);

        if(rt_table[j].cost > 0){
            neighbor[rt_table[j].next] = rt_table[j].cost;
        }
        j++;
        ptr = strstr(ptr, "\n") + 1;
    }
    if(j < rt_num){
        for(int k=0; k<rt_num; k++){
            int nid = location[k].id;
            if(nid != id && neighbor[nid] < 0){
                rt_table[j].dest = nid;
                rt_table[j].next = -1;
                rt_table[j].cost = -1;
                j++;
            }
        }
    }
    sort_table();
}

void send_nbor()
{
    char msg[BUF_SIZE];
    memset(msg, 0, BUF_SIZE);
    rip_header_t *rt = (rip_header_t*)msg;
    rt->type = RIP_RTR;
    rt->id = router_id;
    memmove(msg + sizeof(rip_header_t), rt_table, sizeof(struct path)*rt_num);

    for(int i=0; i<rt_num; i++){
        int nid = location[i].id;
        if(neighbor[nid] < 0)
            continue;
        // printf("send msg to neighbor id = [%d]\n", nid);
        send_msg(location[i].ip, location[i].port, msg);
    }
}

// exchange msg in neighbors.
// update in 5 cases: 
// (1). the cost = -1.
// (2). nid's cost is smaller and positive.
// (3). the next is nid and have different cost. 
//    if neighbor > 0, and (nid'cost < 0) or (it > id's cost), then update neighbor cost. else memmove.
void exchange(char *buf)
{
    rip_header_t *rip = (rip_header_t*)buf;
    int nid = rip->id;
    int dst, d_xz, d_xy, d_yz;
    int flag = 0;
    struct path *ntable = (struct path*)(buf + sizeof(rip_header_t));
    for(int k=0; k<rt_num; k++){
        if(ntable[k].next == router_id && ntable[k].dest == router_id){
            d_xy = ntable[k].cost;
            break;
        }
    } 
    for(int i=0; i<rt_num; i++){ 
        ntable[i].next = nid;
        if(ntable[i].cost >= 0)
            ntable[i].cost += d_xy;
    }
    for(int j=0; j<rt_num; j++){
        if(rt_table[j].cost < 0 || (rt_table[j].cost > ntable[j].cost && ntable[j].cost >= 0)){
            memmove(&rt_table[j], &ntable[j], sizeof(struct path));
            flag = 1;
        }else if(rt_table[j].next == nid && rt_table[j].cost != ntable[j].cost){
            if(neighbor[rt_table[j].dest] > 0 && (ntable[j].cost > neighbor[rt_table[j].dest] || ntable[j].cost < 0)){
                rt_table[j].next = rt_table[j].dest;
                rt_table[j].cost = neighbor[rt_table[j].dest];
            }else{
                memmove(&rt_table[j], &ntable[j], sizeof(struct path));
            }
            flag = 1;
        }
    }

    if(flag)
        send_nbor();
}

void update(char *buf)
{
    int nid, i;
    struct path *ptr = (struct path*)(buf + sizeof(rip_header_t));
    nid = ptr->dest;
    for(i=0; i<rt_num; i++){
        if(rt_table[i].dest == nid)
            break;
    }
    rt_table[i].next = ptr->next;
    if(ptr->cost > 1000)
        ptr->cost = -1;
    rt_table[i].cost = ptr->cost;
    // send_nbor();
    neighbor[nid] = ptr->cost;
}

void *mythread(void* connfd)
{
    pthread_detach(pthread_self());
    int sockfd = *((int*)connfd);
    char buf[BUF_SIZE];
    memset(buf, 0, BUF_SIZE);
    ssize_t n;
    while((n = read(sockfd, buf, BUF_SIZE)) > 0){
        rip_header_t *rip = (rip_header_t*)buf;
        if(rip->id != 0 && neighbor[rip->id] < 0)  // not neighbors, drop msg
            continue;
        
        pthread_mutex_lock(&mutex);

        if(rip->type == RIP_DV){
            printf("doing dv option...\n");
            send_nbor();
        }

        else if(rip->type == RIP_UPDATE){
            printf("doing update...\n");
            update(buf);
        }

        else if(rip->type == RIP_RTR){
            printf("doing rtr...\n");
            exchange(buf);
        }

        else if(rip->type == RIP_SHOW){
            printf("showing rt_table of router %d\n", router_id);
            write(sockfd, rt_table, sizeof(struct path)*rt_num);
        }
        
        else if(rip->type == RIP_RESET){
            printf("reset rt_table of router %d\n", router_id);
            memset(rt_table, 0, sizeof(struct path)*rt_num);
        }
        else printf("error command type\n");
        pthread_mutex_unlock(&mutex);
    }
    close(sockfd);
    return NULL;
}

int router(char* loc, char* conf)
{   
    int listenfd, connfd;
    socklen_t clilen;
    struct sockaddr_in cliaddr, servaddr;
    struct loc *rt_loc; 
    pthread_t tid;
    pthread_mutex_init(&mutex, NULL);

    //get location and rt_table
    table_init(loc, conf, router_id);
    rt_loc = get_loc(location, router_id, rt_num);

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, rt_loc->ip, &servaddr.sin_addr);
    servaddr.sin_port = htons(atoi(rt_loc->port));
    bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
    listen(listenfd, rt_num);
    printf("listening...\n");

    while(1){
        clilen = sizeof(cliaddr);
        connfd = accept(listenfd, (struct sockaddr *) &cliaddr, &clilen);
        printf("success connected\n");
        pthread_create(&tid, NULL, mythread, &connfd);
    }
    close(listenfd);
    return 0;
}



/*
 * main():
 * Parse command-line arguments and call router function
*/
int main(int argc, char **argv) 
{
    char *router_loc_file, *topology_conf_file;

    if (argc != 4) {
        fprintf(stderr, "Usage: ./router <router location file> <topology conf file> <router id>\n");
        exit(-1);
    }

    router_loc_file = argv[1];
    topology_conf_file = argv[2];
    router_id = atoi(argv[3]);

    rt_num = loc_init(router_loc_file);
    return router(router_loc_file, topology_conf_file);
}