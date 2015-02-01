#ifndef EBDR_CONN_H
#define EBDR_CONN_H
#include <Windows.h>

#include "utils.h"
#define MAX_CONN 2048
#define SRV_TCP_PORT 6660

extern  char *log_path;
#define SEND_RCV_LOG strcat(log_path, "send_recv_log.txt")
#define REPLIC_THREAD_LOG  strcat(log_path, "replic_log.txt")

enum ebdr_conn_obj_state_t
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

enum ebdr_conn_state_t
{
	EBDR_CONN_START,
	EBDR_CONN_SETUP,
	EBDR_CONN_WAIT_FOR_CLIENT,
	EBDR_CONN_CONNECTING,
	EBDR_CONN_CONNECTED,
	EBDR_CONN_RELEASED,
	EBDR_CONN_FAILED,
	EBDR_CONN_DISCONNECTED,
};

enum ebdr_conn_mode_t
{
	EBDR_CONN_SERVER,
	EBDR_CONN_CLIENT
};

enum ebdr_conn_type_t
{
	EBDR_ASYNC,
	EBDR_SYNC
};

struct ebdr_connection;

struct ebdr_conn_operations
{
	void (* do_ebdr_conn_init)(int pid);
	int (* do_ebdr_conn_setup)(struct ebdr_connection *conn_req, char *ip_addr, int pid);
	int (* do_ebdr_conn_start) (struct ebdr_connection *conn_req, int pid);
	int (* do_ebdr_conn_send) (void *buf, int size, SOCKET sockfd, enum send_rcv_type_t send_type);
	int (* do_ebdr_conn_recv) (void *buf, int size, SOCKET sockfd, enum send_rcv_type_t send_type, int pid);
	void (* do_ebdr_conn_stop) (struct ebdr_connection *conn_req, int pid);
	void (*do_ebdr_conn_cleanup)(struct ebdr_connection *conn_req);
};

extern struct ebdr_conn_operations server_conn_ops;
extern struct ebdr_conn_operations client_conn_ops;


struct ebdr_connection
{
	char src_ip[64];
	char dest_ip[64];
	int src_port;
	int conn_fd;
	SOCKET sockFd;//listenFd
	struct ebdr_conn_operations *conn_ops;
	enum ebdr_conn_state_t ebdr_conn_state;
	enum ebdr_conn_type_t ebdr_conn_type;
	enum ebdr_conn_obj_state_t obj_state;
	int ebdr_conn_mode;
};

struct sockfd_pid
{
  int sockfd;
  int pid;
};

typedef struct ebdr_connection ebdr_conn_t;
ebdr_conn_t ebdr_connect;
ebdr_conn_t ebdr_conn_server[MAX_CONN + 1];
ebdr_conn_t ebdr_conn_client[MAX_CONN];

void ebdr_connection_init(enum ebdr_conn_mode_t conn_mode, int pid);
static void ebdr_conn_server_init(int server_pid);
#endif 
