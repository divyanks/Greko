#define RECOVERY_LOG strcat(log_path,  "recovery_log.txt")
DWORD  WINAPI recovery_thread_init(LPVOID lpParam);
extern char *log_path;

