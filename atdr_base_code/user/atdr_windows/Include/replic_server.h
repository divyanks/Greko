#include <string.h>

#include "atdr_replication_header.h"
#include "atdr_log.h"
#include "atdr_bitmap_user.h"
#include "atdr_replication.h"
#include "atdr_partner.h"


// extern FILE *ebdrlog_fp;

int get_bitmap(SOCKET sockfd, replic_header *rep_hdr_rcvd);

int get_data(SOCKET sockfd, replic_header *rep_hdr_rcvd);

int make_partner(SOCKET sockfd, replic_header *rep_hdr_rcvd);
int remove_partner(SOCKET sockfd, replic_header *rep_hdr_rcvd);
int remove_relation(SOCKET sockfd, replic_header *rep_hdr_rcvd);

int syn_received(SOCKET sockfd, replic_header *rep_hdr_rcvd);


int ack(SOCKET sockfd, replic_header *rep_hdr_rcvd);
int data_req(SOCKET sockfd, replic_header *rep_hdr_rcvd);

int data_start_recieved(SOCKET sockfd, replic_header *rep_hdr_rcvd );
int terminate(SOCKET sockfd, replic_header *rep_hdr_rcvd );
int verify_data(SOCKET sockfd, replic_header *rep_hdr_rcvd);
int (*fun) (SOCKET sockfd, replic_header *rep_hdr_rcvd);

typedef struct cmdFun
{
	int opcode;
	int(*fun) (SOCKET sockfd, replic_header *rep_hdr_rcvd);
} cmdFun_t;



int server_send_hdr(char *buf, int size, SOCKET sockfd, replic_header *rep_hdr_rcvd);
int server_send_data(char *buf, int size, SOCKET sockfd, replic_header *rep_hdr_rcvd);

int(*server_send_fun) (char *buf, int size, SOCKET sockfd, replic_header *rep_hdr_rcvd);


typedef struct server_send_cmdFun
{
	int opcode;
	int(*server_send_fun) (char *buf, int size, SOCKET sockfd, replic_header *rep_hdr_rcvd);
} server_send_cmdFun_t;

