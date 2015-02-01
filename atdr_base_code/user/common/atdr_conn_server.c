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

#include "ebdr_conn.h"
#include "ebdr_log.h"
#include "ebdr_disk.h"
#include "ebdr_replication_header.h"
#include "ebdr_replication.h"
#include "ebdr_bitmap_user.h"
#include "ebdr_grain_req.h"
#include "ebdr_io_engine.h"

#include "ebdr_partner.h"
#include "replic_server.h"

//extern FILE *ebdrlog_fp;
extern int link_down;
int client_connected = -1;
extern int recovery_mode;
int sock_recovery[MAX_CONN] = {0};

server_send_cmdFun_t server_send_cmdMap[] =
{
	{ PROTO_TYPE, server_send_hdr },
	{ DATA_TYPE,  server_send_data },
};

cmdFun_t cmdMap[] =
{
	{BITMAP_METADATA, get_bitmap},
	{DATA, get_data},
	{MAKE_PARTNER, make_partner},
	{REMOVE_PARTNER, remove_partner},
	{RM_RELATION, remove_relation},
	{SYN, syn_received},
	{ACK, ack},
	{RPLC_TERMINATION, terminate},
	{DATA_REQ, data_req},
	{VERIFY_DATA, verify_data},
};


THREAD_ID_T replic_tid;
pthread_attr_t attr;

THREAD_ID_T send_recv_tid;
pthread_attr_t srattr;

void stop_work(char str[100])
{
    ebdr_log(ERROR, " %s Error occured..Stopped !! errno = %d\n", str, errno);
    exit(0); /* return a meaningful value*/
}

static void ebdr_conn_server_cleanup(ebdr_conn_t *ebdr_conn)
{
	ebdr_log(INFO, "ebdr_conn_cleanup\n");
}


static void ebdr_conn_server_stop(ebdr_conn_t *ebdr_conn, int server_pid)
{
	ebdr_log(INFO, "ebdr_conn_stop\n");
	ebdr_conn_server[server_pid].ebdr_conn_state = EBDR_CONN_RELEASED;	
}

int ebdr_conn_server_send(void *buf, int size, int sockfd, enum send_rcv_type_t send_recv_type)
{
	replic_header *rep_hdr_rcvd;
	struct ebdr_user_bitmap *usr_bitmap_rcvd;
    int i, ret;

    for(i = 0; i < (sizeof(server_send_cmdMap)/sizeof(server_send_cmdFun_t)); i++)
    {
	    if(send_recv_type == server_send_cmdMap[i].opcode )
	    {
		    return server_send_cmdMap[i].server_send_fun(buf, size, sockfd, rep_hdr_rcvd );
	    }
    }
 	return 0;
}

static int ebdr_conn_server_recv(void *buf, int size, int sockfd, enum send_rcv_type_t send_recv, int pid)
{
	char rbuff[BUF_SIZE_IN_BYTES];
	char partner_ip[64];
	int i, n, newSockFd ;
	unsigned long int start_lba;
	unsigned long int grain_index = 0, chunk_index =0;

	replic_header *rep_hdr_rcvd;

	/* init replication object....maybe this will change later */

	while(1)
	{
		//printf("Send_recv = %d \n", send_recv);
		if(send_recv == PROTO_TYPE)
		{	
			rep_hdr_rcvd = (replic_header *)buf;
			//printf("rep_hdr_rcvd->opcode = %d \n", rep_hdr_rcvd->opcode);
			n = recv(sockfd, rep_hdr_rcvd, sizeof(replic_header), MSG_WAITALL);
			if(n < 0)
			{
				ebdr_log(ERROR, "stopped receiving for sockfd:[%d] n = %d opcode:%d\n", sockfd, n, rep_hdr_rcvd->opcode);
				if((rep_hdr_rcvd->opcode == REMOVE_PARTNER) && (n < 0))
				{
					ebdr_log(ERROR, "sockfd[%d] already closed by remove partner \n", sockfd);
					return 1;
				}
#if 1	
				if((n < 0) && (rep_hdr_rcvd->opcode == DATA))
				{
					/* make conn server object as paused */
					ebdr_log(INFO, "[conn_server_recv] recv'd -1 PAUSING conn server object...\n");
					shutdown(ebdr_conn_server[rep_hdr_rcvd->partner_id].conn_fd, SHUT_RDWR);
					close(ebdr_conn_server[rep_hdr_rcvd->partner_id].conn_fd);
					ebdr_conn_server[rep_hdr_rcvd->partner_id].conn_fd = -1;
					sleep (30);
					ebdr_conn_server[rep_hdr_rcvd->partner_id].obj_state = CONN_OBJ_PAUSE;
					sock_recovery[rep_hdr_rcvd->partner_id] = 1;
					ebdr_log(INFO, "[conn_server_recv] ==== closing send_recv_thread =====\n");
					return 1;//Abandon the send-recv process
				}	
#endif
				if(n <= 0)
				{
					return 1;
				}
				stop_work("recv error");
			}

			for(i = 0; i < (sizeof(cmdMap)/sizeof(cmdFun_t)); i++)
			{
				//printf("opcpde = %d \n", cmdMap[i].opcode);
				if(rep_hdr_rcvd->opcode == cmdMap[i].opcode )
					cmdMap[i].fun(sockfd, rep_hdr_rcvd );
			}
		}
	}
	return 0;
}

