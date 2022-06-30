#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>

#include "util.h"
#include "opt_rtp.h"

#define RECV_BUFFER_SIZE 32768  // 32KB

int receiver(char *receiver_port, int window_size, char* file_name) {

  // create rtp socket file descriptor
  int receiver_fd = rtp_socket(window_size);
  if (receiver_fd == 0) {
    perror("create rtp socket failed");
    exit(EXIT_FAILURE);
  }

  // create socket address
  // forcefully attach socket to the port
  struct sockaddr_in address;
  memset(&address, 0, sizeof(struct sockaddr_in));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(atoi(receiver_port));

  // bind rtp socket to address
  if (rtp_bind(receiver_fd, (struct sockaddr *)&address, sizeof(struct sockaddr))<0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  int recv_bytes;
  struct sockaddr_in sender;
  socklen_t addr_len = sizeof(struct sockaddr_in);

  // listen to incoming rtp connection
  if(rtp_listen(receiver_fd, 1, (struct sockaddr*)&sender, &addr_len) < 0){
    printf("wrong connection!\n");
    return 0;
  }
  // accept the rtp connection
  rtp_accept(receiver_fd, (struct sockaddr*)&sender, &addr_len);


 // receive packet
  char* buffer = (char*)calloc(FILE_SIZE, sizeof(char));
  r_window = (struct window_state*)calloc(1, sizeof(struct window_state));
  FILE *fp;
  if((fp = fopen(file_name, "w")) == NULL){
    perror("open file failed");
    exit(EXIT_FAILURE);
  }
  recv_bytes = rtp_recvfrom(receiver_fd, (void *)buffer, FILE_SIZE, 0, (struct sockaddr*)&sender, &addr_len);
  if(recv_bytes < 0){
    perror("receive failed");
    exit(EXIT_FAILURE);
  }

  buffer[recv_bytes] = '\0';
  if(fwrite(buffer, sizeof(char), recv_bytes, fp) < 0){
    perror("write failed");
    exit(EXIT_FAILURE);
  }
  free(r_window);
  fclose(fp);
  rtp_close(receiver_fd);
  printf("finish receive msg\n");
  return 0;
}

/*
 * main():
 * Parse command-line arguments and call receiver function
*/
int main(int argc, char **argv) {
    char *receiver_port;
    int window_size;
    char *file_name;

    if (argc != 4) {
        fprintf(stderr, "Usage: ./receiver [Receiver Port] [Window Size] [File Name]\n");
        exit(EXIT_FAILURE);
    }

    receiver_port = argv[1];
    window_size = atoi(argv[2]);
    file_name = argv[3];
    return receiver(receiver_port, window_size, file_name);
}
