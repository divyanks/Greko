#include <stdio.h>
#include <WinSock2.h>
#include<io.h>
#include<stdio.h>
#include<winsock2.h>

#include "..\Include\atdr_partner.h"

#pragma comment(lib,"ws2_32.lib") //Winsock Library


#include "..\Include\atdr_conn.h"
#include "..\Include\atdr_log.h"
#include "..\Include\atdr_replication.h"
#include "..\Include\replic_server.h"

int total_conn = 0;
int partner_created[MAX_PID] = { 0 };

DWORD replic_tid;
// DWORD send_recv_tid;
int client_connected = -1;
int sock_recovery[MAX_CONN] = { 0 };

server_send_cmdFun_t server_send_cmdMap[] =
{
	{ PROTO_TYPE, server_send_hdr },
	{ DATA_TYPE, server_send_data },
};

cmdFun_t cmdMap[] =
{
	{ BITMAP_METADATA, get_bitmap },
	{ DATA, get_data },
	{ MAKE_PARTNER, make_partner },
	{ REMOVE_PARTNER, remove_partner },
	{ RM_RELATION, remove_relation },
	{ SYN, syn_received },
	{ ACK, ack },
	{ RPLC_TERMINATION, terminate },
	{ DATA_REQ, data_req },
	{ VERIFY_DATA, verify_data },
};


HANDLE send_recv_tid[MAX_PID];


static void atdr_conn_server_init(int server_pid)
{
	/* Assign server ops */
	atdr_conn_server[server_pid].conn_ops = &server_conn_ops;
}

void *send_recv_handler(void *ptr)
{
	int server_pid;
	int sock_fd;
	replic_header rep_hdr_rcvd;

	sock_fd = ((struct sockfd_pid *)ptr)->sockfd;
	server_pid = ((struct sockfd_pid *)ptr)->pid;

	log_pid = MAX_DISKS + CNTRL_THREADS - 2;
	atdr_log_setup(log_pid, SEND_RCV_LOG);

	atdr_log(ATDR_INFO, "[send_recv_handler] Thread for client pid: %d sock_fd=%d\n", server_pid, sock_fd);

	atdr_connection_init(ATDR_CONN_SERVER, server_pid);
	// atdr_conn_server[server_pid].conn_ops->do_atdr_conn_setup(&atdr_conn_server[server_pid], NULL, server_pid);

	/*
	atdrlog_fp = fopen(SEND_RCV_LOG, "a+");

	atdr_log(INFO, "[send_recv_handler] Thread for client fd: %d\n", *(int *)newsockfd);
	atdr_conn_server[cs_count].conn_fd = *(int *)newsockfd;
	atdr_conn_server[cs_count].atdr_conn_state = ATDR_CONN_CONNECTED;
	*/
	atdr_conn_server[server_pid].conn_fd = sock_fd;
	atdr_conn_server[server_pid].atdr_conn_state = ATDR_CONN_CONNECTED;

	atdr_conn_server[server_pid].conn_ops->do_atdr_conn_recv(&rep_hdr_rcvd,
		sizeof(replic_header), atdr_conn_server[server_pid].conn_fd, PROTO_TYPE, server_pid);
	return 0;
}

extern HANDLE fifo_fd;