void *send_recv_handler(void *ptr)
{
	struct sockfd_pid sockfd_pid_var;
	int server_pid;
	int sock_fd;
	replic_header rep_hdr_rcvd;

	sock_fd = ((struct sockfd_pid *)ptr)->sockfd;
	server_pid = ((struct sockfd_pid *)ptr)->pid;

	log_pid = MAX_DISKS + CNTRL_THREADS - 2;
	ebdr_log_setup(log_pid,  SEND_RCV_LOG);

	ebdr_log(INFO, "[send_recv_handler] Thread for client pid: %d sock_fd=%d\n", server_pid, sock_fd);

	ebdr_connection_init(EBDR_CONN_SERVER, server_pid);
	// ebdr_conn_server[server_pid].conn_ops->do_ebdr_conn_setup(&ebdr_conn_server[server_pid], NULL, server_pid);

	/*
	   ebdrlog_fp = fopen(SEND_RCV_LOG, "a+");

	   ebdr_log(INFO, "[send_recv_handler] Thread for client fd: %d\n", *(int *)newsockfd);
	   ebdr_conn_server[cs_count].conn_fd = *(int *)newsockfd;
	   ebdr_conn_server[cs_count].ebdr_conn_state = EBDR_CONN_CONNECTED;
	 */
	ebdr_conn_server[server_pid].conn_fd = sock_fd;
	ebdr_conn_server[server_pid].ebdr_conn_state = EBDR_CONN_CONNECTED;
#if 0
	if((client_connected == 1) && (recovery_mode == 1))
	{
		printf("[send_recv_handler] client_connected = %d \n", client_connected);
		all_partner_servers[server_pid].socket_fd = ebdr_conn_server[server_pid].conn_fd;
		client_connected = -1;
	}
#endif

	if(link_down == 0)
	{
		printf("[server_send_recv_handler] LINK IS UP !!\n");
		printf("[server_send_recv_handler] assigning new server sockfd to all_partner_server sockfd\n");
		all_partner_servers[server_pid].socket_fd = ebdr_conn_server[server_pid].conn_fd;
		printf("[server_send_recv_handler] new server sockfd: %d\n", ebdr_conn_server[server_pid].conn_fd);
	}

	ebdr_conn_server[server_pid].conn_ops->do_ebdr_conn_recv(&rep_hdr_rcvd,
			sizeof(replic_header), ebdr_conn_server[server_pid].conn_fd, PROTO_TYPE, server_pid);
}

void *replic_thread_init(void *args)
{
	struct sockaddr_in cliAddr;
	int newSockFd = 0, cliLen, ret = 0;
	unsigned long int size = 0; // assign appropriate value
	replic_header rep_hdr_rcvd;
	int server_pid = MAX_CONN + 1;
	int pid = 0;
	static struct sockfd_pid sockfd_pid_var;

	log_pid = MAX_DISKS + CNTRL_THREADS - 2;

	ebdr_log_setup(log_pid, REPLIC_THREAD_LOG);

	ebdr_log(INFO, "Server waiting for new connection: \n");
	while(1)
	{
		ebdr_conn_server[server_pid].ebdr_conn_state = EBDR_CONN_WAIT_FOR_CLIENT;
		cliLen = sizeof(cliAddr);

		ebdr_log(INFO, "Server before accept server_pid =%d newSockFd =%d\n", server_pid, newSockFd);
		newSockFd = accept(ebdr_conn_server[server_pid].sockFd, (struct sockaddr *) &cliAddr, &cliLen);
		if(newSockFd < 0)
		{
			ebdr_log(INFO, "[accept] newsockfd = %d listenfd[%d] : [%d]\n", newSockFd, server_pid, 
					ebdr_conn_server[server_pid].sockFd);
			stop_work("accept Error\n");
		}
		else
		{
			ebdr_log(INFO, "Connected to client: %s[%d]\n", inet_ntoa(cliAddr.sin_addr), cliAddr.sin_port);

			ebdr_log(INFO, "[accept] server_pid = %d\n", server_pid);
			ebdr_log(INFO, "[accept] newsockfd = %d listenfd[%d]: %d\n", newSockFd, server_pid, ebdr_conn_server[server_pid].sockFd);

			client_connected = 1;

			/* Create resync thread for each client */
			ret = pthread_attr_init(&srattr);
			if(ret != 0)
			{
				ebdr_log(INFO, "Error initalizing attributes\n");
				stop_work("pthread_attr_init failed ");
			}

			pthread_attr_setdetachstate(&srattr, PTHREAD_CREATE_DETACHED);

			sockfd_pid_var.pid = MAX_CONN + 1;
			sockfd_pid_var.sockfd = newSockFd;

			ebdr_conn_server[MAX_CONN + 1].conn_fd = newSockFd;

			for(pid = 0; pid < MAX_CONN; pid++)
			{
				if((ebdr_conn_server[pid].obj_state == CONN_OBJ_PAUSE) && (sock_recovery[pid] == 1))
				{
					ebdr_log(INFO, "[replic_thread_init] <<<<<< Changing conn server obj state to RESUME <<<<<<\n");
					memcpy(&ebdr_conn_server[pid], &ebdr_conn_server[server_pid], sizeof(ebdr_conn_t));
					all_partner_servers[pid].socket_fd = newSockFd;
					ebdr_conn_server[pid].obj_state = CONN_OBJ_RESUME;
					sockfd_pid_var.pid = pid;
					sock_recovery[pid] = 0;
					break;
				}
				
				if((client_connected == 1) && (recovery_mode == 1))
				{
					memcpy(&ebdr_conn_server[pid], &ebdr_conn_server[server_pid], sizeof(ebdr_conn_t));
					all_partner_servers[pid].socket_fd = newSockFd;
					sockfd_pid_var.pid = pid;
					client_connected = -1;
					break;
				}
			}

			if(pthread_create( &send_recv_tid, &srattr,  send_recv_handler, &sockfd_pid_var) < 0)
			{
				perror("could not create send_recv thread");
				stop_work("pthread_create failed");
			}
			// server_pid++;
		}
	}
	return 0;
}

