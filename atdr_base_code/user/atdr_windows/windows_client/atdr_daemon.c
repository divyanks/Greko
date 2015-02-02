#include <stdio.h>
#include <stdlib.h>

#define _WIN32_WINNT 0x0501

#include <conio.h>
#include <windows.h>

#include <stdio.h>
#include "..\Include\atdr_log.h"
#include "..\Include\atdr_daemon.h"
#include "..\Include\atdr_fifo.h"
#include "..\Include\atdr_disk.h"
#include "..\Include\atdr_conn.h"
#include "..\Include\atdr_partner.h"
#include "..\Include\atdr_replication.h"
#include "..\Include\atdr_replication_header.h"
#include "..\Include\atdr_relation.h"
#include "..\Include\atdr_disk.h"
#include "..\Include\atdr_bitmap_user.h"
#include "..\Include\atdr_io_engine.h"
#include "..\Include\atdr_ioctl_cmds.h"
#include "..\Include\atdr_db.h"

#define LOG_PATH_LEN 128


HANDLE fifo_fd;
HANDLE server_fifo_fd;
HANDLE client_fifo_fd;
BOOL bResult;

IOCTL ioctl_obj;

int log_pid;

char *log_path;

char tmp_log_path[LOG_PATH_LEN];
FILE * atdrlog_fp;

struct atdr_thread_t thread_params[MAX_DISKS + CNTRL_THREADS ];
void restart_resync(int pid, unsigned long int bit_pos, enum rep_mode_t mode);
DWORD resync_tid[MAX_DISKS];
typedef struct IOCTL_INPUT
{
	UINT32  len;
} RG_INPUT, *RG_PINPUT;

typedef struct IOCTL_OUTPUT_CREATE
{

	PVOID MappedUserAddress;
	PVOID Dummy;
	
	
} OUTPUT_CREATE, *POUTPUT_CREATE;


typedef struct IOCTL_OUTPUT_USER
{
	UINT8 bmap_user;


} OUTPUT_USER, *POUTPUT_USER;


int prepare_and_send_ioctl_cmd(int cmd)
{
	int bytesReturned;

	HANDLE hDevice = NULL;
	


#define WCHAR_BUFFER_SIZE 64
	WCHAR tmp[WCHAR_BUFFER_SIZE];
	WCHAR disk_name[WCHAR_BUFFER_SIZE];
	int length = 0;
	int retries = 0;
	wcscpy(disk_name, L"\\\\.\\");
	length = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, ioctl_obj.src_disk, -1, (LPWSTR)tmp,
		WCHAR_BUFFER_SIZE / sizeof(WCHAR));
	wcscat(&disk_name[4], tmp);
	if (length <= 0)
	{
		atdr_log(ATDR_ERROR, "Failed to convert to multibyte wide characters %d\n", GetLastError()); /* This print will change later */
		return -1;
	}

	
		hDevice = CreateFile(disk_name, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
			OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);

		atdr_log(ATDR_INFO, "Opened the file");

		if (hDevice == INVALID_HANDLE_VALUE) {

			atdr_log(ATDR_INFO, "Opened the file %d", GetLastError());

			exit(0);
		}

#define MAX_RETRIES 5

	try_again:

		if (cmd == ATDRCTL_DEV_CREATE)
		{
			OUTPUT_CREATE out;

		

			bResult = DeviceIoControl(hDevice, cmd, &ioctl_obj, sizeof(IOCTL), &out, sizeof(OUTPUT_CREATE), &bytesReturned, NULL);
			atdr_log(ATDR_INFO, "after DeviceIoControl \n");

			//Set the bitmaps once ATDRCTL_DEV_CREATE happens
			if (cmd == ATDRCTL_DEV_CREATE)
			{
				atdr_disk_src_obj[ds_count].active_bitmap = (unsigned long *)out.MappedUserAddress;
				atdr_disk_src_obj[ds_count].active_bitmap_backup = atdr_disk_src_obj[ds_count].active_bitmap + ioctl_obj.u_bitmap_size / sizeof(ulong_t);
			}
		}
		else {
			OUTPUT_USER out;
			bResult = DeviceIoControl(hDevice, cmd, &ioctl_obj, sizeof(IOCTL), &out, sizeof(OUTPUT_USER), &bytesReturned, NULL);
			atdr_log(ATDR_INFO, "after DeviceIoControl \n");

			//Only valid for GET_USER
			// bmap user @ Output should be 0 first iteration and 1 in second iteration
			ioctl_obj.bmap_user = out.bmap_user;

		}

		if (!bResult)
		{
			if (retries < MAX_RETRIES) {
				retries++;
				atdr_log(ATDR_ERROR, "failed to IOCTL retrying %d times\n", retries);
				goto try_again;
			}
			else {
				stop_work("Failed to create %d times", MAX_RETRIES);
			}
		}

		CloseHandle(hDevice);
		
		return bResult;

}

