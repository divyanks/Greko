#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "ebdr_disk.h"
#include "ebdr_recovery.h"
#include "ebdr_log.h"
#include "ebdr_partner.h"
#include "ebdr_conn.h"
#include "ebdr_replication.h"
#include "ebdr_bitmap_user.h"

extern THREAD_ID_T resync_tid[MAX_DISKS];
extern int link_down;
extern int client_connected;
extern int recovery_mode;

extern THREAD_ID_T replic_tid;
extern pthread_attr_t attr;

extern THREAD_ID_T send_recv_tid;
extern pthread_attr_t srattr;

struct recovery_thread_t
{
	int sockfd;
	int pid;
};

int restart_disk(int pid)
{
	ebdr_disk_target_obj[pid].obj_state = DISK_OBJ_RESUME;
	ebdr_log(INFO, "restarting [%d] \n",pid);
	resync_init(pid, 0);   
}

int restart_conn_server(pid)
{
	all_partner_servers[pid].socket_fd = ebdr_conn_server[pid].sockFd;
	printf("[restart_conn_server] New sever socket fd = %d\n", ebdr_conn_client[pid].sockFd);
}


int create_new_conn_client(int pid, char *ip_addr)
{

	int sockFd = 0, ret;
	struct sockaddr_in srvAddr;
	struct linger so_linger;

	if((sockFd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		stop_work("Can't open soc_stream socket\n"); 
	}

	so_linger.l_onoff = 1; /* 1 = TRUE*/
	so_linger.l_linger = 0;
	ret = setsockopt(sockFd, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger));
	ebdr_log(INFO, "[new_conn_client]client socket creation successful new sockfd = %d\n", sockFd);
	memset(&srvAddr, 0, sizeof(srvAddr));
	srvAddr.sin_family = AF_INET;
	srvAddr.sin_addr.s_addr = inet_addr(ip_addr);
	srvAddr.sin_port = htons(SRV_TCP_PORT);

	ebdr_conn_client[pid].sockFd = sockFd;
	if(connect(ebdr_conn_client[pid].sockFd, (struct sockaddr *)&srvAddr, sizeof(srvAddr)) < 0)
	{
		stop_work("Can't connect to server");
	}
	else
	{
		ebdr_log(INFO, "connected to the server\n");
	}

	return 0;
}


int restart_conn_client(pid)
{
	create_new_conn_client(pid, all_partner_clients[pid].ip);
	all_partner_clients[pid].socket_fd = ebdr_conn_client[pid].sockFd;
	printf("[restart_conn_client] New client socket fd = %d\n", ebdr_conn_client[pid].sockFd);
	resync_init(pid, replic_client_obj[pid].last_resynced_bit);
}



int recovery_disk_client(pid)
{
	int i, ret;

	if(ebdr_disk_target_obj[pid].obj_state == DISK_OBJ_PAUSE)
	{
		ret = close(ebdr_disk_target_obj[pid].disk_fd);

		if(ret < 0)
		{
			ebdr_log(ERROR, "error closing of  ebdr_disk_target_obj[%d] \n",pid);
			stop_work("atexit_func");
		}

		if((ebdr_disk_target_obj[pid].disk_fd = open(ebdr_disk_target_obj[i].name,
						O_RDWR | O_TRUNC| O_SYNC | O_LARGEFILE)) < 0)
		{
			ebdr_log(ERROR, "error opening of disk = %s \n", ebdr_disk_target_obj[i].name);
			stop_work("Disk Open failed");
		}

		restart_disk(pid);	
	}
}


int recovery_disk_server(pid)
{
	int i, ret;

	if(ebdr_disk_src_obj[pid].obj_state == DISK_OBJ_PAUSE)
	{
		ret = close(ebdr_disk_src_obj[pid].disk_fd);

		if(ret < 0)
		{
			ebdr_log(ERROR, "error closing of  ebdr_disk_target_obj[%d] \n",pid);
			stop_work("atexit_func");
		}

		if((ebdr_disk_src_obj[pid].disk_fd = open(ebdr_disk_src_obj[i].name,
						O_RDWR | O_TRUNC| O_SYNC | O_LARGEFILE)) < 0)
		{
			ebdr_log(ERROR, "error opening of disk = %s \n", ebdr_disk_src_obj[i].name);
			stop_work("Disk Open failed");
		}

		restart_disk(pid);
	}
}


int recovery_conn_client(pid)
{
	if(ebdr_conn_client[pid].obj_state == CONN_OBJ_PAUSE)
	{
		//pthread_cancel(resync_tid[pid]);
		restart_conn_client(pid);
	}
}


int recovery_conn_server(pid)
{
	if(ebdr_conn_server[pid].obj_state == CONN_OBJ_PAUSE)
	{
		//pthread_cancel(send_recv_tid);
		restart_conn_server(pid);
	}
}


void *recovery_thread_init(void * args)
{
	int pid; 
	printf("In recovery thread \n");

  log_pid = MAX_DISKS+CNTRL_THREADS - 3;

  ebdr_log_setup(log_pid, RECOVERY_LOG);

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
#endif
		}
		sleep(10); 
	}
}


