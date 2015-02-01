
#include "ebdr_sys_common.h"
#include "ebdr_ioctl_cmds.h"
#include "ebdr_replication_header.h"
#include "ebdr_replication.h"
#include "ebdr_fifo.h"
#include "ebdr_daemon.h"
#include "ebdr_bitmap_user.h"
#include "ebdr_disk.h"
#include "ebdr_conn.h"
#include "ebdr_partner.h"
#include "ebdr_relation.h"
#include "ebdr_io_engine.h"
#include "ebdr_log.h"
#include "ebdr_ip.h"
#include "ebdr_recovery.h"

#define LOG_PATH_LEN 128

extern int recovery_mode;
extern int ebdr_disk_init(enum role_t role, char *name, struct ebdr_disk *ebdr_disk_obj);
extern char server_ip[64];

THREAD_ONLY_VAR(FILE *ebdrlog_fp)
THREAD_ONLY_VAR(char *logStrng="temp")
THREAD_ONLY_VAR( int logging)
THREAD_ONLY_VAR(THREAD_ID_T log_pid)

char *log_path;
char tmp_log_path[LOG_PATH_LEN];

char serverfifo[] = "/tmp/serverfifo";
char clientfifo[] = "/tmp/clientfifo";

INITIALIZE_MUTEX(condition_mutex);
pthread_cond_t  condition_cond  = PTHREAD_COND_INITIALIZER;

THREAD_ID_T resync_tid[MAX_DISKS];


pthread_attr_t rattr[MAX_DISKS];

int ioctl_fd;

IOCTL ioctl_obj;
extern stop_work(char str[100]);
extern THREAD_LOCK_T db_lock;

int g_pid = -1;
int link_down = -1; 
int nl_socket;
fd_set rfds, wfds;
struct timeval tv;

time_t time_raw_format;
struct tm * ptr_time;
char date[20];
char timee[20];
struct ebdr_thread_t thread_params[MAX_DISKS+CNTRL_THREADS];

int prepare_and_send_ioctl_cmd(int cmd)
{
	int minor = 1; 
	int ret;

	if((ret = ioctl(ioctl_fd, cmd, &ioctl_obj)) < 0)
	{
		perror("ioctl failed");
		ebdr_log(INFO, "[prepare_and_send_ioctl] cmd: %d ret = %d\n", cmd, ret);
		stop_work("daemon ioctl failed ");
	}
	return ret;
}


void prepare_for_replication(char *target_disk, int client_pid)
{
	ebdr_log(INFO, "[prepare_for_replication] Initializing target disk \n");

	ebdr_disk_init(SECONDARY, target_disk, &ebdr_disk_target_obj[client_pid]);
	ebdr_disk_target_obj[client_pid].ops->disk_setup(&ebdr_disk_target_obj[client_pid]);

	replic_client_obj[client_pid].rep_disk = &ebdr_disk_target_obj[client_pid];
	ebdr_log(INFO, "[prepare_for_replication] disk_fd: %d\n", replic_client_obj[client_pid].rep_disk->disk_fd);
}

void start_protocol_negotiation(unsigned int bitmap_size, unsigned int grain_size, unsigned int chunk_size, int client_pid)
{
	replic_hdr_client_obj[client_pid].opcode = SYN;
	replic_hdr_client_obj[client_pid].bitmap_size = bitmap_size;
	replic_hdr_client_obj[client_pid].grain_size = grain_size;
	replic_hdr_client_obj[client_pid].chunk_size = chunk_size;
	replic_hdr_client_obj[client_pid].partner_id = client_pid;

	ebdr_log(INFO, "[Daemon] sending SYN to server fd:%d \n", all_partner_clients[client_pid].socket_fd);
	replic_client_obj[client_pid].rep_conn->conn_ops->do_ebdr_conn_send(&replic_hdr_client_obj[client_pid],
			sizeof(replic_header), all_partner_clients[client_pid].socket_fd, PROTO_TYPE);

	ebdr_log(INFO, "[Daemon] recv syn-ack from server \n");
	replic_client_obj[client_pid].rep_conn->conn_ops->do_ebdr_conn_recv(&replic_hdr_client_obj[client_pid],
			sizeof(replic_header), all_partner_clients[client_pid].socket_fd, PROTO_TYPE, client_pid);

	replic_hdr_client_obj[client_pid].opcode = ACK;

	ebdr_log(INFO, "[Daemon] sending ack to server \n");
	replic_client_obj[client_pid].rep_conn->conn_ops->do_ebdr_conn_send(&replic_hdr_client_obj[client_pid],
			sizeof(replic_header), all_partner_clients[client_pid].socket_fd, PROTO_TYPE);
}