void stop_work(char str[100])
{
	atdr_log(ATDR_ERROR, " %s Error occured..Stopped !! errno = %d\n", str, errno);
	exit(0); /* return a meaningful value*/
}

FILE * open_atdr_clog()
{
	//    FILE * atdrlog_fp;

	log_pid = MAX_DISKS + CNTRL_THREADS - 1;

	atdr_log_setup(log_pid, CLIENT_LOG);

	if (thread_params[log_pid].atdrlog_fp == NULL)
	{
		stop_work("opening client log file failed ");
	}
	return thread_params[log_pid].atdrlog_fp;
}


FILE * open_atdr_slog()
{
	// FILE * atdrlog_sfp;

	log_pid = MAX_DISKS + CNTRL_THREADS - 1;

	atdr_log_setup(log_pid, SERVER_LOG);

	if (thread_params[log_pid].atdrlog_fp == NULL)
	{
		stop_work("opening sever log file failed ");
	}
	return thread_params[log_pid].atdrlog_fp;
}


void atdr_logging()
{
#ifdef SERVER
	atdrlog_fp = open_atdr_slog();
	atdr_log(ATDR_INFO, "client log file creation successful\n");
#endif

#ifdef CLIENT
	atdrlog_fp = open_atdr_clog();
	atdr_log(ATDR_INFO, "client log file creation successful\n");
#endif

}

HANDLE open_server_fifo()
{
	server_fifo_fd = CreateNamedPipe(
		SERVER_FIFO,             // pipe name 
		PIPE_ACCESS_DUPLEX,       // read/write access 
		PIPE_TYPE_MESSAGE |       // message type pipe 
		PIPE_READMODE_MESSAGE |   // message-read mode 
		PIPE_WAIT,                // blocking mode 
		PIPE_UNLIMITED_INSTANCES, // max. instances  
		BUFFER_SIZE,              // output buffer size 
		BUFFER_SIZE,              // input buffer size 
		NMPWAIT_USE_DEFAULT_WAIT, // client time-out 
		NULL);                    // default security attribute 

	if (INVALID_HANDLE_VALUE == server_fifo_fd)
	{
		printf("\nError occurred while creating the pipe: %d", GetLastError());
		exit(1);
		return NULL;  //Error
	}
	else
	{
		printf("\nCreateNamedPipe() was successful.");
	}

	printf("\nWaiting for client connection...");

	//Wait for the client to connect
	BOOL bClientConnected = ConnectNamedPipe(server_fifo_fd, NULL);

	if (FALSE == bClientConnected)
	{
		printf("\nError occurred while connecting to the client: %d", GetLastError());
		CloseHandle(server_fifo_fd);
		return NULL;  //Error
	}
	else
	{
		printf("\nConnectNamedPipe() was successful.");
	}

	return server_fifo_fd;
}

HANDLE open_client_fifo()
{
	client_fifo_fd = CreateNamedPipe(
		CLIENT_FIFO,             // pipe name 
		PIPE_ACCESS_DUPLEX,       // read/write access 
		PIPE_TYPE_MESSAGE |       // message type pipe 
		PIPE_READMODE_MESSAGE |   // message-read mode 
		PIPE_WAIT,                // blocking mode 
		PIPE_UNLIMITED_INSTANCES, // max. instances  
		BUFFER_SIZE,              // output buffer size 
		BUFFER_SIZE,              // input buffer size 
		NMPWAIT_WAIT_FOREVER, // client time-out 
		NULL);                    // default security attribute 

	if (INVALID_HANDLE_VALUE == client_fifo_fd)
	{
		printf("\nError occurred while creating the pipe: %d", GetLastError());
		return NULL;
	}
	else
	{
		printf("\nCreateNamedPipe() was successful.");
	}

	printf("\nWaiting for client connection...");

	//Wait for the client to connect
	BOOL bClientConnected = ConnectNamedPipe(client_fifo_fd, NULL);

	if (FALSE == bClientConnected)
	{
		printf("\nError occurred while connecting to the client: %d", GetLastError());
		CloseHandle(client_fifo_fd);
		return NULL;  //Error
	}
	else
	{
		printf("\nConnectNamedPipe() was successful.");
	}
	return client_fifo_fd;
}