void *replic_thread_init(void *args)
{
	struct sockaddr_in cliAddr;
	SOCKET newSockFd;
	int cliLen, ret = 0;
	unsigned long int size = 0; // assign appropriate value
	int server_pid = MAX_CONN + 1;
	int pid = 0, num_rows = 0;
	static struct sockfd_pid sockfd_pid_var[MAX_CONN];
	struct timeval timeout;


	log_pid = MAX_DISKS + CNTRL_THREADS - 2;

	atdr_log_setup(log_pid, REPLIC_THREAD_LOG);

	atdr_log(ATDR_INFO, "Server waiting for new connection: \n");
	while (1)
	{
		atdr_conn_server[server_pid].atdr_conn_state = ATDR_CONN_WAIT_FOR_CLIENT;
		cliLen = sizeof(cliAddr);

		newSockFd = accept(atdr_conn_server[server_pid].sockFd, (struct sockaddr *) &cliAddr, &cliLen);
		if (newSockFd < 0)
		{
			atdr_log(ATDR_INFO, "[accept] newsockfd = %d listenfd[%d] : [%d]\n", newSockFd, server_pid,
				atdr_conn_server[server_pid].sockFd);
			stop_work("accept Error\n");
		}
		else
		{
			atdr_log(ATDR_INFO, "Connected to client: %s[%d]\n", inet_ntoa(cliAddr.sin_addr), cliAddr.sin_port);

			atdr_log(ATDR_INFO, "[accept] server_pid = %d\n", server_pid);
			atdr_log(ATDR_INFO, "[accept] newsockfd = %d listenfd[%d]: %d\n", newSockFd, server_pid, atdr_conn_server[server_pid].sockFd);

			client_connected = 1;
			/* Set the timeout to 30 seconds. */
			timeout.tv_sec = 30;
			timeout.tv_usec = 0;
			if (setsockopt(newSockFd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != 0)
			{
				atdr_log(ERROR, "Could not set send timeout\n");
			}

			/* Set the option active */
			int optval = 1;
			int optlen = sizeof(optval);
			if (setsockopt(newSockFd, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0) {
				atdr_log(ERROR, "Could not set keepalive timeout\n");
			}

			atdr_conn_server[MAX_CONN + 1].conn_fd = newSockFd;


			num_rows = mkpartner_from_db_on_server();
			if(num_rows <= 0)
			{
				// printf("[replic_thread_init] No Partnership so far...creating first pid = 0\n");
				pid = 0;
				partner_created[pid] = 1;
				sockfd_pid_var[pid].pid = pid;
			}
			else
			{
				for(pid = 0; pid <= num_rows; pid++)
				{
					if((atdr_conn_server[pid].obj_state == CONN_OBJ_PAUSE) && (sock_recovery[pid] == 1))
					{
						atdr_log(ATDR_INFO, "[replic_thread_init] <<<<<< Changing conn server obj state to RESUME <<<<<<\n");
						memcpy(&atdr_conn_server[pid], &atdr_conn_server[server_pid], sizeof(atdr_conn_t));
						all_partner_servers[pid].socket_fd = newSockFd;
						atdr_conn_server[pid].obj_state = CONN_OBJ_RESUME;
						sockfd_pid_var[pid].pid = pid;
						sock_recovery[pid] = 0;
						printf("[replic_thread_init] after conn obj resume pid = %d\n", pid);
						total_conn++;
						break;
					}

					/* multi */
					if ((client_connected == 1) && (recovery_mode[pid] == 1) &&
						(all_partner_servers[pid].obj_state == PARTNER_OBJ_IN_USE))
					{
						memcpy(&atdr_conn_server[pid], &atdr_conn_server[server_pid], sizeof(atdr_conn_t));
						all_partner_servers[pid].socket_fd = newSockFd;
						sockfd_pid_var[pid].pid = pid;
						printf("[replic_thread_init] sockfd_pid_var[%d].pid = %d\n", pid, sockfd_pid_var[pid].pid);
						printf("[replic_thread_init] conn obj state = %d\n", atdr_conn_server[pid].obj_state);
			 			all_partner_servers[pid].obj_state = PARTNER_OBJ_RECOVERY; /* multi */ 
						client_connected = -1;
						total_conn++;
						break;
					}

					if (total_conn >= num_rows)
					{
						printf("[replic_thread_init] total_conn = %d\n", total_conn);
						break;
					}
					printf("[replic_thread_init] checking partner_created[%d] = %d\n", pid, partner_created[pid]);
					if (partner_created[pid] == 0)
					{
						printf("[replic_thread_init] partner create for pid %d is zero \n", pid);
						partner_created[pid] = 1;
						total_conn++;
						break;
					}
				}
			}
			sockfd_pid_var[pid].sockfd = newSockFd;

			printf("[replic_thread_init] Creating send recv handler thread for pid %d ...\n", pid);


			HANDLE hndle;
			hndle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)send_recv_handler, &sockfd_pid_var[pid], 0, &send_recv_tid);

			if (hndle == NULL)
				printf("could not create replication thread, error: %d.\n", GetLastError());
			else
				atdr_log(ATDR_INFO, "Replication thread creation successful\n");
			CloseHandle(hndle);
		}
	}
	return 0;
}



