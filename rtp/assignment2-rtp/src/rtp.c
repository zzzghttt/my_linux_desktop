#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>

#include "rtp.h"
#include "util.h"

void cache_add(char** cache, char *frame){
    rtp_header_t *rtp = (rtp_header_t*)frame;
    int index = rtp->seq_num % rcb->window_size;
    // printf("add cache[%d]\n",index);
    memcpy(cache[index], frame, BUFFER_SIZE);
}

void cache_de(char** cache, uint32_t i){
    int index = i % rcb->window_size;
    // printf("---delete cache[%d]\n",index);
    memset(cache[index], 0, BUFFER_SIZE);
}

char* cache_get(char** cache, uint32_t seqnum){
    int index = seqnum % rcb->window_size;
    rtp_header_t *rtp = (rtp_header_t *)cache[index];
    if(rtp->type != RTP_DATA){
        return NULL;
    }
    return cache[index];
}

//varify checksum
int check_cksum(rtp_header_t *rtp, char* buffer, int recv_bytes){
    uint32_t pkt_checksum = rtp->checksum;
    rtp->checksum = 0;
    uint32_t computed_checksum = compute_checksum(buffer, recv_bytes);
    if (pkt_checksum != computed_checksum) {
        return 0;
    }
    return 1;
}

void rcb_init(uint32_t window_size) {
    if (rcb == NULL) {
        rcb = (rcb_t *) calloc(1, sizeof(rcb_t));
    } else {
        perror("The current version of the rtp protocol only supports a single connection");
        exit(EXIT_FAILURE);
    }
    rcb->window_size = window_size;
    rcb->seq_num = 0;
    // TODO: you can initialize your RTP-related fields here
}

/*********************** Note ************************/
/* RTP in Assignment 2 only supports single connection.
/* Therefore, we can initialize the related fields of RTP when creating the socket.
/* rcb is a global varialble, you can directly use it in your implementatyion.
/*****************************************************/
int rtp_socket(uint32_t window_size) {
    rcb_init(window_size); 
    // create UDP socket
    return socket(AF_INET, SOCK_DGRAM, 0);  
}


int rtp_bind(int sockfd, struct sockaddr *addr, socklen_t addrlen) {
    return bind(sockfd, addr, addrlen);
}

int send_ack(int sockfd, uint32_t seqnum, const struct sockaddr *to, socklen_t tolen){

    // Send the ACK
    char buffer[BUFFER_SIZE];
    rtp_header_t* rtp = (rtp_header_t*)buffer;
    rtp->length = 0;
    rtp->checksum = 0;
    rtp->seq_num = seqnum;    //expected next seq num
    rtp->type = RTP_ACK;
    rtp->checksum = compute_checksum((void *)buffer, sizeof(rtp_header_t));

    int sent_bytes = sendto(sockfd, (void*)buffer, sizeof(rtp_header_t), 0, to, tolen);
    if (sent_bytes != (sizeof(struct RTP_header))) {
        perror("send error");
        exit(EXIT_FAILURE);
    }
    return 1;
}

int send_end(int sockfd, uint32_t seqnum, const struct sockaddr *to, socklen_t tolen){

    char buffer[BUFFER_SIZE];
    rtp_header_t* rtp = (rtp_header_t*)buffer;
    rtp->length = 0;
    rtp->checksum = 0;
    rtp->seq_num = seqnum;    //expected next seq num
    rtp->type = RTP_END;
    rtp->checksum = compute_checksum((void *)buffer, sizeof(rtp_header_t));

    FD_ZERO(&rset4);
    FD_SET(sockfd, &rset4);
    int sent_bytes = sendto(sockfd, (void*)buffer, sizeof(rtp_header_t), 0, to, tolen);
    if (sent_bytes != (sizeof(struct RTP_header))) {
        perror("connect error");
        exit(EXIT_FAILURE);
    }
    switch(select(sockfd+1, &rset4, NULL, NULL, &re_timer)){
        case -1: exit(EXIT_FAILURE);
            break;
        case 0: 
          //end ack loss, close connection
            break;
        default:
        if(FD_ISSET(sockfd, &rset4)){
            char buffer[2048];
            int recv_bytes = recvfrom(sockfd, buffer, 2048, 0, (struct sockaddr*)to, &tolen);
            rtp_header_t* recv_rtp = (rtp_header_t*)buffer;
            if(recv_rtp->type == RTP_ACK)
            {
                if(check_cksum(recv_rtp, buffer, recv_bytes) == 0){
                    printf("received incorrect end ack\n");
                    return 0;
                }
                printf("------receive an END ACK------seqnum = %d\n", recv_rtp->seq_num);
                return 1;
            }
        }
        break;
    }
    return 0;
}