static void create_fifo(void)
{
#ifdef CLIENT
	fifo_fd = open_client_fifo();
#endif
#ifdef SERVER
	fifo_fd = open_server_fifo();
#endif
}

int make_relation_with_client(int partnerid, int role, char *src_disk, unsigned long int bitmap_size,
	unsigned long int grain_size, unsigned long int chunk_size)
{
	int relation_id, relation_role;

	atdr_relation_init(ATDR_RELATION_SERVER, &all_relation_servers[partnerid], partnerid);
	if (role == 0)
	{
		relation_role = RELATION_PRIMARY;
	}
	else
	{
		relation_role = RELATION_SECONDARY;
	}
	/* check for partnerid in all_partner_servers list, if it is not there don't make relation  */
	if (is_server_partner_id_valid(partnerid))
	{
		relation_id = all_relation_servers[partnerid].ops->make_relation(relation_role, partnerid, src_disk);
		atdr_log(ATDR_INFO, "[Daemon] relation_id = %d\n", relation_id);
	}
	else
	{
		atdr_log(ATDR_INFO, "[Daemon] partner id[%d] is not valid. \n", partnerid);
		return -1;
	}
	replic_hdr_server_obj[partnerid].ops = &replic_hdr_server_ops;
	replic_hdr_server_obj[partnerid].ops->replic_hdr_setup(bitmap_size, grain_size, chunk_size, partnerid);
	return 0;
}


static int atdr_daemonize(void)
{
	SC_HANDLE schSCManager, schService;



	// The service executable location, just dummy here else make sure the executable is there :o)...

	// Well as a real example, we are going to create our own telnet service.

	// The executable for telnet is: C:\WINDOWS\system32\tlntsvr.exe

#ifdef CLIENT
	LPCTSTR lpszBinaryPathName = L"%SystemRoot%\\system32\\atdrdc.exe";

	// Service display name...

	LPCTSTR lpszDisplayName = L"atdrdc";

	// Registry Subkey

	LPCTSTR lpszServiceName = L"atdrdc";
#endif
#ifdef SERVER
	LPCTSTR lpszBinaryPathName = L"%SystemRoot%\\system32\\atdrds.exe";

	// Service display name...

	LPCTSTR lpszDisplayName = L"atdrds";

	// Registry Subkey

	LPCTSTR lpszServiceName = L"atdrds";
#endif


	// Open a handle to the SC Manager database...

	schSCManager = OpenSCManager(

		NULL, // local machine

		NULL, // SERVICES_ACTIVE_DATABASE database is opened by default

		SC_MANAGER_ALL_ACCESS); // full access rights



	if (NULL == schSCManager)

		printf("OpenSCManager() failed, error: %d.\n", GetLastError());

	else

		printf("OpenSCManager() looks OK.\n");



	// Create/install service...

	// If the function succeeds, the return value is a handle to the service. If the function fails, the return value is NULL.

	schService = CreateService(

		schSCManager, // SCManager database

		lpszServiceName, // name of service

		lpszDisplayName, // service name to display

		SERVICE_ALL_ACCESS, // desired access

		SERVICE_WIN32_OWN_PROCESS, // service type

		SERVICE_DEMAND_START, // start type

		SERVICE_ERROR_NORMAL, // error control type

		lpszBinaryPathName, // service's binary

		NULL, // no load ordering group

		NULL, // no tag identifier

		NULL, // no dependencies, for real telnet there are dependencies lor

		NULL, // LocalSystem account

		NULL); // no password



	if (schService == NULL)

	{

		printf("CreateService() failed, error: %ld\n", GetLastError());

		return FALSE;

	}

	else

	{

		printf("CreateService() for %S looks OK.\n", lpszServiceName);

		if (CloseServiceHandle(schService) == 0)

			printf("CloseServiceHandle() failed, Error: %d.\n", GetLastError());

		else

			printf("CloseServiceHandle() is OK.\n");

		return 0;

	}
}

static DWORD read_from_fifo(HANDLE fifo_fd)
{
	char szBuffer[CMD_BUFFER_LEN];
	DWORD cbBytes;

	
	//Read client message
	BOOL bResult = ReadFile(
		fifo_fd,                // handle to pipe 
		szBuffer,             // buffer to receive data 
		CMD_BUFFER_LEN,     // size of buffer 
		&cbBytes,             // number of bytes read 
		NULL);                // not overlapped I/O 

	if ((!bResult) || (0 == cbBytes))
	{
		printf("\nError occurred while reading from the client: %d", GetLastError());
		exit(0);
		return 0;  //Error
	}
	else
	{
		printf("\nReadFile() was successful.");
		strcpy(str, szBuffer);
		printf("\nClient sent the following message: %s", str);
		return 1;
	}
}