void request_server_for_metadata(int client_pid)
{
	ebdr_user_bitmap_init(REMOTE, client_pid);
	replic_hdr_client_obj[client_pid].opcode = BITMAP_METADATA;
	replic_hdr_client_obj[client_pid].partner_id = client_pid;

	if(bitmap_client_obj[client_pid].obj_state != BITMAP_OBJ_PAUSE)
	{
		bitmap_client_obj[client_pid].ops->ebdr_bitmap_setup(ebdr_disk_target_obj[client_pid].bitmap, client_pid); 

		ebdr_log(INFO, "[Daemon] partner_id:[%d] client_sock_fd[%d]\n", client_pid,
				all_partner_clients[client_pid].socket_fd);

		replic_client_obj[client_pid].rep_conn->conn_ops->do_ebdr_conn_send(&replic_hdr_client_obj[client_pid],
				sizeof(replic_header), all_partner_clients[client_pid].socket_fd, PROTO_TYPE);

		replic_client_obj[client_pid].rep_conn->conn_ops->do_ebdr_conn_recv(bitmap_client_obj[client_pid].bitmap_area,
				bitmap_client_obj[client_pid].bitmap_size, all_partner_clients[client_pid].socket_fd, DATA_TYPE, client_pid);

		ebdr_disk_target_obj[client_pid].bitmap = &bitmap_client_obj[client_pid];
		bitmap_client_obj[client_pid].ops->ebdr_bitmap_setup(ebdr_disk_target_obj[client_pid].bitmap, client_pid);

		ebdr_log(INFO, "[client_send_recv] received bitmap metadata[%d]:0x%lx for client[%d]\n", client_pid,
				*(ebdr_disk_target_obj[client_pid].bitmap->bitmap_area),
				all_partner_clients[client_pid].socket_fd);
	}
}

void make_partner_with_server(char *partner_ip, int bandwidth, char *disk, unsigned long bitmap_size, unsigned long grain_size, unsigned long chunk_size )
{
	int client_pid;

	client_pid = get_new_partner_client_id();
	ebdr_connection_init(EBDR_CONN_CLIENT, client_pid);
	ebdr_conn_client[client_pid].conn_ops->do_ebdr_conn_setup(&ebdr_conn_client[client_pid], partner_ip, client_pid);
	
  replication_init(REP_REMOTE,client_pid);
	replic_client_obj[client_pid].ops->replic_obj_setup(&replic_client_obj[client_pid], client_pid);

	if (client_pid < MAX_PARTNERS)
	{
		ebdr_partner_init(PARTNER_CLIENT, &all_partner_clients[client_pid]);

		//call make partner for client using the all_partner_clientp[client_pid]
		all_partner_clients[client_pid].ops->make_partner(partner_ip, bandwidth,
				ebdr_conn_client[client_pid].sockFd, client_pid);

	}
	ebdr_log(INFO, "[Daemon] After make partner pid = %d\n", client_pid);
	replic_hdr_client_obj[client_pid].opcode = MAKE_PARTNER;
	replic_hdr_client_obj[client_pid].bandwidth = bandwidth;
	replic_hdr_client_obj[client_pid].partner_id = client_pid;
	strcpy(replic_hdr_client_obj[client_pid].dest_ip, partner_ip);

	strcpy(replic_hdr_client_obj[client_pid].disk, disk);
	replic_hdr_client_obj[client_pid].bitmap_size = bitmap_size;
	replic_hdr_client_obj[client_pid].grain_size = grain_size;
	replic_hdr_client_obj[client_pid].chunk_size = chunk_size;

	ebdr_log(INFO, "[Daemon] client sock fd[%d]: %d\n", client_pid, ebdr_conn_client[client_pid].sockFd);
	replic_client_obj[client_pid].rep_conn->conn_ops->do_ebdr_conn_send(&replic_hdr_client_obj[client_pid],
			sizeof(replic_header), ebdr_conn_client[client_pid].sockFd, PROTO_TYPE);
}

int make_relation_with_client(int partnerid, int role, char *src_disk, unsigned long int bitmap_size,
		unsigned long int grain_size, unsigned long int chunk_size)
{
	int relation_id, relation_role;

	ebdr_relation_init(EBDR_RELATION_SERVER, &all_relation_servers[partnerid], partnerid);
	if(role == 0)
	{
		relation_role = RELATION_PRIMARY;
	}
	else
	{
		relation_role = RELATION_SECONDARY;
	}
	/* check for partnerid in all_partner_servers list, if it is not there don't make relation  */
	if(is_server_partner_id_valid(partnerid))
	{
		relation_id = all_relation_servers[partnerid].ops->make_relation(relation_role, partnerid, src_disk);
		ebdr_log(INFO, "[Daemon] relation_id = %d\n", relation_id);
	}
	else
	{
		ebdr_log(INFO, "[Daemon] partner id[%d] is not valid. \n", partnerid);
		return -1;
	}
	replic_hdr_server_obj[partnerid].ops = &replic_hdr_server_ops;
	replic_hdr_server_obj[partnerid].ops->replic_hdr_setup(bitmap_size, grain_size, chunk_size, partnerid);
	return 0;
}

