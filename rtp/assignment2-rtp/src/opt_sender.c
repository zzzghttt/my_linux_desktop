#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "util.h"
#include "opt_rtp.h"

int end_seqnum;

void* data2frame(char *data){
    static char frame[BUFFER_SIZE];
    int len = strlen(data);
    rtp_header_t* rtp = (rtp_header_t*)frame;
    rtp->length = len;
    rtp->checksum = 0;
    rtp->seq_num = rcb->seq_num;
    rtp->type = RTP_DATA;
    memcpy((void *)frame+sizeof(rtp_header_t), data, len);
    rtp->checksum = compute_checksum((void *)frame, sizeof(rtp_header_t) + len);

    return (void*)frame;
}


void *mythread(){
    char* data;
    while(1){
        FD_ZERO(&rset3);
        FD_SET(rcb->sockfd, &rset3);
        switch(select((rcb->sockfd)+1, &rset3, NULL, NULL, &re_timer)){
            case -1:    //error
                return NULL;
                break;
            case 0: //retransmition
                while((data = cache_get(cache, window->Sf)) == NULL){} //wait
                rtp_header_t *rtp = (rtp_header_t*)data;
                rtp_sendto(rcb->sockfd, (void *)data, rtp->length + sizeof(rtp_header_t), 0, (const struct sockaddr*)rcb->receiver_addr, rcb->receiver_len);
                break; 
            default:
            if(FD_ISSET(rcb->sockfd, &rset3)){
                char buffer[2048];
                recvfrom(rcb->sockfd, buffer, 2048, 0, rcb->receiver_addr, &rcb->receiver_len);
                rtp_header_t* recv_rtp = (rtp_header_t*)buffer;
                if(recv_rtp->type == RTP_ACK && recv_rtp->seq_num >= window->Sf)
                {
                    printf("------receive an ACK msg------seq_num = %u\n", recv_rtp->seq_num);
                    // free data buffered in the cache
                    cache_de(cache, recv_rtp->seq_num);
                    
                    //slide the window
                    int movesz = 0;
                    for(movesz = 0; movesz < window->sz; movesz++){
                      if(cache_get(cache, window->Sf + movesz) != NULL)
                        break;
                    }
                    window->Sf += movesz;
                    window->left += movesz;
                    window->right += movesz;
                    //all ACK received, finish transmition
                    if(window->Sf >= end_seqnum)
                        return NULL;
                }else{
                    while((data = cache_get(cache, window->Sf)) == NULL){} //wait
                    rtp_header_t *rtp = (rtp_header_t*)data;
                    rtp_sendto(rcb->sockfd, (void *)data, rtp->length + sizeof(rtp_header_t), 0, (const struct sockaddr*)rcb->receiver_addr, rcb->receiver_len);
                }
            }
              break;
        }
    }
    return NULL;
}

int send_msg(int sockfd, const void *msg, int len, int flags, const struct sockaddr *to, socklen_t tolen) {
    end_seqnum = len/DATASIZE + 1;

    window->sz = rcb->window_size;
    window->left = 0;
    window->right = window->left + window->sz - 1;
    window->Sf = 0;
    window->Sn = 0;
    cache = (char**)calloc(rcb->window_size, sizeof(char*));
    for(int i=0; i<window->sz; i++){
      cache[i] = (char*)calloc(1, BUFFER_SIZE);
    }
    char data[BUFFER_SIZE];
    int send_num;
    uint64_t n = 0;
    int restlen = len;
    pthread_t tid;
    while(1){
      if(window->Sn > window->right)
          continue;
      memset(data, 0, BUFFER_SIZE);
      if(restlen >= DATASIZE){
          memcpy((void*)data, msg + n, DATASIZE);
          n += DATASIZE;
          restlen -= DATASIZE;
      }else if(restlen == 0){
          pthread_join(tid, NULL);
          send_end(sockfd, rcb->seq_num, to, tolen);
          break;
      }else{
          memcpy((void*)data, msg + n, restlen);
          restlen = 0;
      }
      char *buffer = (char*)data2frame(data);
      rcb->seq_num++;
      //add this buffer to cache
      cache_add(cache, buffer);
      //send buffer data now
      rtp_sendto(sockfd, (void *)buffer, FRAMESIZE, 0, to, tolen);
      if(window->Sn == 0)
        pthread_create(&tid, NULL, mythread, NULL);
      window->Sn++;
    }
    free(cache);
    for(int i=0; i<window->sz; i++){
      free(cache[i]);
    }
    return 1;
}

int sender(char *receiver_ip, char* receiver_port, int window_size, char* message){

  // create socket
  int sock = 0;
  if ((sock = rtp_socket(window_size)) < 0) {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  // create receiver address
  struct sockaddr_in receiver_addr;
  memset(&receiver_addr, 0, sizeof(receiver_addr));
  receiver_addr.sin_family = AF_INET;
  receiver_addr.sin_port = htons(atoi(receiver_port));

  // convert IPv4 or IPv6 addresses from text to binary form
  if(inet_pton(AF_INET, receiver_ip, &receiver_addr.sin_addr)<=0) {
    perror("address failed");
    exit(EXIT_FAILURE);
  }

  // connect to server
  if(rtp_connect(sock, (const struct sockaddr *)&receiver_addr, sizeof(struct sockaddr)) != 1){
    printf("connection error\n");
    rtp_close(sock);
    return 0;
  }

  //init rcb
  if (rcb == NULL)
      rcb = (rcb_t *) calloc(1, sizeof(rcb_t));
  rcb->window_size = window_size;
  rcb->sockfd = sock;
  rcb->receiver_addr = (struct sockaddr*)&receiver_addr;
  rcb->receiver_len = sizeof(struct sockaddr);
  rcb->seq_num = 0; //the pkg num should send

  window = (struct window_state*)calloc(1, sizeof(struct window_state));
  // send data
  // TODO: if message is filename, open the file and send its content
  FILE *fp;
  if((fp = fopen(message, "r")) == NULL){
    //not a file, send message
    send_msg(sock, (void *)message, strlen(message), 0, (const struct sockaddr*)&receiver_addr, sizeof(struct sockaddr));
  }else{
    //send file
    //TODO:pthread used in rtp.c
    char* buffer = (char*)calloc(FILE_SIZE, sizeof(char));
    int readnum;
    if((readnum = fread(buffer, sizeof(char), FILE_SIZE, fp)) < 0)
      printf("read error");
    send_msg(sock, (void *)buffer, readnum, 0, (const struct sockaddr*)&receiver_addr, sizeof(struct sockaddr));
    
    fclose(fp);
  }
  free(window);
  free(rcb);
  // close rtp socket
  rtp_close(sock);
  printf("finish send msg\n");
  return 0;
}



/*
 * main()
 * Parse command-line arguments and call sender function
*/
int main(int argc, char **argv) {
  char *receiver_ip;
  char *receiver_port;
  int window_size;
  char *message;

  if (argc != 5) {
    fprintf(stderr, "Usage: ./sender [Receiver IP] [Receiver Port] [Window Size] [message]");
    exit(EXIT_FAILURE);
  }

  receiver_ip = argv[1];
  receiver_port = argv[2];
  window_size = atoi(argv[3]);
  message = argv[4];
  return sender(receiver_ip, receiver_port, window_size, message);
}