static void create_replication_thread(void)
{
	int ret = 0;
	HANDLE hndle;

	hndle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)replic_thread_init, NULL, 0, &replic_tid);
	printf("The thread ID: %d.\n", replic_tid);

	if (hndle == NULL)
		printf("could not create replication thread, error: %d.\n", GetLastError());
	else
		atdr_log(ATDR_INFO, "Replication thread creation successful");
	
	CloseHandle(hndle);
}



static void atdr_conn_server_setup(atdr_conn_t *atdr_conn, char *ip_addr, int server_pid)
{
	WSADATA wsa;
	SOCKET sockFd;
	struct sockaddr_in srvAddr;
	struct linger so_linger;
	int ret;

	atdr_conn_server[server_pid].atdr_conn_mode = ATDR_CONN_SERVER;
	atdr_conn_server[server_pid].atdr_conn_state = ATDR_CONN_SETUP;
	atdr_conn_server[server_pid].obj_state = CONN_OBJ_IN_USE;

	atdr_log(ATDR_INFO, "\nInitialising Winsock...");
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		atdr_log(ATDR_FATAL, "Failed. Error Code : %d", WSAGetLastError());
		stop_work("WSAStartup failed\n");
	}

	atdr_log(ATDR_INFO, "Initialised.\n");


	if ((sockFd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
	{
		atdr_log(ATDR_ERROR, "Could not create socket : %d", WSAGetLastError());
	}

	so_linger.l_onoff = 1; /* 1 = TRUE*/
	so_linger.l_linger = 0;
	ret = setsockopt(sockFd, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger));


	srvAddr.sin_family = AF_INET;
	srvAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	srvAddr.sin_port = htons(SRV_TCP_PORT);
	atdr_conn_server[server_pid].sockFd = sockFd;

	if (bind(atdr_conn_server[server_pid].sockFd, (struct sockaddr *)&srvAddr, sizeof(srvAddr)) == SOCKET_ERROR)
	{
		atdr_log(ATDR_FATAL, "Bind failed with error code : %d", WSAGetLastError());
		stop_work("Bind failed \n");
	}
	atdr_log(ATDR_INFO, "Bind done");

	if (listen(atdr_conn_server[server_pid].sockFd, MAX_CONN) < 0)
	{
		atdr_log(ATDR_FATAL, "do_atdr_conn_setup: Listen failed %d", WSAGetLastError());
		stop_work("Listen failed\n");
	}

	atdr_log(ATDR_INFO, "[server_setup] listen fd: %d \n", atdr_conn_server[server_pid].sockFd);
	create_replication_thread();
}

static void atdr_conn_server_cleanup(atdr_conn_t *atdr_conn)
{
	atdr_log(ATDR_INFO, "atdr_conn_cleanup\n");
}


static void atdr_conn_server_stop(atdr_conn_t *atdr_conn, int server_pid)
{
	atdr_log(ATDR_INFO, "atdr_conn_stop\n");
	atdr_conn_server[server_pid].atdr_conn_state = ATDR_CONN_RELEASED;
}