int validate_ipaddr(char *ipaddr)
{
	
	return 1;
}


int is_disk_created(char *disk)
{
	int ret = 0;
#if 0
	IOCTL ioctl_obj_tmp;
	memcpy(&ioctl_obj_tmp, &ioctl_obj, sizeof(ioctl_obj));

	prepare_and_send_ioctl_cmd(ATDRCTL_BLKDEV_SEARCH);
	if (*((int *)&ioctl_obj))
		ret = 1;
	else
		ret = 0;

	memcpy(&ioctl_obj, &ioctl_obj_tmp, sizeof(ioctl_obj));
#endif
	return ret;

}


int request_server_for_metadata(int client_pid)
{
	atdr_user_bitmap_init(REMOTE, client_pid);
	replic_hdr_client_obj[client_pid].opcode = BITMAP_METADATA;
	replic_hdr_client_obj[client_pid].partner_id = client_pid;

	if (all_partner_clients[client_pid].obj_state == PARTNER_OBJ_RELEASED)
	{
		all_partner_clients[client_pid].obj_state = PARTNER_OBJ_IN_USE;
		all_relation_clients[client_pid].obj_state = RELATION_OBJ_IN_USE;
		atdr_disk_target_obj[client_pid].obj_state = DISK_OBJ_IN_USE;

		update_into_partner(client_pid, all_partner_clients[client_pid].obj_state, "atdrdbc");
		update_relation_state(client_pid, all_relation_clients[client_pid].obj_state, "atdrdbc");
		update_disk_state(client_pid, atdr_disk_target_obj[client_pid].obj_state, "atdrdbc");
	}

	if (bitmap_client_obj[client_pid].obj_state != BITMAP_OBJ_PAUSE)
	{
		bitmap_client_obj[client_pid].ops->atdr_bitmap_setup(atdr_disk_target_obj[client_pid].bitmap, client_pid);

		atdr_log(ATDR_INFO, "[Daemon] partner_id:[%d] client_sock_fd[%d]\n", client_pid,
			all_partner_clients[client_pid].socket_fd);

		replic_client_obj[client_pid].rep_conn->conn_ops->do_atdr_conn_send(&replic_hdr_client_obj[client_pid],
			sizeof(replic_header), all_partner_clients[client_pid].socket_fd, PROTO_TYPE);

		replic_client_obj[client_pid].rep_conn->conn_ops->do_atdr_conn_recv(bitmap_client_obj[client_pid].bitmap_area,
			bitmap_client_obj[client_pid].bitmap_size, all_partner_clients[client_pid].socket_fd, DATA_TYPE, client_pid);

		atdr_disk_target_obj[client_pid].bitmap = &bitmap_client_obj[client_pid];
		//bitmap_client_obj[client_pid].ops->atdr_bitmap_setup(atdr_disk_target_obj[client_pid].bitmap, client_pid);

		atdr_log(ATDR_INFO, "[client_send_recv] received bitmap metadata[%d]:0x%lx for client[%d]\n", client_pid,
			*(atdr_disk_target_obj[client_pid].bitmap->bitmap_area),
			all_partner_clients[client_pid].socket_fd);
		if (*(atdr_disk_target_obj[client_pid].bitmap->bitmap_area) == 0)
		{
			return 0;
		}
		else
		{
			bitmap_client_obj[client_pid].ops->store_bitmap_to_disk(&bitmap_client_obj[client_pid], client_pid);
			return 1;
		}
	}
	return 0;
}
void make_partner_with_server(char *partner_ip, int bandwidth, char *disk, unsigned long bitmap_size, unsigned long grain_size, unsigned long chunk_size)
{
	int client_pid;

	client_pid = get_new_partner_client_id();


	atdr_connection_init(ATDR_CONN_CLIENT, client_pid);
	atdr_conn_client[client_pid].conn_ops->do_atdr_conn_setup(&atdr_conn_client[client_pid], partner_ip, client_pid);

	replication_init(REP_REMOTE, client_pid);

	replic_client_obj[client_pid].ops->replic_obj_setup(&replic_client_obj[client_pid], client_pid);

	if (client_pid < MAX_PARTNERS)
	{
		atdr_partner_init(PARTNER_CLIENT, &all_partner_clients[client_pid]);

		//call make partner for client using the all_partner_clientp[client_pid]
		all_partner_clients[client_pid].ops->make_partner(partner_ip, bandwidth,
			atdr_conn_client[client_pid].sockFd, client_pid);

	}
	atdr_log(ATDR_INFO, "[Daemon] After make partner pid = %d\n", client_pid);
	replic_hdr_client_obj[client_pid].opcode = MAKE_PARTNER;
	replic_hdr_client_obj[client_pid].bandwidth = bandwidth;
	replic_hdr_client_obj[client_pid].partner_id = client_pid;
	strcpy(replic_hdr_client_obj[client_pid].dest_ip, partner_ip);

	strcpy(replic_hdr_client_obj[client_pid].disk, disk);
	replic_hdr_client_obj[client_pid].bitmap_size = bitmap_size;
	replic_hdr_client_obj[client_pid].grain_size = grain_size;
	replic_hdr_client_obj[client_pid].chunk_size = chunk_size;

	atdr_log(ATDR_INFO, "[Daemon] client sock fd[%d]: %d\n", client_pid, atdr_conn_client[client_pid].sockFd);
	replic_client_obj[client_pid].rep_conn->conn_ops->do_atdr_conn_send(&replic_hdr_client_obj[client_pid],
		sizeof(replic_header), atdr_conn_client[client_pid].sockFd, PROTO_TYPE);
}

