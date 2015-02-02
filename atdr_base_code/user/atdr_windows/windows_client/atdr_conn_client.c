#include <stdio.h>

#include "..\Include\atdr_conn.h"
#include "..\Include\atdr_log.h"
#include "..\Include\atdr_replication_header.h"
#include "..\Include\replic_client.h"

#define SRV_IP_ADDR "127.0.0.1"

struct sockaddr_in srvAddr; /*make it local eventually */

cmdFun_t ccmdMap[] =
{
	{ PROTO_TYPE, send_hdr },
	{ DATA_TYPE, send_data },
	{ DATA, send_data_start },
	{ DATA_REQ, send_hdr },
};

cmdFun_t rcmdMap[] =
{
	{ PROTO_TYPE, recv_hdr },
	{ DATA_TYPE, recv_data },
	{ DATA_PREP, recv_data_resp },
};

static int atdr_conn_client_start(atdr_conn_t *atdr_conn, int client_pid)
{
	if (connect(atdr_conn_client[client_pid].sockFd, (struct sockaddr *)&srvAddr, sizeof(srvAddr)) < 0)
	{
		stop_work("[conn_client_start] Can't connect to server");
	}
	else {
		atdr_log(ATDR_INFO, "connected to the server\n");
	}
	atdr_conn_client[client_pid].atdr_conn_state = ATDR_CONN_CONNECTED;

	//cc_count++;
	return 0;
}


static void atdr_conn_client_init(int client_pid)
{
	/* Assign client connection operations */
	atdr_conn_client[client_pid].conn_ops = &client_conn_ops;
}

static int atdr_conn_client_setup(atdr_conn_t *atdr_conn, char *ip_addr, int client_pid)
{
	int sockFd = 0;
	struct timeval timeout;

	atdr_log(ATDR_INFO, "[client_setup] client_pid = %d\n", client_pid);
	atdr_conn_client[client_pid].atdr_conn_mode = ATDR_CONN_CLIENT;
	atdr_conn_client[client_pid].atdr_conn_state = ATDR_CONN_SETUP;
	atdr_conn_client[client_pid].obj_state = CONN_OBJ_IN_USE;

	if ((sockFd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		stop_work("Can't open soc_stream socket\n");
	}

	/* Set the timeout to 30 seconds. */
	timeout.tv_sec = 30;
	timeout.tv_usec = 0;

	if (setsockopt(sockFd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0)
	{
		atdr_log(ATDR_ERROR, "Could not set recv timeout\n");
	}


	if (setsockopt(sockFd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != 0)
	{
		atdr_log(ATDR_ERROR, "Could not set send timeout\n");
	}

	/* Set the option active */
	int optval = 1;
	int optlen = sizeof(optval);
	if (setsockopt(sockFd, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0) {
		atdr_log(ATDR_ERROR, "Could not set keepalive timeout\n");
	}


	atdr_log(ATDR_INFO, "client socket creation successful \n");
	memset(&srvAddr, 0, sizeof(srvAddr));
	srvAddr.sin_family = AF_INET;
	srvAddr.sin_addr.s_addr = inet_addr(ip_addr);
	srvAddr.sin_port = htons(SRV_TCP_PORT);

	atdr_conn_client[client_pid].sockFd = sockFd;
	atdr_log(ATDR_INFO, "[Client_setup] client sockfd: %d\n", atdr_conn_client[client_pid].sockFd);
	atdr_conn_client[client_pid].conn_ops->do_atdr_conn_start(&atdr_conn_client[client_pid], client_pid); /* error handling ?? */
	return sockFd;
}



int atdr_conn_client_send(void *buf, int size, SOCKET sockfd, enum send_rcv_type_t send_recv_type)
{
	replic_header *rep_hdr_rcvd = NULL;
	int i;
	

	for (i = 0; i<(sizeof(ccmdMap) / sizeof(cmdFun_t)); i++)
	{
		if (send_recv_type == ccmdMap[i].opcode)
			return ccmdMap[i].fun(buf, size, sockfd, rep_hdr_rcvd);
	}
	return 0;
}

static int atdr_conn_client_recv(void *buf, int size, SOCKET sockfd, enum send_rcv_type_t send_recv_type, int pid)
{
	int i; 
	static replic_header *rep_hdr_rcvd;

	for (i = 0; i < (sizeof(rcmdMap) / sizeof(cmdFun_t)); i++)
	{
		if (send_recv_type == rcmdMap[i].opcode)
			rcmdMap[i].fun(buf, size, sockfd, rep_hdr_rcvd);
	}
	return 0;
}



struct atdr_conn_operations client_conn_ops =
{
	.do_atdr_conn_init = atdr_conn_client_init,
	.do_atdr_conn_setup = atdr_conn_client_setup,
	.do_atdr_conn_start = atdr_conn_client_start,
	.do_atdr_conn_send = atdr_conn_client_send,
	.do_atdr_conn_recv = atdr_conn_client_recv
};