static void create_replication_thread(void)
{
	int ret = 0;
	ret = pthread_attr_init(&attr);
	if(ret != 0)
	{
		ebdr_log(ERROR,  "Error initalizing attributes\n");
		stop_work("pthread_attr_init in create_replication_thread failed ");
	}

	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
	ret = pthread_create(&replic_tid, &attr, replic_thread_init, NULL);

	if(ret != 0)
	{
		ebdr_log(ERROR, "Error creating thread\n");
		stop_work("pthread_create in create_replication_thread failed ");
	}
	ebdr_log(INFO, "\n Replication thread created.\n");
}


static int ebdr_conn_server_setup(ebdr_conn_t *ebdr_conn, char *ip_addr, int server_pid)
{
	int sockFd = 0, newSockFd = 0, ret;
	int on=1;
	struct linger so_linger; 
	struct sockaddr_in srvAddr;

	THREAD_ID_T conn_thread;

	ebdr_conn_server[server_pid].ebdr_conn_mode = EBDR_CONN_SERVER;
	ebdr_conn_server[server_pid].ebdr_conn_state = EBDR_CONN_SETUP;
	ebdr_conn_server[server_pid].obj_state = CONN_OBJ_IN_USE;

	if((sockFd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		stop_work("Can't open soc_stream socket\n");
	}
	ebdr_log(INFO, "server socket creation successful \n");

	
	int yes = 1;
	if ( setsockopt(sockFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1 )
	{
		ebdr_log(INFO, "setsockopt error\n");
	}

	so_linger.l_onoff = 1; /* 1 = TRUE*/
	so_linger.l_linger = 0;
	ret = setsockopt(sockFd, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger));

	memset(&srvAddr, 0, sizeof(srvAddr));
	srvAddr.sin_family = AF_INET;
	srvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	srvAddr.sin_port = htons(SRV_TCP_PORT);
	ebdr_conn_server[server_pid].sockFd = sockFd; 
	if(bind(ebdr_conn_server[server_pid].sockFd, (struct sockaddr *)&srvAddr, sizeof(srvAddr)) < 0)
	{
		stop_work("Can't bind local address");
		goto bind_err;
	}

	if (listen(ebdr_conn_server[server_pid].sockFd, MAX_CONN) < 0 )
	{
		stop_work("do_ebdr_conn_setup: Listen");
		goto listen_err;
	}
	ebdr_log(INFO, "[server_setup] listen fd: %d \n", ebdr_conn_server[server_pid].sockFd);
	create_replication_thread();
	return sockFd;
listen_err:
bind_err:
	close(sockFd);
	return sockFd;
}

static void ebdr_conn_server_init(int server_pid)
{
	/* Assign server ops */
	ebdr_conn_server[server_pid].conn_ops = &server_conn_ops;
}

struct ebdr_conn_operations server_conn_ops = 
{
	.do_ebdr_conn_init 		= ebdr_conn_server_init,
	.do_ebdr_conn_setup 	= ebdr_conn_server_setup,
	.do_ebdr_conn_send 		= ebdr_conn_server_send,
	.do_ebdr_conn_recv 		= ebdr_conn_server_recv,
	.do_ebdr_conn_stop 		= ebdr_conn_server_stop,
	.do_ebdr_conn_cleanup 	= ebdr_conn_server_cleanup
};

