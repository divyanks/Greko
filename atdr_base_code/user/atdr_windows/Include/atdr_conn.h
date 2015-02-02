#ifndef ATDR_CONN_H
#define ATDR_CONN_H
#include <Windows.h>

#include "utils.h"
#define MAX_CONN 2048
#define SRV_TCP_PORT 6660

extern  char *log_path;
#define SEND_RCV_LOG strcat(log_path, "send_recv_log.txt")
#define REPLIC_THREAD_LOG  strcat(log_path, "replic_log.txt")

enum atdr_conn_obj_state_t
{
	CONN_OBJ_IN_USE,
	CONN_OBJ_PAUSE,
	CONN_OBJ_RESUME,
	CONN_OBJ_RELEASED
};

enum send_rcv_type_t
{
	PROTO_TYPE,
	DATA_TYPE,
};

enum atdr_conn_state_t
{
	ATDR_CONN_START,
	ATDR_CONN_SETUP,
	ATDR_CONN_WAIT_FOR_CLIENT,
	ATDR_CONN_CONNECTING,
	ATDR_CONN_CONNECTED,
	ATDR_CONN_RELEASED,
	ATDR_CONN_FAILED,
	ATDR_CONN_DISCONNECTED,
};

enum atdr_conn_mode_t
{
	ATDR_CONN_SERVER,
	ATDR_CONN_CLIENT
};

enum atdr_conn_type_t
{
	ATDR_ASYNC,
	ATDR_SYNC
};

struct atdr_connection;

struct atdr_conn_operations
{
	void (* do_atdr_conn_init)(int pid);
	int (* do_atdr_conn_setup)(struct atdr_connection *conn_req, char *ip_addr, int pid);
	int (* do_atdr_conn_start) (struct atdr_connection *conn_req, int pid);
	int (* do_atdr_conn_send) (void *buf, int size, SOCKET sockfd, enum send_rcv_type_t send_type);
	int (* do_atdr_conn_recv) (void *buf, int size, SOCKET sockfd, enum send_rcv_type_t send_type, int pid);
	void (* do_atdr_conn_stop) (struct atdr_connection *conn_req, int pid);
	void (*do_atdr_conn_cleanup)(struct atdr_connection *conn_req);
};

extern struct atdr_conn_operations server_conn_ops;
extern struct atdr_conn_operations client_conn_ops;


struct atdr_connection
{
	char src_ip[64];
	char dest_ip[64];
	int src_port;
	int conn_fd;
	SOCKET sockFd;//listenFd
	struct atdr_conn_operations *conn_ops;
	enum atdr_conn_state_t atdr_conn_state;
	enum atdr_conn_type_t atdr_conn_type;
	enum atdr_conn_obj_state_t obj_state;
	int atdr_conn_mode;
};

struct sockfd_pid
{
  int sockfd;
  int pid;
};

typedef struct atdr_connection atdr_conn_t;
atdr_conn_t atdr_connect;
atdr_conn_t atdr_conn_server[MAX_CONN + 1];
atdr_conn_t atdr_conn_client[MAX_CONN];

void atdr_connection_init(enum atdr_conn_mode_t conn_mode, int pid);
static void atdr_conn_server_init(int server_pid);
#endif 
