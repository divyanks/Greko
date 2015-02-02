#include "..\Include\atdr_disk.h"
#include "..\Include\atdr_recovery.h"
#include "..\Include\atdr_log.h"
#include "..\Include\atdr_partner.h"
#include "..\Include\atdr_conn.h"
#include "..\Include\atdr_replication.h"
#include "..\Include\atdr_bitmap_user.h"

int restart_disk(int pid)
{
	atdr_disk_target_obj[pid].obj_state = DISK_OBJ_RESUME;
	atdr_log(ATDR_INFO, "restarting [%d] \n",pid);
	resync_init(pid, replic_client_obj[pid].last_resynced_bit);
	//resync_init(pid, 0);   
	return 0;
}

int restart_conn_server(int pid)
{
	all_partner_servers[pid].socket_fd = atdr_conn_server[pid].sockFd;
	printf("[restart_conn_server] pid %d New sever socket fd = %d\n", pid, atdr_conn_client[pid].sockFd);
	if (atdr_conn_client[pid].sockFd > 0)
	{
		printf("[restart_conn_server] pid %d changing conn obj to resume \n", pid);
		atdr_conn_server[pid].obj_state = CONN_OBJ_RESUME;
	}
	return 0;
}


int create_new_conn_client(int pid, char *ip_addr)
{
	int sockFd = 0, ret;
	struct sockaddr_in srvAddr;
	struct linger so_linger;

	atdr_conn_client[pid].atdr_conn_mode = ATDR_CONN_CLIENT;
	atdr_conn_client[pid].atdr_conn_state = ATDR_CONN_SETUP;
	atdr_conn_client[pid].obj_state = CONN_OBJ_IN_USE;

	if((sockFd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		stop_work("Can't open soc_stream socket\n"); 
	}

	so_linger.l_onoff = 1; /* 1 = TRUE*/
	so_linger.l_linger = 0;
	ret = setsockopt(sockFd, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger));
	atdr_log(ATDR_INFO, "[new_conn_client]client socket creation successful new sockfd = %d\n", sockFd);
	memset(&srvAddr, 0, sizeof(srvAddr));
	srvAddr.sin_family = AF_INET;
	srvAddr.sin_addr.s_addr = inet_addr(ip_addr);
	srvAddr.sin_port = htons(SRV_TCP_PORT);

	atdr_conn_client[pid].sockFd = sockFd;
	if(connect(atdr_conn_client[pid].sockFd, (struct sockaddr *)&srvAddr, sizeof(srvAddr)) < 0)
	{
		stop_work("Can't connect to server");
	}
	else
	{
		atdr_log(ATDR_INFO, "connected to the server\n");
	}

	return 0;
}


int restart_conn_client(int pid)
{
	create_new_conn_client(pid, all_partner_clients[pid].ip);
	atdr_conn_client[pid].obj_state = CONN_OBJ_RESUME;
	all_partner_clients[pid].socket_fd = atdr_conn_client[pid].sockFd;
	printf("[restart_conn_client] New client socket fd = %d\n", atdr_conn_client[pid].sockFd);
	resync_init(pid, replic_client_obj[pid].last_resynced_bit);
	return 0;
}



int recovery_disk_client(int pid)
{
	int ret;

    #define WCHAR_BUFFER_SIZE 28
	WCHAR tmp[28];
	WCHAR disk_name[WCHAR_BUFFER_SIZE];
	int length = 0;

	if(atdr_disk_target_obj[pid].obj_state == DISK_OBJ_PAUSE)
	{
		ret = CloseHandle(atdr_disk_target_obj[pid].disk_fd);

		if(ret < 0)
		{
			atdr_log(ATDR_ERROR, "ATDR_ERROR closing of  atdr_disk_target_obj[%d] \n",pid);
			stop_work("atexit_func");
		}

		wcscpy(disk_name, L"\\\\.\\");
		length = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, atdr_disk_target_obj[pid].name, -1, (LPWSTR)tmp,
			WCHAR_BUFFER_SIZE / sizeof(WCHAR));
		wcscat(&disk_name[4], tmp);
		if (length <= 0)
		{
			atdr_log(ATDR_ERROR, "Failed to convert to multibyte wide characters %d\n", GetLastError()); /* This print will change later */
			return -1;
		}
		if ((atdr_disk_target_obj[pid].disk_fd = CreateFile(disk_name, GENERIC_WRITE | GENERIC_READ, // open for writing
			FILE_SHARE_WRITE, // share for writing
			NULL, // default security
			OPEN_EXISTING, // create new file only
			FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_RANDOM_ACCESS,
			// normal file archive and impersonate client
			NULL)) == INVALID_HANDLE_VALUE)
		{
			atdr_log(ATDR_ERROR, "INVALID_FILE SOURCE %d\n", GetLastError()); /* This print will change later */
			return -1;
		}


		restart_disk(pid);	
	}
	return 0;
}