void prepare_for_replication(char *target_disk, int client_pid)
{
	atdr_log(ATDR_INFO, "[prepare_for_replication] Initializing target disk \n");

	atdr_disk_init(SECONDARY, target_disk, &atdr_disk_target_obj[client_pid]);
//	atdr_disk_target_obj[client_pid].ops->disk_setup(&atdr_disk_target_obj[client_pid]);

	replic_client_obj[client_pid].rep_disk = &atdr_disk_target_obj[client_pid];
	atdr_log(ATDR_INFO, "[prepare_for_replication] disk_fd: %d\n", replic_client_obj[client_pid].rep_disk->disk_fd);
}



void start_protocol_negotiation(unsigned int bitmap_size, unsigned int grain_size, unsigned int chunk_size, int client_pid)
{
	replic_hdr_client_obj[client_pid].opcode = SYN;
	replic_hdr_client_obj[client_pid].bitmap_size = bitmap_size;
	replic_hdr_client_obj[client_pid].grain_size = grain_size;
	replic_hdr_client_obj[client_pid].chunk_size = chunk_size;
	replic_hdr_client_obj[client_pid].partner_id = client_pid;

	atdr_log(ATDR_INFO, "[Daemon] sending SYN to server fd:%d \n", all_partner_clients[client_pid].socket_fd);
	replic_client_obj[client_pid].rep_conn->conn_ops->do_atdr_conn_send(&replic_hdr_client_obj[client_pid],
		sizeof(replic_header), all_partner_clients[client_pid].socket_fd, PROTO_TYPE);

	atdr_log(ATDR_INFO, "[Daemon] recv syn-ack from server \n");
	replic_client_obj[client_pid].rep_conn->conn_ops->do_atdr_conn_recv(&replic_hdr_client_obj[client_pid],
		sizeof(replic_header), all_partner_clients[client_pid].socket_fd, PROTO_TYPE, client_pid);


	replic_hdr_client_obj[client_pid].opcode = ACK;

	atdr_log(ATDR_INFO, "[Daemon] sending ack to server \n");
	replic_client_obj[client_pid].rep_conn->conn_ops->do_atdr_conn_send(&replic_hdr_client_obj[client_pid],
		sizeof(replic_header), all_partner_clients[client_pid].socket_fd, PROTO_TYPE);
}