int make_relation_with_server(int partnerid, int role, char *target_disk, unsigned long int bitmap_size,
		unsigned long int grain_size, unsigned long int chunk_size)
{
	int relation_role, relation_id;
  
  int client_rid;
  client_rid = partnerid;

	ebdr_relation_init(EBDR_RELATION_CLIENT, &all_relation_clients[partnerid], partnerid);
	if(role == 0)
	{
		relation_role = RELATION_PRIMARY;
	}
	else
	{
		relation_role = RELATION_SECONDARY;
	}

	/* check for partnerid in all_partner_clients list, if it is not there don't make relation  */
	if(is_client_partner_id_valid(partnerid))
	{
		relation_id = all_relation_clients[client_rid].ops->make_relation(relation_role, partnerid, target_disk);
	}
	else
	{
		ebdr_log(INFO, "[Daemon] partner id[%d] is not valid.\n", partnerid);
		return -1;
	}

	ebdr_log(INFO, "\n[Daemon] ******* Starting protocol negotiation... ******** \n");
	/* start protocol negotiation */
	start_protocol_negotiation(bitmap_size, grain_size, chunk_size, partnerid);

	all_relation_clients[relation_id].bitmap_size = replic_hdr_client_obj[relation_id].bitmap_size ;
	all_relation_clients[relation_id].grain_size = replic_hdr_client_obj[relation_id].grain_size ;
	all_relation_clients[relation_id].chunk_size = replic_hdr_client_obj[relation_id].chunk_size ;

	insert_into_relation(all_relation_clients[relation_id].role, all_relation_clients[relation_id].partner_id, 
			all_relation_clients[relation_id].relation_id, RELATION_OBJ_IN_USE, all_relation_clients[relation_id].device, 
			all_relation_clients[relation_id].bitmap_size,
			all_relation_clients[relation_id].grain_size,
			all_relation_clients[relation_id].chunk_size,"ebdrdbc");

	ebdr_log(INFO, "\n[Daemon] @@@@@@@@ Starting preparation for replication... @@@@@@ \n");
	prepare_for_replication(target_disk, partnerid);
	return 0;
}

void remove_relation_in_client(int client_rid)
{
	ebdr_log(INFO, "[Daemon] calling client relation_obj_destroy \n");
	all_relation_clients[client_rid].ops->relation_obj_destroy(client_rid);

	/* Send RM_RELATION to server */
	ebdr_log(INFO, "[Daemon] Sending RM_RELATION to server \n");
	replic_hdr_client_obj[client_rid].opcode = RM_RELATION;
	replic_hdr_client_obj[client_rid].relation_id = client_rid;
	replic_client_obj[client_rid].rep_conn->conn_ops->do_ebdr_conn_send(&replic_hdr_client_obj[client_rid],
			sizeof(replic_header), ebdr_conn_client[client_rid].sockFd, PROTO_TYPE);
}

void remove_partner_with_server(int pid)
{
	ebdr_log(INFO, "[remove_partner_with_server] Sending remove partner opcode to server \n");
	replic_hdr_client_obj[pid].opcode = REMOVE_PARTNER;
	replic_hdr_client_obj[pid].partner_id = pid;
	replic_client_obj[pid].rep_conn->conn_ops->do_ebdr_conn_send(&replic_hdr_client_obj[pid],
			sizeof(replic_header), ebdr_conn_client[pid].sockFd, PROTO_TYPE);

	/* ========================== ASSUMPTION  ======================== 
	 * Assuming relation_id is equal to partner_id, this code won't work if both are different
	 * For simplicty, this code is written. Need to improve this code later
	 */
	if(all_relation_clients[pid].partner_id == pid && all_relation_clients[pid].obj_state == RELATION_OBJ_IN_USE)
	{
		ebdr_log(INFO, "[remove_partner_server] releasing relation obj of pid = [%d]\n", pid);
		all_relation_clients[pid].ops->relation_obj_destroy(pid);
	}
	ebdr_log(INFO, "[remove_partner_server] removing partner at client \n");
	all_partner_clients[pid].ops->partner_obj_destroy(pid);
}

