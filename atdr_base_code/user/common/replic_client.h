#include <sys/types.h>
#include <sys/socket.h>
#include "ebdr_replication_header.h"
#include "ebdr_replication.h"

int send_hdr(char *buf, int size, int sockfd, replic_header *rep_hdr_rcvd);
int send_data(char *buf, int size, int sockfd, replic_header *rep_hdr_rcvd);

int (*fun) ( char *buf, int size, int sockfd, replic_header *rep_hdr_rcvd);


int recv_hdr(char *buf, int size, int sockfd, replic_header *rep_hdr_rcvd);
int recv_data(char *buf, int size, int sockfd, replic_header *rep_hdr_rcvd);

int recv_data_resp(char *buf, int size, int sockfd, replic_header *rep_hdr_rcvd);

int send_data_start(char *buf, int size, int sockfd, replic_header *rep_hdr_rcvd);
int verify_data_resp(int , replic_header * rep_hdr_rcvd);
typedef struct cmdFun
{
  int opcode;
  int (*fun) ( char *buf, int size, int sockfd, replic_header *rep_hdr_rcvd) ;
} cmdFun_t;

