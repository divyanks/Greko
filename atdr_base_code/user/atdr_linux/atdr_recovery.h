#include "sys_common.h"

#define RECOVERY_LOG strcat(log_path,  "recovery_log.txt")
void *recovery_thread_init(void *args);
extern char *log_path;

