#include <windows.h>
#include "..\Include\atdr_daemon.h"


int atdr_log_setup(int log_pid, char *file_name)
{
	int ret = 0;
	thread_params[log_pid].atdrlog_fp = fopen(file_name, "a+");
	if (thread_params[log_pid].atdrlog_fp == NULL)
	{
		printf("Unable to open the file %d \n", errno);
	}

	if (thread_params[log_pid].atdrlog_fp == NULL)
	{
		printf("Unable to open the file, %d", GetLastError());
	}
	thread_params[log_pid].pid = log_pid;
	thread_params[log_pid].logging_level = 4;
	strcpy(thread_params[log_pid].logStrng, "DATUG");
	strcpy(log_path, tmp_log_path);

	return ret;
}

