#ifndef ATDR_DAEMON_H
#define ATDR_DAEMON_H

#include <stdio.h>
#include "atdr_fifo.h"
#include "../Include/atdr_log.h"

#define START		0
#define STOP 		1

#define MAX_DISKS 2048
#define CNTRL_THREADS 4

extern  char *log_path;
extern int log_pid;

#define CNTRL_THREAD_LOG  strcat(log_path,   "cntrl_thread_log.txt")
#define SERVER_LOG strcat(log_path,  "server.txt")
#define CLIENT_LOG strcat(log_path, "client.txt")
#define LEVEL_NAME 64

extern void stop_work(char str[100]);

typedef struct user_ioctl
{
	char src_disk[64];
    unsigned int u_grain_size;
	unsigned int u_bitmap_size;
	unsigned int bmap_user;
	unsigned int bmap_ver;
	unsigned int index;
}IOCTL;

enum
{
	KERNEL,
	USER,
};

struct atdr_thread_t
{
	int pid;
	unsigned long int bit_pos;
	int logging_level;
	int logging;
	FILE *atdrlog_fp;
	char logStrng[LEVEL_NAME];
};


char str[CMD_LEN];
#define LOG_PATH_LEN 128
extern char tmp_log_path[LOG_PATH_LEN];
extern struct atdr_thread_t thread_params[MAX_DISKS + CNTRL_THREADS];
int prepare_and_send_ioctl_cmd(int cmd);
void stop_work(char str[100]);
int request_server_for_metadata(int client_pid);
void prepare_for_replication(char *target_disk, int client_pid);
int resync_init(int pid, unsigned long int bit_pos);
int make_relation_with_client(int partnerid, int role, char *src_disk, unsigned long int bitmap_size,
	unsigned long int grain_size, unsigned long int chunk_size);
int make_relation_with_server(int partnerid, int role, char *target_disk, unsigned long int bitmap_size,
	unsigned long int grain_size, unsigned long int chunk_size);
#endif
