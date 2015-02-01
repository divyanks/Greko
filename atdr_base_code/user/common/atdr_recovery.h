#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/fs.h>
#include <pthread.h>

void *recovery_thread_init(void *args);
extern char *log_path;
#define RECOVERY_LOG strcat(log_path,  "recovery_log.txt")