void snapshot_disk_init(char *snap_disk, int pid)
{
	ebdr_log(INFO, "[snapshot_disk_init]Initializing source disk:%s \n", snap_disk);
	ebdr_log(INFO, "[snapshot_disk_init] partner_id: [%d] \n", pid);

	memcpy(ebdr_disk_src_obj[pid].snap_name, snap_disk, DISK_NAME);
	ebdr_log(INFO, "[snapshot_disk_init] Snap Name: %s\n", ebdr_disk_src_obj[pid].snap_name);
	ebdr_log(INFO, "[snapshot_disk_init] recovery_mode = %d\n", recovery_mode);
	ebdr_disk_src_obj[pid].ops->disk_setup(&ebdr_disk_src_obj[pid]);
	printf("[snapshot_disk_init] ebdr_disk_src_obj[%d].name = %s \n", pid, ebdr_disk_src_obj[pid].name);
	if(recovery_mode != 1)
	{
		printf("[snapshot_disk_init] updating snap name in disk table ...\n");
		update_into_disk(pid, ebdr_disk_src_obj[pid].name, ebdr_disk_src_obj[pid].snap_name,"ebdrdbs");
	}
	/* update relation server table with snap disk name */
	memcpy(all_relation_servers[pid].device, snap_disk, DISK_NAME);

	//ebdr_user_bitmap_init(LOCAL, pid);
	//ebdr_log(INFO, "[get_bitmap] partner_id : [%d] \n", pid);
}

void *resync_handler(void *partner_id)
{
	struct ebdr_thread_t tmp_info;
	tmp_info = *(struct ebdr_thread_t *) partner_id;
	//int pid = *(int *)partner_id;
	int pid = tmp_info.pid;
	unsigned long int bit_pos = tmp_info.bit_pos;

	char str[10];

	sprintf(str, "%s%d%s", "./log", pid, ".txt");

  //Setting the thred specific pid
  log_pid = pid;
  //Set the thread params before starting resync
  ebdr_log_setup(log_pid, str);

	ebdr_log(INFO, "Starting resync for partner id : [%d]\n", pid);

	/* Check if partner_state is PARTNER_OBJ_PAUSE */

	if(all_partner_clients[pid].obj_state == PARTNER_OBJ_RESUME || ebdr_disk_target_obj[pid].obj_state == DISK_OBJ_PAUSE)
	{
		bit_pos = replic_client_obj[pid].last_resynced_bit;
		ebdr_log(INFO, "[resync_start] resync resumed at bit_pos = %lu\n", bit_pos);
	}
	//mode is zeroed out
	restart_resync(pid, bit_pos, 0);
}


int resync_init(int pid, unsigned long int bit_pos)
{
	int ret = 0;
	io_engine_init(IO_CLIENT, pid);

	/* check client partner id is valid or not */
#if 0
	if(is_client_partner_id_valid(pid) == 0)
	{
		ebdr_log(INFO, "[resync_init] partner id is not valid\n");
		return -1;
	}
#endif

	if(pid < MAX_DISKS)
	{
		/* Create resync thread for each client */
		ret = pthread_attr_init(&rattr[pid]);
		if(ret != 0)
		{
			ebdr_log(FATAL, "Error initalizing attributes\n");
			stop_work("[resync_init] pthread_attr_init failed ");
		}

		thread_params[pid].pid = pid;
		thread_params[pid].bit_pos = bit_pos;
		ebdr_log(INFO, "[resync_init] resync_tid[%d] \n", pid);
		pthread_attr_setdetachstate(&rattr[pid], PTHREAD_CREATE_DETACHED);
		if(pthread_create( &resync_tid[pid], &rattr[pid],  resync_handler, &thread_params[pid]) < 0)
		{
			perror("could not create resync thread");
			stop_work("[resync_init] pthread_create failed");
		}
	}
	else
	{
		ebdr_log(ERROR, "There is no such partner id: %d\n", pid);
	}
	return 0;
}


int validate_ipaddr(char *ipaddr)
{
	char tmp_ip[64];
	if(0 == inet_pton(AF_INET, ipaddr, &tmp_ip))
	{
		return 0; /* wrong ip address */
	}
	return 1;
}

int is_disk_created(char *disk)
{
	int ret;
	IOCTL ioctl_obj_tmp;
	memcpy(&ioctl_obj_tmp, &ioctl_obj, sizeof(ioctl_obj));

	prepare_and_send_ioctl_cmd(EBDRCTL_BLKDEV_SEARCH);
	if(*((int *)&ioctl_obj))
		ret = 1;
	else
		ret = 0;

	memcpy(&ioctl_obj, &ioctl_obj_tmp, sizeof(ioctl_obj));
	return ret;

}

