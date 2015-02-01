#include "sys_common.h"
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
//extern int link_down;
int total_conn = 0;
int client_connected = -1;
extern int recovery_mode[MAX_PID];
int sock_recovery[MAX_CONN] = {0};
int partner_created[MAX_PID] = {0};

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


pthread_t replic_tid;
pthread_attr_t attr;

pthread_t send_recv_tid[MAX_PID];
pthread_attr_t srattr[MAX_PID];

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

	while(1)
	{
		if(send_recv == PROTO_TYPE)
		{	
			rep_hdr_rcvd = (replic_header *)buf;
			n = recv(sockfd, rep_hdr_rcvd, sizeof(replic_header), MSG_WAITALL);
			if(n < 0)
			{
				ebdr_log(ERROR, "stopped receiving for sockfd:[%d] n = %d opcode:%d errno = %d\n",
					   	sockfd, n, rep_hdr_rcvd->opcode, errno);
				
				if((rep_hdr_rcvd->opcode == REMOVE_PARTNER) && (n < 0))
				{
					ebdr_log(ERROR, "sockfd[%d] already closed by remove partner \n", sockfd);
					return 1;
				}
			
				if((n < 0) && (rep_hdr_rcvd->opcode == DATA))
				{
					/* make conn server object as paused */
					ebdr_log(INFO, "[conn_server_recv] recv'd -1 PAUSING conn server object[%d]...\n", rep_hdr_rcvd->partner_id);
#if 0
					int num_rows = mkpartner_from_db_on_server();
					int i;
					printf("clearing total %d partner_created objects...\n", num_rows);
					for(i = 0;i < num_rows; i++)
					{
						partner_created[i] = 0;
					}
#endif

					partner_created[rep_hdr_rcvd->partner_id] = 0;
					shutdown(ebdr_conn_server[rep_hdr_rcvd->partner_id].conn_fd, SHUT_RDWR);
					close(ebdr_conn_server[rep_hdr_rcvd->partner_id].conn_fd);
					ebdr_conn_server[rep_hdr_rcvd->partner_id].conn_fd = -1;
					ebdr_log(INFO, "[conn_server_recv] pid = %d  conn_fd = %d\n going to sleep for 1 min...\n", 
							rep_hdr_rcvd->partner_id, ebdr_conn_server[rep_hdr_rcvd->partner_id].conn_fd);
					sleep (60);
					ebdr_conn_server[rep_hdr_rcvd->partner_id].obj_state = CONN_OBJ_PAUSE;
					sock_recovery[rep_hdr_rcvd->partner_id] = 1;
					ebdr_log(INFO, "[conn_server_recv] ==== cancelling send_recv_thread for pid %d =====\n",
							rep_hdr_rcvd->partner_id);
					pthread_cancel(send_recv_tid[rep_hdr_rcvd->partner_id]);
					partner_created[rep_hdr_rcvd->partner_id] = 0;
					return 1;//Abandon the send-recv process
				}	
				
				if(n <= 0)
				{
					return 1;
				}
				stop_work("recv error");
			}

			for(i = 0; i < (sizeof(cmdMap)/sizeof(cmdFun_t)); i++)
			{
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

	sockfd_pid_var = *(struct sockfd_pid *) ptr;
	//sock_fd = ((struct sockfd_pid *)ptr)->sockfd;
	//server_pid = ((struct sockfd_pid *)ptr)->pid;

	sock_fd = sockfd_pid_var.sockfd;
	server_pid = sockfd_pid_var.pid;

	log_pid = MAX_DISKS + CNTRL_THREADS - 2;
	ebdr_log_setup(log_pid,  SEND_RCV_LOG);

	ebdr_log(INFO, "[send_recv_handler] Thread for client pid: %d sock_fd=%d\n", server_pid, sock_fd);

	ebdr_connection_init(EBDR_CONN_SERVER, server_pid);
	ebdr_conn_server[server_pid].conn_fd = sock_fd;
	ebdr_conn_server[server_pid].ebdr_conn_state = EBDR_CONN_CONNECTED;
#if 0
	if(link_down == 0)
	{
		printf("[server_send_recv_handler] LINK IS UP !!\n");
		printf("[server_send_recv_handler] assigning new server sockfd to all_partner_server sockfd\n");
		all_partner_servers[server_pid].socket_fd = ebdr_conn_server[server_pid].conn_fd;
		printf("[server_send_recv_handler] new server sockfd: %d\n", ebdr_conn_server[server_pid].conn_fd);
	}
#endif

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
	int pid = 0, num_rows = 0;
	static struct sockfd_pid sockfd_pid_var[MAX_CONN];
	struct timeval timeout;

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

			/* Set the timeout to 30 seconds. */
			timeout.tv_sec = 30;
			timeout.tv_usec = 0;
			if (setsockopt(newSockFd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != 0)
			{
				ebdr_log(ERROR, "Could not set send timeout\n");
			}

			/* Set the option active */
			int optval = 1;
			int optlen = sizeof(optval);
			if(setsockopt(newSockFd, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0) {
				ebdr_log(ERROR, "Could not set keepalive timeout\n");
			}   

#if 0	
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
#endif
			ebdr_conn_server[MAX_CONN + 1].conn_fd = newSockFd;

			num_rows = mkpartner_from_db_on_server();
			if(num_rows <= 0)
			{
				printf("[replic_thread_init] No Partnership so far...creating first pid = 0\n");
				pid = 0;
				partner_created[pid] = 1;
				sockfd_pid_var[pid].pid = pid;
			}
			else
			{
				for(pid = 0; pid <= num_rows; pid++)
				{
					if((ebdr_conn_server[pid].obj_state == CONN_OBJ_PAUSE) && (sock_recovery[pid] == 1))
					{
						ebdr_log(INFO, "[replic_thread_init] <<<<<< Changing conn server obj state to RESUME <<<<<<\n");
						memcpy(&ebdr_conn_server[pid], &ebdr_conn_server[server_pid], sizeof(ebdr_conn_t));
						all_partner_servers[pid].socket_fd = newSockFd;
						ebdr_conn_server[pid].obj_state = CONN_OBJ_RESUME;
						sockfd_pid_var[pid].pid = pid;
						sock_recovery[pid] = 0;
						printf("[replic_thread_init] after conn obj resume pid = %d\n", pid);
						total_conn++;
						break;
					}

					/* multi */	
					if((client_connected == 1) && (recovery_mode[pid] == 1) && 
							(all_partner_servers[pid].obj_state == PARTNER_OBJ_IN_USE))
					{
						memcpy(&ebdr_conn_server[pid], &ebdr_conn_server[server_pid], sizeof(ebdr_conn_t));
						all_partner_servers[pid].socket_fd = newSockFd;
						sockfd_pid_var[pid].pid = pid;
						printf("[replic_thread_init] sockfd_pid_var[%d].pid = %d\n", pid, sockfd_pid_var[pid].pid);
						printf("[replic_thread_init] conn obj state = %d\n", ebdr_conn_server[pid].obj_state);
						all_partner_servers[pid].obj_state = PARTNER_OBJ_RECOVERY; /* multi */
						client_connected = -1;
						total_conn++;
						break;
					}

					if(total_conn >= num_rows)
					{
						printf("[replic_thread_init] total_conn = %d\n", total_conn);
						break;
					}
					printf("[replic_thread_init] checking partner_created[%d] = %d\n", pid, partner_created[pid]);
					if(partner_created[pid] == 0)
					{
						printf("[replic_thread_init] partner create for pid %d is zero \n", pid);
						partner_created[pid] = 1;
						total_conn++;
						break;
					}
				}
			}
#if 0
			if(partner_created[num_rows] == 0)
			{
				printf("[replic_thread_init] creating partnership for pid %d \n", num_rows);
				partner_created[num_rows] = 1;
				pid = num_rows;
			}
#endif
			/* Create resync thread for each client */
			ret = pthread_attr_init(&srattr[pid]);
			if(ret != 0)
			{
				ebdr_log(INFO, "Error initalizing attributes\n");
				stop_work("pthread_attr_init failed ");
			}

			pthread_attr_setdetachstate(&srattr[pid], PTHREAD_CREATE_DETACHED);
			sockfd_pid_var[pid].sockfd = newSockFd;

			printf("[replic_thread_init] Creating send recv handler thread for pid %d ...\n", pid);
			if(pthread_create( &send_recv_tid[pid], &srattr[pid],  send_recv_handler, &sockfd_pid_var[pid]) < 0)
			{
				perror("could not create send_recv thread\n");
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

	pthread_t conn_thread;

	ebdr_conn_server[server_pid].ebdr_conn_mode = EBDR_CONN_SERVER;
	ebdr_conn_server[server_pid].ebdr_conn_state = EBDR_CONN_SETUP;
	ebdr_conn_server[server_pid].obj_state = CONN_OBJ_IN_USE;

	if((sockFd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		stop_work("Can't open soc_stream socket\n");
	}
	ebdr_log(INFO, "server socket creation successful \n");

	
	int yes = 1;
	if (setsockopt(sockFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1 )
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