int rtp_listen(int sockfd, int backlog, struct sockaddr *addr, socklen_t *addrlen) {
    // In standard POSIX API, backlog is the number of connections allowed on the incoming queue.
    // For RTP, backlog is always 1
    printf("listening...\n");
    while(1){
        FD_ZERO(&rset2);
        FD_SET(sockfd, &rset2);
        switch(select(sockfd+1, &rset2, NULL, NULL, &re_timer)){
            case -1: exit(EXIT_FAILURE);
                break;
            case 0: break;
            default:
            if(FD_ISSET(sockfd, &rset2)){
                char buffer[2048];
                int recv_bytes = recvfrom(sockfd, buffer, 2048, 0, addr, addrlen);
                rtp_header_t* recv_rtp = (rtp_header_t*)buffer;
                if(recv_rtp->type == RTP_START)
                    if(check_cksum(recv_rtp, buffer, recv_bytes) == 0)
                        return -1;
                    printf("------receive a START msg------\n");
                    send_ack(sockfd, recv_rtp->seq_num, (const struct sockaddr*)addr, *addrlen);
                return 1;
            }
                break;
        }
        
    }
    return -1;
}


int rtp_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    // Since RTP in Assignment 2 only supports one connection,
    // there is no need to implement accpet function.
    // You don’t need to make any changes to this function.
    return 1;
}

int rtp_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {

    char buffer[BUFFER_SIZE];
    rtp_header_t* rtp = (rtp_header_t*)buffer;
    rtp->length = 0;
    rtp->checksum = 0;
    rtp->seq_num = 0;
    rtp->type = RTP_START;
    rtp->checksum = compute_checksum((void *)buffer, sizeof(rtp_header_t));

    //send START msg
    FD_ZERO(&rset1);
    FD_SET(sockfd, &rset1);
    int sent_bytes = sendto(sockfd, (void*)rtp, sizeof(rtp_header_t), 0, addr, addrlen);
    if (sent_bytes != (sizeof(struct RTP_header))) {
        perror("connect error");
        exit(EXIT_FAILURE);
    }
    switch(select(sockfd+1, &rset1, NULL, NULL, &re_timer)){
        case -1: exit(EXIT_FAILURE);
            break;
        case 0: 
            send_end(sockfd, 0, addr, addrlen);
            break;
        default:
        if(FD_ISSET(sockfd, &rset1)){
            char buffer[2048];
            int recv_bytes = recvfrom(sockfd, buffer, 2048, 0, (struct sockaddr*)addr, &addrlen);
            rtp_header_t* recv_rtp = (rtp_header_t*)buffer;
            if(recv_rtp->type == RTP_ACK)
            {
                if(check_cksum(recv_rtp, buffer, recv_bytes) == 0){
                    send_end(sockfd, 0, addr, addrlen);
                    return 0;
                }
                printf("------receive an ACK msg------\n");
                return 1;
            }
        }
            break;
    }
    return 0;
}

int rtp_close(int sockfd) {
    return close(sockfd);
}


int rtp_sendto(int sockfd, const void *msg, int len, int flags, const struct sockaddr *to, socklen_t tolen) {
    rtp_header_t* rtp = (rtp_header_t*)msg;
    rcb->seq_num = rtp->seq_num;
    int sent_bytes = sendto(sockfd, (void *)msg, len, 0, to, tolen);
        if (sent_bytes != len) {
            printf("in rtp_sendto:\n sockfd: %d \n sockaddr:%p \n sent_bytes:%d\n", 
                sockfd, to, sent_bytes);
            printf("len=%d, seq_num = %u", len, rtp->seq_num);
            perror("send error");
            exit(EXIT_FAILURE);
        }
    return 1;
}