int parse_daemon_request(char *cmd)
{
	char tmp[256];
	char partner_ip[64];
	unsigned long int bitmap_size, grain_size, chunk_size, bandwidth; 
	char snap_disk[DISK_NAME];
	char disk_destn[DISK_NAME];
	char target_disk[DISK_NAME];
	char src_disk[DISK_NAME];
	int partnerid, ret = 0;
	int pid, rid;
	int relation_role, role, relation_id;
	static int once = 0;
  
  int server_pid;
  int client_pid;
  int thrdId, log_level;

	switch(cmd[0])
	{
    case 'a':
  			sscanf(cmd,"%s %d %d", tmp,
					&thrdId, &log_level);
        thread_params[thrdId].logging_level = log_level;
        if(log_level == 1)
          strcpy(thread_params[thrdId].logStrng, "FATAL");
        if(log_level == 2)
          strcpy(thread_params[thrdId].logStrng, "ERROR");
        if(log_level == 3)
          strcpy(thread_params[thrdId].logStrng, "WARN");
        if(log_level == 4)
          strcpy(thread_params[thrdId].logStrng, "INFO");
        if(log_level == 5)
          strcpy(thread_params[thrdId].logStrng, "DEBUG");
        break;

  case 'b':
  			sscanf(cmd,"%s %d %d", tmp,
					&thrdId, &log_level);
        thread_params[thrdId].logging_level = log_level;
        if(log_level == 1)
          strcpy(thread_params[thrdId].logStrng, "FATAL");
        if(log_level == 2)
          strcpy(thread_params[thrdId].logStrng, "ERROR");
        if(log_level == 3)
          strcpy(thread_params[thrdId].logStrng, "WARN");
        if(log_level == 4)
          strcpy(thread_params[thrdId].logStrng, "INFO");
        if(log_level == 5)
          strcpy(thread_params[thrdId].logStrng, "DEBUG");
 
        break;


	          
		case 'c':
			ebdr_log(INFO, "create device\n");
			sscanf(cmd,"%s %s %lu %lu", tmp,
					ioctl_obj.src_disk,
					&ioctl_obj.u_grain_size,
					&ioctl_obj.u_bitmap_size);
			ebdr_log(INFO, "backing device of ebdr :%s \n", ioctl_obj.src_disk);

			if(!is_disk_created(ioctl_obj.src_disk))
			{
				prepare_and_send_ioctl_cmd(EBDRCTL_DEV_CREATE);
				server_pid = ds_count;
				// ebdr_log_setup(ds_count);
				ebdr_disk_init(PRIMARY, ioctl_obj.src_disk, &ebdr_disk_src_obj[server_pid]);
				if(!once)
				{
					ebdr_connection_init(EBDR_CONN_SERVER, MAX_CONN +1);
					ebdr_conn_server[MAX_CONN + 1].conn_ops->do_ebdr_conn_setup(&ebdr_conn_server[MAX_CONN +1], NULL, MAX_CONN +1);
					once = 1;
				}
			}
			else
				ebdr_log(ERROR, "device: %s has been already created \n", ioctl_obj.src_disk);

			break;
		case 'e':
			server_list_partners();
			break;
		case 'f':
			client_list_partners();
			break;
		case 'g':
			ebdr_log(INFO, "set grain size and bitmap size\n");
			sscanf(cmd,"%s %lu %lu",tmp, &ioctl_obj.u_grain_size,
					&ioctl_obj.u_bitmap_size);
			prepare_and_send_ioctl_cmd(EBDRCTL_SET_GRAIN_SIZE);
			break;
		case 'i':
			server_list_relations();
			break;
		case 'j':
			client_list_relations();
			break;
		case 'm':
			ebdr_log(INFO, "start ebdr client bitmap \n");
			sscanf(cmd, "%s %d", tmp, &pid);
			/* Validate partner id */
			ret = is_client_partner_id_valid(pid);
			if(ret)
			{
				request_server_for_metadata(pid);
			}
			else
			{
				ebdr_log(ERROR, "Client Partner id is invalid \n");
			}
			break;
	case 's':
			ebdr_log(INFO, "start snap copy \n");
			memset(snap_disk, '\0', DISK_NAME);
			sscanf(cmd,"%s %s %d",tmp, snap_disk, &pid);
			/* Validate partner id */
			ret = is_server_partner_id_valid(pid);
			if(ret)
			{
				snapshot_disk_init(snap_disk, pid);
			}
			else
			{
				ebdr_log(INFO, "Server Partner id is invalid \n");
			}
			break;
		case 'p':
			ebdr_log(INFO, "make partner ...\n");
			sscanf(cmd,"%s %s %lu %s %lu %lu %lu",tmp, partner_ip, &bandwidth, target_disk, &bitmap_size,
					&grain_size, &chunk_size );
			ret = validate_ipaddr(partner_ip);
			if(ret)
			{
				make_partner_with_server(partner_ip, bandwidth, target_disk, bitmap_size, grain_size, chunk_size);
			}
			else
			{
				ebdr_log(ERROR, "IP is wrong. Please check IP address again\n");
			}
			break;
		case 'k':
			ebdr_log(INFO, "remove partner...\n");
			sscanf(cmd,"%s %d",tmp, &pid);
			/* Validat partner id */
			ret = is_server_partner_id_valid(pid);
			if(ret)
			{
				remove_partner_with_server(pid);
			}
			else
			{
				ebdr_log(INFO, "Client Partner id is invalid \n");
			}
			break;
		case 'x':
			ebdr_log(INFO, "[Daemon] Start make relation with client \n");
			sscanf(cmd,"%s %d %d %s %lu %lu %lu", tmp, &partnerid, &role, src_disk, &bitmap_size,
					&grain_size, &chunk_size);

			/* Validate partner id */
			ret = is_server_partner_id_valid(partnerid);
			if(ret)
			{
				make_relation_with_client(partnerid, role, src_disk, bitmap_size, grain_size, chunk_size);
			}
			else
			{
				ebdr_log(ERROR, "Server Partner id is invalid \n");
			}
			break;
		case 'y':
			ebdr_log(INFO, "[Daemon] Start make relation with server \n");
			sscanf(cmd,"%s %d %d %s %lu %lu %lu", tmp, &partnerid, &role, target_disk, &bitmap_size,
					&grain_size, &chunk_size);

			/* Validate partner id */
			ret = is_client_partner_id_valid(partnerid);
			if(ret)
			{
				make_relation_with_server(partnerid, role, target_disk, bitmap_size, grain_size, chunk_size);
			}
			else
			{
				ebdr_log(ERROR, "Please check input again \n");
			}
			break;
		case 'o':
			ebdr_log(INFO, "remove relationpeer...\n");
			sscanf(cmd,"%s %d",tmp, &rid);
			remove_relation_in_client(rid);
			break;
		case 'r':
			ebdr_log(INFO, "calling resync_start...\n");
			sscanf(cmd, "%s %d", tmp, &pid);
			/* Validate partner id */
			ret = is_client_partner_id_valid(pid);
			if(ret)
			{
				resync_init(pid, 0);
			}
			else
			{
				ebdr_log(INFO, "Client Partner id is invalid \n");
			}
			break;
		case 'n':
			ebdr_log(INFO, "pausing resync...\n");
			sscanf(cmd, "%s %d", tmp, &pid);
			/* Validate partner id */
			ret = is_client_partner_id_valid(pid);
			if(ret)
			{
				if(replic_client_obj[pid].last_resynced_bit == 0) 
				{
					ebdr_log(INFO, "Resync already finished.\n");
				}
				else
				{
					all_partner_clients[pid].obj_state = PARTNER_OBJ_PAUSE;
				}
			}
			else
			{
				ebdr_log(INFO, "Client Partner id is invalid \n");
			}
			break;
		case 'l':
			ebdr_log(INFO, "resuming resync...\n");
			sscanf(cmd, "%s %d", tmp, &pid);
			/* Validate partner id */
			ret = is_client_partner_id_valid(pid);
			if(ret)
			{
				if(all_partner_clients[pid].obj_state == PARTNER_OBJ_RELEASED)
				{
					ebdr_log(INFO, "Resync already completed.\n");
				}
				else
				{
					all_partner_clients[pid].obj_state = PARTNER_OBJ_RESUME;
					resync_init(pid, 0);
				}
			}
			else
			{
				ebdr_log(INFO, "Client Partner id is invalid \n");
			}
			break;
		case 'z':
#ifdef SERVER 
			mkrelation_from_db_on_server();
#endif
#ifdef CLIENT
			mkpartner_from_db_on_client();
			mkrelation_from_db_on_client();

#endif
			break;
		default: 
			break;
	}

	return 0;
}