int make_relation_with_server(int partnerid, int role, char *target_disk, unsigned long int bitmap_size,
	unsigned long int grain_size, unsigned long int chunk_size)
{
	int relation_role, relation_id;

	int client_rid;
	client_rid = partnerid;

	atdr_relation_init(ATDR_RELATION_CLIENT, &all_relation_clients[partnerid], partnerid);
	if (role == 0)
	{
		relation_role = RELATION_PRIMARY;
	}
	else
	{
		relation_role = RELATION_SECONDARY;
	}

	/* check for partnerid in all_partner_clients list, if it is not there don't make relation  */
	if (is_client_partner_id_valid(partnerid))
	{
		relation_id = all_relation_clients[client_rid].ops->make_relation(relation_role, partnerid, target_disk);
	}

	else
	{
		atdr_log(ATDR_INFO, "[Daemon] partner id[%d] is not valid.\n", partnerid);
		return -1;
	}

	atdr_log(ATDR_INFO, "\n[Daemon] ******* Starting protocol negotiation... ******** \n");
	/* start protocol negotiation */
    start_protocol_negotiation(bitmap_size, grain_size, chunk_size, partnerid); 

	all_relation_clients[relation_id].bitmap_size = replic_hdr_client_obj[relation_id].bitmap_size;
	all_relation_clients[relation_id].grain_size = replic_hdr_client_obj[relation_id].grain_size;
	all_relation_clients[relation_id].chunk_size = replic_hdr_client_obj[relation_id].chunk_size;

	insert_into_relation(all_relation_clients[relation_id].role, all_relation_clients[relation_id].partner_id, 
		all_relation_clients[relation_id].relation_id, RELATION_OBJ_IN_USE, all_relation_clients[relation_id].device,
		all_relation_clients[relation_id].bitmap_size,
		all_relation_clients[relation_id].grain_size,
		all_relation_clients[relation_id].chunk_size, "atdrdbc");

	atdr_log(ATDR_INFO, "\n[Daemon] @@@@@@@@ Starting preparation for replication... @@@@@@ \n");
	prepare_for_replication(target_disk, partnerid); 
	return 0;
}

void snapshot_disk_init(char *snap_disk, int pid)
{
	atdr_log(ATDR_INFO, "[snapshot_disk_init]Initializing source disk:%s \n", snap_disk);
	atdr_log(ATDR_INFO, "[snapshot_disk_init] partner_id: [%d] \n", pid);

	memcpy(atdr_disk_src_obj[pid].snap_name, snap_disk, DISK_NAME);
	atdr_log(ATDR_INFO, "[snapshot_disk_init] Snap Name: %s\n", atdr_disk_src_obj[pid].snap_name);
	atdr_log(ATDR_INFO, "[snapshot_disk_init] recovery_mode = %d\n", recovery_mode);
	atdr_disk_src_obj[pid].ops->disk_setup(&atdr_disk_src_obj[pid]);
	printf("[snapshot_disk_init] atdr_disk_src_obj[%d].name = %s \n", pid, atdr_disk_src_obj[pid].name);
	if (recovery_mode != 1)
	{
		printf("[snapshot_disk_init] updating snap name in disk table ...\n");
		update_into_disk(pid, atdr_disk_src_obj[pid].name, atdr_disk_src_obj[pid].snap_name, "atdrdbs");
	}
	/* update relation server table with snap disk name */
	memcpy(all_relation_servers[pid].device, snap_disk, DISK_NAME);

	//atdr_user_bitmap_init(LOCAL, pid);
	//atdr_log(INFO, "[get_bitmap] partner_id : [%d] \n", pid);
}


void resync_handler(int partner_id)
{
	// struct atdr_thread_t tmp_info;
	// tmp_info = *(struct atdr_thread_t *) partner_id;
	int pid = partner_id;
	// int pid = tmp_info.pid;
	unsigned long int bit_pos = thread_params[pid].bit_pos; // = tmp_info.bit_pos;

	char str[20];

	sprintf(str, "%s%d%s", "./log", pid, ".txt");

	//Setting the thred specific pid
	log_pid = pid;
	//Set the thread params before starting resync
	atdr_log_setup(log_pid, str);

	atdr_log(ATDR_INFO, "Starting resync for partner id : [%d]\n", pid);

	/* Check if partner_state is PARTNER_OBJ_PAUSE */
#if 0 
	if (all_partner_clients[pid].obj_state == PARTNER_OBJ_RESUME || atdr_disk_target_obj[pid].obj_state == DISK_OBJ_PAUSE)
	{
		bit_pos = replic_client_obj[pid].last_resynced_bit;
		atdr_log(ATDR_INFO, "[resync_start] resync resumed at bit_pos = %lu\n", bit_pos);
	}
#endif
	//mode is zeroed out
	restart_resync(pid, bit_pos, 0);
}


int resync_init(int pid, unsigned long int bit_pos)
{
	int ret = 0;
	HANDLE hndle;

	io_engine_init(IO_CLIENT, pid);

	/* check client partner id is valid or not */

	if (is_client_partner_id_valid(pid) == 0)
	{
		atdr_log(ATDR_INFO, "[resync_init] partner id is not valid\n");
		return -1;
	}


	if (pid < MAX_DISKS)
	{
		thread_params[pid].pid = pid;
		thread_params[pid].bit_pos = bit_pos;
		atdr_log(ATDR_INFO, "[resync_init] resync_tid[%d] \n", pid);
		hndle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)resync_handler, pid, 0, &resync_tid[pid]);
		if(hndle ==  NULL)
		{
			perror("could not create resync thread");
			stop_work("[resync_init] pthread_create failed");
		}
		CloseHandle(hndle);
	}
	else
	{
		atdr_log(ATDR_ERROR, "There is no such partner id: %d\n", pid);
	}
	return 0;
}