int recovery_disk_server(int pid)
{
	int ret;

    #define WCHAR_BUFFER_SIZE 28
	WCHAR tmp[28];
	WCHAR disk_name[WCHAR_BUFFER_SIZE];
	int length = 0;

	if(atdr_disk_src_obj[pid].obj_state == DISK_OBJ_PAUSE)
	{
		ret = CloseHandle(atdr_disk_src_obj[pid].disk_fd);
		atdr_log(ATDR_INFO, "[recovery_disk_server] Sleeping pid %d for 60 secs...\n", pid);
		Sleep(60);
		if(ret < 0)
		{
			atdr_log(ATDR_ERROR, "error closing of  atdr_disk_target_obj[%d] \n",pid);
			stop_work("atexit_func");
		}

		atdr_log(ATDR_INFO, "Resuming disk server object... \n");
		wcscpy(disk_name, L"\\\\.\\");
		length = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, atdr_disk_src_obj[pid].name, -1, (LPWSTR)tmp,
			WCHAR_BUFFER_SIZE / sizeof(WCHAR));
		wcscat(&disk_name[4], tmp);
		if (length <= 0)
		{
			atdr_log(ATDR_ERROR, "Failed to convert to multibyte wide characters %d\n", GetLastError()); /* This print will change later */
			return -1;
		}

		if ((atdr_disk_src_obj[pid].disk_fd = CreateFile(disk_name, GENERIC_WRITE | GENERIC_READ, // open for writing
			FILE_SHARE_WRITE, // share for writing

			NULL, // default security

			OPEN_EXISTING, // create new file only

			FILE_ATTRIBUTE_NORMAL || FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH,

			// normal file archive and impersonate client

			NULL)) == INVALID_HANDLE_VALUE)
		{
			atdr_log(ATDR_ERROR, "INVALID_FILE SOURCE %d\n", GetLastError()); /* This print will change later */
			return -1;
		}
		atdr_disk_src_obj[pid].obj_state = DISK_OBJ_RESUME;
	}
	return 0;
}


int recovery_conn_client(int pid)
{
	if(atdr_conn_client[pid].obj_state == CONN_OBJ_PAUSE)
	{
		restart_conn_client(pid);
	}
	return 0;
}


int recovery_conn_server(int pid)
{
	if(atdr_conn_server[pid].obj_state == CONN_OBJ_PAUSE)
	{
		printf("[recovery_conn_server] pid %d is in pause state\n", pid);
		restart_conn_server(pid);
	}
	return 0;
}


#if 0
DWORD  WINAPI recovery_thread_init(LPVOID lpParam)
{
		printf("The parameter: %u.\n", *(DWORD*)lpParam);
		printf("inside recovery thread\n");
		return 0;

	int pid; 
	printf("In recovery thread \n");

	log_pid = MAX_DISKS+CNTRL_THREADS - 3;

	atdr_log_setup(log_pid, RECOVERY_LOG);


	while(1)
	{

		for(pid = 0; pid < MAX_PARTNERS; pid++)
		{
#ifdef CLIENT
			recovery_disk_client(pid);
			recovery_conn_client(pid);
#endif
#ifdef SERVER
			recovery_disk_server(pid);
			recovery_conn_server(pid);

		}
		Sleep(10); 
	}
#endif

}

#endif