static int ebdr_daemonize(void)
{
	pid_t process_id = 0;
	pid_t sid = 0;

	/* Create child process */
	process_id = fork();
	if (process_id < 0)
	{
		ebdr_log(INFO, "fork failed!\n");
		stop_work("[ebdr_daemonize] fork failed ");
	}
	/* Parent process */
	if (process_id > 0)
	{
		ebdr_log(INFO, "process_id of child process %d \n", process_id);
		exit(0);
	}
	/* unmask the file mode */
	umask(0);
	/* set new session */
	sid = setsid();
	if(sid < 0)
	{
		exit(1);
	}
	/* Change the current working directory to root */
	chdir("/");
	/* Close stdin. stdout and stderr */
	close(STDIN_FILENO);
	//close(STDOUT_FILENO);
	close(STDERR_FILENO);
	return 0;
}


int open_server_fifo()
{
	int server_fifo_fd;
	server_fifo_fd = open(serverfifo, O_RDONLY);
	if (server_fifo_fd == -1)
	{
		perror("server Unable to open FIFO");
		exit(0);
		close(server_fifo_fd);
	}
	return server_fifo_fd;

}

int open_client_fifo()
{
	int client_fifo_fd;
	client_fifo_fd = open(clientfifo, O_RDONLY);
	if (client_fifo_fd == -1)
	{
		perror("client Unable to open FIFO");
		exit(0);
		close(client_fifo_fd);
	}
	return client_fifo_fd;

}