int parse_daemon_request(char *cmd)
{
	char tmp[256];
	char partner_ip[64];
	unsigned long int bitmap_size, grain_size, chunk_size, bandwidth;
	char target_disk[DISK_NAME];
	char snap_disk[DISK_NAME];

	int ret;
	int partnerid, role;
	int pid;

	static int once = 0;
	int server_pid;


	switch (cmd[0])
	{
	case 'c':
		atdr_log(ATDR_INFO, "create device\n");
		sscanf(cmd, "%s %s %lu %lu", tmp,
			ioctl_obj.src_disk,
			&ioctl_obj.u_grain_size,
			&ioctl_obj.u_bitmap_size);
		atdr_log(ATDR_INFO, "backing device of atdr :%s \n", ioctl_obj.src_disk);

		if (!is_disk_created(ioctl_obj.src_disk))
		{
			prepare_and_send_ioctl_cmd(ATDRCTL_DEV_CREATE);
			server_pid = ds_count;
			//atdr_log_setup(ds_count);
			atdr_disk_init(PRIMARY, ioctl_obj.src_disk, &atdr_disk_src_obj[server_pid]);
			if (!once)
			{
				atdr_connection_init(ATDR_CONN_SERVER, MAX_CONN + 1);
				atdr_conn_server[MAX_CONN + 1].conn_ops->do_atdr_conn_setup(&atdr_conn_server[MAX_CONN + 1], NULL, MAX_CONN + 1);
				once = 1;
			}
		}
		else
			atdr_log(ERROR, "device: %s has been already created \n", ioctl_obj.src_disk);
		break;

	case 'l':
		atdr_log(ATDR_INFO, "resuming resync...\n");
		sscanf(cmd, "%s %d", tmp, &pid);
		/* Validate partner id */
		ret = is_client_partner_id_valid(pid);
		if (ret)
		{
			if (replic_client_obj[pid].rep_state == RESYNC_COMPLETED)
			{
				atdr_log(ATDR_INFO, "Resync already completed.\n");
			}
			else
			{
				all_partner_clients[pid].obj_state = PARTNER_OBJ_RESUME;
				replic_client_obj[pid].rep_state = RESYNC_RESUME;
				update_into_partner(pid, all_partner_clients[pid].obj_state, "atdrdbc");
				update_resync_state(pid, replic_client_obj[pid].rep_state, "RESYNC_RESUME ", "atdrdbc");
				resync_init(pid, replic_client_obj[pid].last_resynced_bit);
				//resync_init(pid, 0);
			}
		}
		else
		{
			atdr_log(ATDR_INFO, "Client Partner id is invalid \n");
		}
		break;


	case 'm':
		atdr_log(ATDR_INFO, "start atdr client bitmap \n");
		sscanf(cmd, "%s %d", tmp, &pid);
		/* Validate partner id */
		ret = is_client_partner_id_valid(pid);
		if (ret)
		{
			request_server_for_metadata(pid);
		}
		else
		{
			atdr_log(ERROR, "Client Partner id is invalid \n");
		}
		break;

	case 'n':
		atdr_log(ATDR_INFO, "pausing resync...\n");
		sscanf(cmd, "%s %d", tmp, &pid);
		/* Validate partner id */
		ret = is_client_partner_id_valid(pid);
		if (ret)
		{
			if (replic_client_obj[pid].last_resynced_bit == 0)
			{
				atdr_log(ATDR_INFO, "Resync already finished.\n");
			}
			else
			{
				all_partner_clients[pid].obj_state = PARTNER_OBJ_PAUSE;
				replic_client_obj[pid].rep_state = RESYNC_PAUSE;
				update_into_partner(pid, all_partner_clients[pid].obj_state, "atdrdbc");
				update_resync_state(pid, replic_client_obj[pid].rep_state, "RESYNC_PAUSE", "atdrdbc");
			}
		}
		else
		{
			atdr_log(ATDR_INFO, "Client Partner id is invalid \n");
		}
		break;

	case 'p':
		atdr_log(ATDR_INFO, "make partner ...\n");
		sscanf(cmd, "%s %s %lu %s %lu %lu %lu", tmp, partner_ip, &bandwidth, target_disk, &bitmap_size,
			&grain_size, &chunk_size);
		ret = validate_ipaddr(partner_ip);
		if (ret)
		{
			make_partner_with_server(partner_ip, bandwidth, target_disk, bitmap_size, grain_size, chunk_size);
		}
		else
		{
			atdr_log(ERROR, "IP is wrong. Please check IP address again\n");
		}
		break;
	case 'q':
		sscanf(cmd, "%s %d", tmp, &pid);
		restart_resync(pid, 0, FULL_RESYNC);
		break;
	case 'r':
		atdr_log(ATDR_INFO, "calling resync_start...\n");
		sscanf(cmd, "%s %d", tmp, &pid);
		/* Validate partner id */
		ret = is_client_partner_id_valid(pid);
		if (ret)
		{
			resync_init(pid, 0);
		}
		else
		{
			atdr_log(ATDR_INFO, "Client Partner id is invalid \n");
		}
		break;
	case 's':
		atdr_log(ATDR_INFO, "start snap copy \n");
		memset(snap_disk, '\0', DISK_NAME);
		sscanf(cmd, "%s %s %d", tmp, snap_disk, &pid);
		/* Validate partner id */
		ret = is_server_partner_id_valid(pid);
		if (ret)
		{
			snapshot_disk_init(snap_disk, pid);
		}
		else
		{
			atdr_log(ATDR_INFO, "Server Partner id is invalid \n");
		}
		break;


	case 'y':
		atdr_log(ATDR_INFO, "[Daemon] Start make relation with server \n");
		sscanf(cmd, "%s %d %d %s %lu %lu %lu", tmp, &partnerid, &role, target_disk, &bitmap_size,
			&grain_size, &chunk_size);

		/* Validate partner id */
		ret = is_client_partner_id_valid(partnerid);
		if (ret)
		{
			make_relation_with_server(partnerid, role, target_disk, bitmap_size, grain_size, chunk_size);
		}
		else
		{
			atdr_log(ERROR, "Please check input again \n");
		}
		break;

	}

	return 0;
}