int atdr_conn_server_send(void *buf, int size, SOCKET sockfd, enum send_rcv_type_t send_recv_type)
{
	replic_header *rep_hdr_rcvd;
	int i;

	rep_hdr_rcvd = NULL;

	for (i = 0; i < (sizeof(server_send_cmdMap) / sizeof(server_send_cmdFun_t)); i++)
	{
		if (send_recv_type == server_send_cmdMap[i].opcode)
		{
			return server_send_cmdMap[i].server_send_fun(buf, size, sockfd, rep_hdr_rcvd);
		}
	}
	return 0;
}

static int atdr_conn_server_recv(void *buf, int size, SOCKET sockfd, enum send_rcv_type_t send_recv, int pid)
{
	int i, n;
	unsigned long int grain_index = 0, chunk_index = 0;

	replic_header *rep_hdr_rcvd;

	/* init replication object....maybe this will change later */

	while (1)
	{
		//printf("Send_recv = %d \n", send_recv);
		if (send_recv == PROTO_TYPE)
		{
			rep_hdr_rcvd = (replic_header *)buf;
			//printf("rep_hdr_rcvd->opcode = %d \n", rep_hdr_rcvd->opcode);
			n = recv(sockfd, (char *)rep_hdr_rcvd, sizeof(replic_header), 0);
			if (n < 0)
			{
				atdr_log(ERROR, "stopped receiving for sockfd:[%d] n = %d opcode:%d\n", sockfd, n, rep_hdr_rcvd->opcode);
				if ((rep_hdr_rcvd->opcode == REMOVE_PARTNER) && (n < 0))
				{
					atdr_log(ERROR, "sockfd[%d] already closed by remove partner \n", sockfd);
					return 1;
				}
#if 1
				if ((n < 0) && (rep_hdr_rcvd->opcode == DATA))
				{
					/* make conn server object as paused */
					atdr_log(ATDR_INFO, "[conn_server_recv] recv'd -1 PAUSING conn server object...\n");
#if 0
					int num_rows = mkpartner_from_db_on_server();
					int i;
					printf("clearing total %d partner_created objects...\n", num_rows);
					for (i = 0; i < num_rows; i++)
					{
						partner_created[i] = 0;
					}
#endif

					partner_created[rep_hdr_rcvd->partner_id] = 0;

					shutdown(atdr_conn_server[rep_hdr_rcvd->partner_id].conn_fd, SD_SEND);
					closesocket(atdr_conn_server[rep_hdr_rcvd->partner_id].conn_fd);
					atdr_conn_server[rep_hdr_rcvd->partner_id].conn_fd = -1;
					Sleep(60);
					atdr_conn_server[rep_hdr_rcvd->partner_id].obj_state = CONN_OBJ_PAUSE;
					sock_recovery[rep_hdr_rcvd->partner_id] = 1;
					atdr_log(ATDR_INFO, "[conn_server_recv] ==== closing send_recv_thread =====\n");
					partner_created[rep_hdr_rcvd->partner_id] = 0; 

					return 1;//Abandon the send-recv process
				}
#endif
				if (n <= 0)
				{
					return 1;
				}
				stop_work("recv error");
			}

			for (i = 0; i < (sizeof(cmdMap) / sizeof(cmdFun_t)); i++)
			{
				//printf("opcpde = %d \n", cmdMap[i].opcode);
				if (rep_hdr_rcvd->opcode == cmdMap[i].opcode)
					cmdMap[i].fun(sockfd, rep_hdr_rcvd);
			}
		}
	}
	return 0;
}

struct atdr_conn_operations server_conn_ops = 
{
	.do_atdr_conn_init 		= atdr_conn_server_init,
	.do_atdr_conn_setup = atdr_conn_server_setup,
	.do_atdr_conn_send = atdr_conn_server_send,
	.do_atdr_conn_recv = atdr_conn_server_recv,
	.do_atdr_conn_stop = atdr_conn_server_stop,
	.do_atdr_conn_cleanup = atdr_conn_server_cleanup
};