int rtp_recvfrom(int sockfd, void *buf, int len, int flags,  struct sockaddr *from, socklen_t *fromlen) {

    r_window->sz = rcb->window_size;
    r_window->left = 0;
    r_window->right = r_window->left + r_window->sz - 1;
    r_window->Sf = 0;   //next expected pkg
    r_window->Sn = 0;   //the highest received seqnum pkg + 1

    r_cache = (char**)calloc(rcb->window_size, sizeof(char*));
    for(int i=0; i<r_window->sz; i++){
      r_cache[i] = (char*)calloc(1, BUFFER_SIZE);
    }
    char buffer[BUFFER_SIZE];
    uint32_t offset = 0; //for memcpy to buf
    int end_flag = 0;
    int end_seqnum = 0;
    int countnum = 0;
    while(1){
        int recv_bytes = recvfrom(sockfd, buffer, BUFFER_SIZE, flags, from, fromlen);
        if (recv_bytes < 0) {
            perror("receive error");
            exit(EXIT_FAILURE);
        }
        buffer[recv_bytes] = '\0';
        // extract header
        rtp_header_t *rtp = (rtp_header_t *)buffer;
        
        // verify checksum
        if (check_cksum(rtp, buffer, recv_bytes) == 0) {
            //drop the pkg 
            memset(buffer, 0, BUFFER_SIZE);
            continue;
        }
        // verify the pkg type
        if(rtp->type == RTP_END){
            end_flag = 1;
            end_seqnum = rtp->seq_num;
            if(r_window->Sf >= end_seqnum){
                send_ack(sockfd, end_seqnum, from, *fromlen);
                break;
            }
            continue;
        }
        else if(rtp->type != RTP_DATA){
            memset(buffer, 0, BUFFER_SIZE);
            continue;
        }

        
        //verify seq_num
        
        //drop pkg and send ack ask for the pkg we need
        if(rtp->seq_num < r_window->left){
            send_ack(sockfd, r_window->Sf, from, *fromlen);
            memset(buffer, 0, BUFFER_SIZE);
            continue;
        } 
        else if(rtp->seq_num > r_window->right){
            send_ack(sockfd, r_window->Sf, from, *fromlen);
            memset(buffer, 0, BUFFER_SIZE);
            continue;
        } 
        //buffers the pkg to cache
        else if(rtp->seq_num != r_window->Sf){
            if(rtp->seq_num >= r_window->Sn){
                //update the highest seqnum
                r_window->Sn = rtp->seq_num + 1;
            }
            cache_add(r_cache, buffer);
            send_ack(sockfd, r_window->Sf, from, *fromlen);
        }   
        //received the pkg we need and copy the sequent pkg to buf, send ack
        else{
            cache_add(r_cache, buffer);
            char *buffer2;
            while((buffer2 =  cache_get(r_cache, r_window->Sf)) != NULL){
                rtp_header_t *buf_rtp = (rtp_header_t*)buffer2;
                memcpy(buf + offset, (void*)buffer2+sizeof(rtp_header_t), buf_rtp->length);
                offset += buf_rtp->length;
                cache_de(r_cache, r_window->Sf);
                r_window->Sf++;
                if(r_window->Sf == end_seqnum)
                    break;
                if(r_window->Sf >= r_window->Sn)
                    r_window->Sn = r_window->Sf;
                r_window->left++;
                r_window->right++;
            }
            send_ack(sockfd, r_window->Sf, from, *fromlen);
        }
        memset(buffer, 0, BUFFER_SIZE);

        countnum += recv_bytes - sizeof(rtp_header_t);
        //check END condition
        if(end_flag == 1 && r_window->Sf >= end_seqnum){
            send_ack(sockfd, end_seqnum, from, *fromlen);
            break;
        }
    }

    free(r_cache);
    for(int i=0; i<r_window->sz; i++){
      free(r_cache[i]);
    }
    free(rcb);
    return countnum;
}