int open_fifo()
{

#ifdef SERVER  
	fifo_fd = open_server_fifo();
#endif

#ifdef CLIENT
	fifo_fd = open_client_fifo();
#endif

	return fifo_fd;
}


static int create_new_connection(int pid, char *ip_addr)
{
	int sockFd = 0, ret;
	struct sockaddr_in srvAddr;

	if((sockFd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		stop_work("Can't open soc_stream socket\n");
	}

	ebdr_log(INFO, "client socket creation successful \n");
	memset(&srvAddr, 0, sizeof(srvAddr));
	srvAddr.sin_family = AF_INET;
	srvAddr.sin_addr.s_addr = inet_addr(ip_addr);
	srvAddr.sin_port = htons(SRV_TCP_PORT);

	ebdr_conn_client[pid].sockFd = sockFd;
	if(connect(ebdr_conn_client[pid].sockFd, (struct sockaddr *)&srvAddr, sizeof(srvAddr)) < 0)
	{
		stop_work("[create_new_connection] Can't connect to server");
	}else {
		ebdr_log(INFO, "connected to the server\n");
	}

	return 0;
}


static void restart_connection(int pid)
{
	create_new_connection(pid, all_partner_clients[pid].ip); 

	all_partner_clients[pid].socket_fd = ebdr_conn_client[pid].sockFd;
	printf("[restart_connection] New client socket fd = %d\n", ebdr_conn_client[pid].sockFd);

	bitmap_client_obj[pid].obj_state = BITMAP_OBJ_RESUME;
	if(all_partner_clients[pid].obj_state == PARTNER_OBJ_PAUSE)
	{
		all_partner_clients[pid].obj_state = PARTNER_OBJ_RESUME; 
	}
	printf("[restart_connection] Last resynced bit = %lu\n", replic_client_obj[pid].last_resynced_bit);
	printf("[restart_connection] all_partner_clients obj state = %d\n", all_partner_clients[pid].obj_state);
}


void link_status_check_init()
{
	struct sockaddr_nl addr;

	nl_socket = socket (AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);

	printf("In link_status_init\n");

	if (nl_socket < 0)
	{
		printf ("Socket Open Error!");
		exit (1);
	}
	memset ((void *) &addr, 0, sizeof (addr));

	addr.nl_family = AF_NETLINK;
	addr.nl_pid = getpid ();
	addr.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR | RTMGRP_IPV6_IFADDR;

	if (bind (nl_socket, (struct sockaddr *) &addr, sizeof (addr)) < 0)
	{
		printf ("Socket bind failed!");
		exit (1);
	}
}


int link_status_check()
{
	int status;
	int retval;
	int ret = 0;
	char buf[4096];
	struct iovec iov = { buf, sizeof buf };
	struct sockaddr_nl snl;
	struct nlmsghdr *h;
	struct ifinfomsg *ifi;
	struct msghdr msg = { (void *) &snl, sizeof snl, &iov, 1, NULL, 0, 0 };

	status = recvmsg (nl_socket, &msg, 0);
	if (status < 0)
	{
		/* Socket non-blocking so bail out once we have read everything */
		if (errno == EWOULDBLOCK || errno == EAGAIN)
			return ret;

		/* Anything else is an error */
		printf ("read_netlink: Error recvmsg: %d\n", status);
		perror ("read_netlink: Error: ");
		return status;
	}
	if (status == 0)
	{
		printf ("read_netlink: EOF\n");
	}


	// We need to handle more than one message per 'recvmsg'
	for (h = (struct nlmsghdr *) buf; NLMSG_OK (h, (unsigned int) status);
			h = NLMSG_NEXT (h, status))
	{
		//Finish reading 
		if (h->nlmsg_type == NLMSG_DONE)
			return ret;

		// Message is some kind of error 
		if (h->nlmsg_type == NLMSG_ERROR)
		{
			printf ("read_netlink: Message is an error - decode TBD\n");
			return -1;        // Error
		}

		if (h->nlmsg_type == RTM_NEWLINK)
		{
			ifi = NLMSG_DATA (h);
			printf ("NETLINK::%s\n", (ifi->ifi_flags & IFF_RUNNING) ? "Up" : "Down");
			if(ifi->ifi_flags & IFF_RUNNING)
			{
				printf("In  UP case \n");
				link_down = 0; 
				printf("[Link_UP] g_pid = %d\n", g_pid);
#if CLIENT
				restart_connection(g_pid);
#endif
			}
			else
			{
				printf("In Down Case \n");
				link_down = 1;
				printf("pid = %d\n", g_pid);
#if CLIENT          
				bitmap_client_obj[g_pid].obj_state = BITMAP_OBJ_PAUSE;
				printf("bitmap client obj state = %d\n", bitmap_client_obj[g_pid].obj_state);
				all_partner_clients[g_pid].obj_state = PARTNER_OBJ_PAUSE;
				printf("partner client obj state = %d\n", all_partner_clients[g_pid].obj_state);
#endif
#if SERVER
				printf("[link_status] closing server sockfd: %d\n", ebdr_conn_server[g_pid].conn_fd);
				shutdown(ebdr_conn_server[g_pid].conn_fd, SHUT_RDWR); 
				close(ebdr_conn_server[g_pid].conn_fd);
#endif

			}
		}
	}
}


void *cntrl_thread_init(void *arg)
{
	int retval;


  log_pid = MAX_DISKS + CNTRL_THREADS - 1;
  
  ebdr_log_setup(log_pid, CNTRL_THREAD_LOG);

#ifdef SERVER
	open_fd_for_ioctl();
#endif
	fifo_fd = open_fifo();

	link_status_check_init();

	while (1)
	{
		FD_ZERO (&rfds);
		FD_CLR (nl_socket, &rfds);
		FD_SET (nl_socket, &rfds);
		FD_SET (fifo_fd, &rfds);

		tv.tv_sec = 10;
		tv.tv_usec = 0;

		retval = select (FD_SETSIZE, &rfds, NULL, NULL, &tv);
		if(FD_ISSET(fifo_fd, &rfds))
		{
			retval = read_from_fifo(fifo_fd);
			/* process the request received via IPC */
			if(retval)
			{
				parse_daemon_request(str);
			}
		}

		if(FD_ISSET(nl_socket, &rfds))
		{
			link_status_check();
		}


		memset(str, 0, sizeof(str));
	}
	close(ioctl_fd);

}


static void thread_init()
{
	pthread_create(&cntrl_thread, NULL, cntrl_thread_init, NULL );
	pthread_create(&recovery_thread, NULL, recovery_thread_init, NULL );
	pthread_join(cntrl_thread, NULL);
	pthread_join(recovery_thread, NULL);
}


FILE * open_ebdr_slog()
{
	// FILE * ebdrlog_sfp;

  log_pid = MAX_DISKS + CNTRL_THREADS - 0;
  
  ebdr_log_setup(log_pid, SERVER_LOG);

	if (thread_params[log_pid].ebdrlog_fp == NULL)
	{
		stop_work("opening sever log file failed ");
	}
	return thread_params[log_pid].ebdrlog_fp;
}


FILE * open_ebdr_clog()
{
	//    FILE * ebdrlog_fp;

  log_pid = MAX_DISKS + CNTRL_THREADS - 0;
  
  ebdr_log_setup(log_pid, CLIENT_LOG);

	if (thread_params[log_pid].ebdrlog_fp == NULL)
	{
		stop_work("opening client log file failed ");
	}
	return thread_params[log_pid].ebdrlog_fp;
}


void ebdr_logging()
{

#ifdef SERVER  
	ebdrlog_fp = open_ebdr_slog();
	ebdr_log(INFO, "server log file creation successful\n");
#endif

#ifdef CLIENT
	ebdrlog_fp = open_ebdr_clog();
	ebdr_log(INFO, "client log file creation successful\n");
#endif

}


void atexit_func(void)
{
	int ret = 0, i = 0;

	//Closing the ioctl_fd
	ebdr_log(INFO, "at exit called\n");
	//Cleaning the open disk descriptors
	for(i = 0; i < MAX_DISKS; i++)
	{
		if(ebdr_disk_src_obj[i].obj_state = DISK_OBJ_IN_USE)
		{
			ret = close(ebdr_disk_src_obj[i].disk_fd);

			if(ret < 0) {
				ebdr_log(ERROR, "close failed atexit_func %d \n", errno);
				stop_work("atexit_func");
			}
		}
	}
	pthread_mutex_destroy(&db_lock);
}



int main(int argc, char* argv[])
{
	int i = 0;

  log_path = (char *)malloc(LOG_PATH_LEN);

  log_path = getenv("LOG_PATH");
  if(!log_path)
   {
     printf("LOG_PATH environment variable not set, using the default path: /tmp/\n");
     log_path = (char *)malloc(LOG_PATH_LEN);
     strcpy(log_path, "/tmp/");
   }
  printf("log_path =%s\n", log_path);

  strcpy(tmp_log_path, log_path);

  printf("tmp_log_path =%s\n", tmp_log_path);
	ebdr_logging();

	ebdr_daemonize();
	i = atexit(atexit_func);

	if (i != 0) {
		fprintf(stderr, "cannot set exit function\n");
		exit(EXIT_FAILURE);
	}

#if SERVER
	db_init("ebdrdbs");
#endif
#if CLIENT
	db_init("ebdrdbc");
#endif

	create_fifo();

	thread_init();

	return 0;
}