DWORD  WINAPI cntrl_thread_init(LPVOID lpParam)
{
	int retval;
	int once = 0;

	while (1)
	{
#ifdef SERVER
		fifo_fd = open_server_fifo();
#endif
#ifdef CLIENT
			fifo_fd = open_client_fifo();
#endif
			retval = read_from_fifo(fifo_fd);
			if (retval)
			{
				parse_daemon_request(str);
			}
			memset(str, 0, sizeof(str));
			DisconnectNamedPipe(fifo_fd);
			CloseHandle(fifo_fd);

	}

	return 0;

}

DWORD  WINAPI recovery_thread_init(LPVOID lpParam)
{
	printf("The parameter: %u.\n", *(DWORD*)lpParam);
	printf("inside recovery thread\n");
	return 0;
}


int thread_init()
{
	DWORD cntrl_thread, dwThrdParam = 1;

	HANDLE cntrlThread,rcvryThread;
	printf("thread init\n");
	cntrlThread = CreateThread(
		NULL, // default security attributes
		0, // use default stack size
		cntrl_thread_init, // thread function
		&dwThrdParam, // argument to thread function
		0, // use default creation flags
		&cntrl_thread);// returns the thread identifier
	printf("after cntrol thread create\n");
	WaitForSingleObject(
		cntrlThread,
		INFINITE
		);
	WaitForSingleObject(
		cntrl_thread,
		INFINITE
		);

	rcvryThread = CreateThread(
		NULL, // default security attributes
		0, // use default stack size
		recovery_thread_init, // thread function
		&dwThrdParam, // argument to thread function
		0, // use default creation flags
		&rcvryThread);// returns the thread identifier
	WaitForSingleObject(
		rcvryThread,
		INFINITE
		);
	printf("after recovery thread create \n");

	return 0;
}

int main(int argc, char* argv[])
{
	int i = 0;
	log_path = (char *)malloc(LOG_PATH_LEN);
	log_path = getenv("LOG_PATH");
	if (!log_path)
	{
		printf("LOG_PATH environment variable not set, using the default path: /tmp/\n");
		log_path = (char *)malloc(LOG_PATH_LEN);
		strcpy(log_path, "C:\\");
	}
	printf("log_path =%s\n", log_path);

	strcpy(tmp_log_path, log_path);

	printf("tmp_log_path =%s\n", tmp_log_path);
	atdr_logging();
	atdr_daemonize();

#if SERVER
	db_init("atdrdbs");
#endif
#if CLIENT
	db_init("atdrdbc");
#endif

//	create_fifo();

	thread_init();
	return 0;
}

