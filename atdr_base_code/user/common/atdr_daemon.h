#ifndef EBDR_DAEMON_H
#define EBDR_DAEMON_H
#include "ebdr_sys_common.h"
#include "ebdr_fifo.h"

#define START		0
#define STOP 		1

#define MAX_DISKS 2048
#define CNTRL_THREADS 4

extern  char *log_path;
extern __thread THREAD_ID_T log_pid;

#define CNTRL_THREAD_LOG  strcat(log_path,   "cntrl_thread_log.txt")
#define SERVER_LOG strcat(log_path,  "ebdrserverlog.txt")
#define CLIENT_LOG strcat(log_path, "ebdrclient.txt")
#define LEVEL_NAME 64

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


extern int ioctl_fd;
int fifo_fd;

int server_fifo_fd;
int client_fifo_fd;
char str[CMD_LEN];
//extern struct ebdr_user_bitmap *resync_bitmap;
extern struct ebdr_user_bitmap resync_bitmap;
extern long unsigned int *active_bitmap[MAX_DISKS];
extern int resync;
extern void *resyncd(void *args);
extern struct ebdr_thread_t thread_params[MAX_DISKS+CNTRL_THREADS];
#define LOG_PATH_LEN 128
extern char tmp_log_path[LOG_PATH_LEN];

THREAD_ID_T cntrl_thread;
THREAD_ID_T resync_thread;
THREAD_ID_T recovery_thread;

#endif
