#ifndef EBDR_DAEMON_H
#define EBDR_DAEMON_H
#include <stdio.h>
#include <string.h>

#include "atdr_fifo.h"


#define START		0
#define STOP 		1

#define MAX_DISKS 2048
#define CNTRL_THREADS 4

extern  char *log_path;
extern int log_pid;

#define CNTRL_THREAD_LOG  strcat(log_path,   "cntrl_thread_log.txt")
#define SERVER_LOG strcat(log_path,  "ebdrserverlog.txt")
#define CLIENT_LOG strcat(log_path, "ebdrclient.txt")
#define LEVEL_NAME 64

extern void stop_work(char str[100]);


typedef struct user_ioctl
{
	char src_disk[64];
	unsigned long int u_grain_size;
	unsigned long int u_bitmap_size;
	int bmap_user;
	int bmap_ver;
	int index;
}IOCTL;

enum
{
	KERNEL,
	USER,
};

struct ebdr_thread_t
{
	int pid;
	unsigned long int bit_pos;
	int logging_level;
	int logging;
	FILE *ebdrlog_fp;
	char logStrng[LEVEL_NAME];
};



int server_fifo_fd;
int client_fifo_fd;
char str[CMD_LEN];
#define LOG_PATH_LEN 128
extern char tmp_log_path[LOG_PATH_LEN];
extern struct ebdr_thread_t thread_params[MAX_DISKS + CNTRL_THREADS];
int ebdr_log_setup(int log_pid, char *file_name);

#endif
