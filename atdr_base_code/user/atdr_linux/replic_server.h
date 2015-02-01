
#include "ebdr_replication_header.h"
#include "ebdr_log.h"
#include "ebdr_bitmap_user.h"
#include "ebdr_replication.h"
#include "ebdr_partner.h"

int get_bitmap(int sockfd, replic_header *rep_hdr_rcvd);

int get_data(int sockfd, replic_header *rep_hdr_rcvd);

int make_partner(int sockfd, replic_header *rep_hdr_rcvd);
int remove_partner(int sockfd, replic_header *rep_hdr_rcvd);
int remove_relation(int sockfd, replic_header *rep_hdr_rcvd);

int syn_received(int sockfd, replic_header *rep_hdr_rcvd);


int ack(int sockfd, replic_header *rep_hdr_rcvd);
int data_req(int sockfd, replic_header *rep_hdr_rcvd);

int data_start_recieved( int sockfd, replic_header *rep_hdr_rcvd );
int terminate( int sockfd, replic_header *rep_hdr_rcvd );
int verify_data(int sockfd, replic_header *rep_hdr_rcvd);
int (*fun) (int sockfd, replic_header *rep_hdr_rcvd);


typedef struct cmdFun
{
  int opcode;
  int (*fun) (int sockfd, replic_header *rep_hdr_rcvd ) ;
} cmdFun_t;



int server_send_hdr(char *buf, int size, int sockfd, replic_header *rep_hdr_rcvd);
int server_send_data(char *buf, int size, int sockfd, replic_header *rep_hdr_rcvd);

int (*server_send_fun) ( char *buf, int size, int sockfd, replic_header *rep_hdr_rcvd);


typedef struct server_send_cmdFun
{
  int opcode;
  int (*server_send_fun) ( char *buf, int size, int sockfd, replic_header *rep_hdr_rcvd) ;
} server_send_cmdFun_t;

