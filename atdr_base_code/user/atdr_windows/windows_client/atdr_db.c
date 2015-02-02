#include <my_global.h>
#include <mysql.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "..\Include\atdr_partner.h"
#include "..\Include\atdr_log.h"
#include "..\Include\atdr_relation.h"
#include "..\Include\atdr_replication.h"
#include "..\Include\atdr_replication_header.h"
#include "..\Include\atdr_disk.h"
#include "..\Include\atdr_db.h"
#include "..\Include\atdr_daemon.h"
#include "..\Include\sys_common.h"
#include "..\Include\atdr_bitmap_user.h"

struct variables
{
	char key[10];
	char *var;
};


#define MAX_DB_ERR_LEN 256

int recovery_mode[MAX_PID] = { 0 };
extern int client_connected;		
extern int errno;

MYSQL *con;
HANDLE db_lock;
DWORD dwWaitResult;

struct variables Arr[] = {
	"username", db_user,
	"password", db_pswrd,
	"host", db_host
};


int mutex_lock()
{
	int ret = FALSE;

	
	dwWaitResult = WaitForSingleObject(	db_lock, INFINITE);  // no time-out interval
	
	switch (dwWaitResult)
	{
		// The thread got ownership of the mutex
	case WAIT_OBJECT_0:
		__try {
			// TODO: Write to the database
			atdr_log(ATDR_INFO, "Thread %d  got ownership of mutex and it writing to database...\n",
				GetCurrentThreadId());
			ret = TRUE;
		}

		__finally {
			atdr_log(ATDR_INFO, "Thread %d  does got ownership of mutex so returning false\n",
				GetCurrentThreadId());
			/*
			// Release ownership of the mutex object
			if (!ReleaseMutex(db_lock))
			{
				// Handle error.
			}
			ret = FALSE;
			*/
		}
		break;

		// The thread got ownership of an abandoned mutex
		// The database is in an indeterminate state
	case WAIT_ABANDONED:
		atdr_log(ATDR_INFO, "Thread %d  does got ownership of mutex and mutex is abonded state\n",
			GetCurrentThreadId());
		ret = FALSE;
	}
	
	return ret;
}

int mutex_unlock()
{
	if (!ReleaseMutex(db_lock))
	{
		atdr_log(ATDR_FATAL, "Error in unlocking mutex %d\n", GetLastError());
		return -1;
	}
	else
		return 0;
}

void finish_with_error(MYSQL *con)
{
	char str[256];
	memset(str, '\0', 256);
	if(con != NULL)
	{
		sprintf(str, "%s\n", mysql_error(con));
		mysql_close(con);
	}
	if (!ReleaseMutex(db_lock))
		atdr_log(ATDR_FATAL, "Error in unlocking mutex\n");
	
}

/* resync table 09-10-14 */
int retrieve_from_resync(struct atdr_replication *replic_obj, struct atdr_disk *disk_obj)
{
	int num_fields,i=0;
	MYSQL_ROW row;
	MYSQL_RES *res;

	/* RESYNC_COMPLETED = 3 */
	if (mysql_query(con, "select * from resync where state != 3"))
	{
		//Query Failed
		finish_with_error(con);
	}

	res = mysql_store_result(con);
	//Check the return value

	num_fields = mysql_num_fields(res);
	//Check the return value

	while(row = mysql_fetch_row(res))
	{
		sscanf(row[1], "%s", disk_obj[i].name);
		sscanf(row[2], "%lu", &replic_obj[i].last_resynced_bit);
		sscanf(row[3], "%u", &replic_obj[i].rep_state);
		printf("[retrieve_resync] target disk name = %s last_resynced_bit = %lu rep_state = %d\n", disk_obj[i].name,
			replic_obj[i].last_resynced_bit, replic_obj[i].rep_state);

		i++;
	}
	return i;
}


int update_resync_state(int pid, int state, char *status, char *db_name)
{
	char str[100];

	sprintf(str, "%s %s", "use ", db_name);
	mutex_lock();
	if (mysql_query(con, str)) {
		finish_with_error(con);
	}

	sprintf(str, "%s%d%s%s%s%d", "UPDATE resync SET state = ", state, " , status = '", status, "' WHERE pid = ", pid);
	printf("[update_resync] str = %s \n", str);
	if (mysql_query(con, str)) {
		finish_with_error(con);
	}
	mutex_unlock();
	return 0;
}


int update_into_resync(int pid, char *target_disk, unsigned long int last_resynced_bit, int state, char *db_name)
{
	char str[100];

	sprintf(str, "%s %s", "use ", db_name);
	
	mutex_lock();
	if (mysql_query(con, str)) {
		finish_with_error(con);
	}

	sprintf(str, "%s%lu%s%s%d%s%d", "UPDATE resync SET last_resynced_bit = ", last_resynced_bit, " ", "WHERE pid = ", pid, " AND state = ", state);
	printf("[update_resync] str = %s \n", str);
	if (mysql_query(con, str)) {
		finish_with_error(con);
	}
	mutex_unlock();

	return 0;
}

int insert_into_resync(int pid, char *target_disk, unsigned long int last_resynced_bit, int state, char *status, char *db_name)
{
	char str[100];

	sprintf(str, "%s %s", "use ", db_name);
	mutex_lock();
	if (mysql_query(con, str)) {
		finish_with_error(con);
	}

	sprintf(str, "%s%d%s%s%s%lu%s%d%s%s%s", "INSERT INTO resync VALUES(", pid, ",'", target_disk, "',", last_resynced_bit, ",", state, ",' ", status, "')");
	printf("[insert_resync] str = %s \n", str);
	if (mysql_query(con, str)) {
		finish_with_error(con);
	}
	mutex_unlock();
	return 0;
}

int update_disk_state(int pid, int state, char *db_name)
{
	char str[100];

	sprintf(str, "%s %s", "use ", db_name);
	mutex_lock();
	if (mysql_query(con, str)) {
		finish_with_error(con);
	}

	sprintf(str, "%s%d%s%s%d", "UPDATE disk SET state = ", state, " ", "WHERE pid = ", pid);
	printf("[update_disk] str = %s \n", str);
	if (mysql_query(con, str)) {
		finish_with_error(con);
	}
	mutex_unlock();
	return 0;
}

int update_into_disk(int pid, char *name, char *snap_name, char *db_name )
{
	char str[250];

	sprintf(str, "%s %s", "use ", db_name);
	printf("[update_into_disk] str = %s \n", str);

	mutex_lock();

	if (mysql_query(con, str)) {
		finish_with_error(con);
	}

	sprintf(str, "%s%d%s%s%s%s%s%s%s%s","UPDATE disk SET pid = ", pid, ",snap_name = ", "'",snap_name , "'"," WHERE name = ", "'",name, "'");
	printf("[update_into_disk] str = %s \n", str);
	if (mysql_query(con, str)) {
		finish_with_error(con);
	}
	mutex_unlock();
	return 0;
}


int retrieve_from_disk(struct atdr_disk *atdr_dsk)
{
	int num_fields,i=0;
	MYSQL_ROW row;
	MYSQL_RES *res;

	/* DISK_OBJ_RELEASED = 4*/
	if (mysql_query(con, "select * from disk where state != 4"))
	{
		//Query Failed
		finish_with_error(con);
	}

	res = mysql_store_result(con);
	//Check the return value
	num_fields = mysql_num_fields(res);
	while(row = mysql_fetch_row(res))
	{
		//sscanf(row[0], "%d", pid );
		sscanf(row[1], "%s", atdr_dsk[i].name );
		sscanf(row[2], "%s", atdr_dsk[i].snap_name);
		sscanf(row[3], "%lu", &atdr_dsk[i].bitmap_size);
		sscanf(row[4], "%u", &atdr_dsk[i].obj_state);
		printf("[retrieve_from_disk] disk_name = %s, snap_name = %s bitmap_size = %lu obj_state = %d\n", atdr_dsk[i].name,
			atdr_dsk[i].snap_name, atdr_dsk[i].bitmap_size, atdr_dsk[i].obj_state);

		i++;
	}
	return i;
}

int insert_into_disk(int pid, char *name, char *snap_name, unsigned long int bitmap_size, int state, char *db_name)
{
	char str[100];

	sprintf(str, "%s %s", "use ", db_name);
	
	mutex_lock();
	if (mysql_query(con, str)) {
		finish_with_error(con);
	}

	sprintf(str, "%s%d%s%s%s%s%s%s%s%lu%s%d%s", "INSERT INTO disk VALUES(", pid, ",'", name, "'", ",", "'", snap_name, "',", bitmap_size, ",", state, ")");
	printf("[insert_into_disk] str = %s \n", str);
	if (mysql_query(con, str)) {
		printf("[insert_into_disk] db_name = %s mysql_error = %s \n", db_name, mysql_error(con));
		finish_with_error(con);
	}
	mutex_unlock();
	return 0;
}

int update_into_partner(int pid, int obj_state, char *db_name)
{
	char str[100];

	sprintf(str, "%s %s", "use ", db_name);
	printf("[update_into_partner] str = %s \n", str);

	mutex_lock();
	if (mysql_query(con, str)) {
		finish_with_error(con);
	}

	sprintf(str, "%s%d%s%d", "UPDATE partner SET state = ", obj_state, " WHERE pid = ", pid);
	printf("[update_into_partner] str = %s \n", str);
	if (mysql_query(con, str)) {
		finish_with_error(con);
	}
	mutex_unlock();
	return 0;
}


int retrieve_from_partner(partner_t *all_partner)
{
	int num_fields,i=0;
	MYSQL_ROW row;
	MYSQL_RES *res;
	/* PARTNER_OBJ_RELEASED = 2*/
	if (mysql_query(con, "select * from partner where state != 2"))
	{
		//Query Failed
		finish_with_error(con);
	}

	res = mysql_store_result(con);
	//Check the return value

	num_fields = mysql_num_fields(res);
	//Check the return value

	while(row = mysql_fetch_row(res))
	{
		sscanf(row[0], "%d", &all_partner[i].id);
		sscanf(row[1], "%u", &all_partner[i].obj_state); 
		sscanf(row[2], "%s", all_partner[i].ip);
		sscanf(row[3], "%lu", &all_partner[i].bandwidth);
		printf("[retrieve_from_partner] pid = %d ip = %s bandwidth = %lu\n",all_partner[i].id,
			   	all_partner[i].ip, all_partner[i].bandwidth);
		i++;
	}
	return i;
}



int insert_into_partner(int pid,enum partner_obj_state_t state, char *ip, unsigned long int bandwidth, char *db_name )
{
	char str[200];
	sprintf(str, "%s %s", "use ", db_name);

	mutex_lock();
	if (mysql_query(con, str)) {
		finish_with_error(con);
	}

	sprintf(str, "%s%d%s%d%s%s%s%lu%s" ,"INSERT INTO partner VALUES(", pid, "," , state ,",'",ip,"',",bandwidth, ")");
	if (mysql_query(con, str)) {
		finish_with_error(con);
	}
	mutex_unlock();
	return 0;
}

int update_relation_state(int pid, int state, char *db_name)
{
	char str[100];

	sprintf(str, "%s %s", "use ", db_name);
	
	mutex_lock();
	if (mysql_query(con, str)) {
		finish_with_error(con);
	}

	sprintf(str, "%s%d%s%s%d", "UPDATE relation SET state = ", state, " ", "WHERE pid = ", pid);
	printf("[update_relation] str = %s \n", str);
	if (mysql_query(con, str)) {
		finish_with_error(con);
	}
	mutex_unlock();
	return 0;
}




int retrieve_from_relation(relation_t *all_relation)
{
	int num_fields,i=0;
	MYSQL_ROW row;
	MYSQL_RES *res;
	/* RELATION_OBJ_RELEASED = 2 */
	if (mysql_query(con, "select * from relation where state != 2"))
	finish_with_error(con);

	res = mysql_store_result(con);
	num_fields = mysql_num_fields(res);

	while(row = mysql_fetch_row(res))
	{
		sscanf(row[0], "%u", &all_relation[i].role);
		sscanf(row[1], "%d", &all_relation[i].partner_id); 
		sscanf(row[2], "%d", &all_relation[i].relation_id);
		sscanf(row[3], "%u", &all_relation[i].obj_state);
		sscanf(row[4], "%s", all_relation[i].device);
		sscanf(row[5], "%lu", &all_relation[i].bitmap_size);
		sscanf(row[6], "%lu", &all_relation[i].grain_size);
		sscanf(row[7], "%lu", &all_relation[i].chunk_size);
		i++;
	}
	return i;
}


int insert_into_relation(int role, int pid, int rid, int state, char *disk, unsigned long int bitmap_size, 
		unsigned long int grain_size, unsigned long int chunk_size, char *db_name)
{
	char str[200];
	sprintf(str, "%s %s", "use ", db_name);
	
	mutex_lock();
	if (mysql_query(con, str)) {
		finish_with_error(con);
	}

	sprintf(str, "%s%d%s%d%s%d%s%d%s%s%s%lu%s%lu%s%lu%s" ,"INSERT INTO relation VALUES(", role, "," , pid, "," , rid , "," , state ,",'",disk,"',",bitmap_size,"," ,grain_size,",",chunk_size,")");
	if (mysql_query(con, str)) {
		finish_with_error(con);
	}
	mutex_unlock();
	return 0;
}


int insert_into_db(int percent, int partner_id, int relation_id, char *disk_name, char *db_name)
{
	char str[200];

	sprintf(str, "%s %s", "use ", db_name);
	mutex_lock();

	if (mysql_query(con, str)) {
		finish_with_error(con);
	}


	memset(str, '\0', 100);
	sprintf(str, "%s%d%s%d%s%s%s%s%s%s%s %d %s", "INSERT INTO Status VALUES(1, ", partner_id, ",", relation_id, 
			", '", disk_name, "','", __DATE__, "','", __TIME__, "',", percent, ")");
	printf("str=%s\n", str)
		;
	if (mysql_query(con, str)){
		finish_with_error(con);
	}
	mutex_unlock();
	return 0;
}


int keyVar(char *key, char *var)
{
	int i=0;
	for(i=0; i<3;i++)
	{
		if(strcmp(Arr[i].key, key) == 0)
		{
			printf("Arr[%d].key=%s\n",i,Arr[i].key);
			atdr_log(ATDR_DATUG, "Arr[%d].key=%s\n",i,Arr[i].key);
			strcpy(Arr[i].var,  var);
		}
	}
	return 0; 
}


static char *tokens[20];
int readConfig()
{
	FILE *dbfp;
	char line[80];
	int tknIndx = 0;

	char filename[60];
	
	strcpy(filename, "c:\\atdr\\atdr_db.conf");
	dbfp = fopen(filename, "r");
	if(!dbfp)
	{
		atdr_log(ATDR_FATAL, "dKb_conf: Opening atdr_db conf file = %s  not successful errno=%d ", filename, errno);
		atdr_log(ATDR_FATAL, "db_conf: err string is =%s ", strerror(errno));
        exit(0);
	}
	else
	{
		atdr_log(ATDR_DATUG, "db_conf: file =%s open successful", filename);
	}
	
	while(fgets(line, 80, dbfp))
	{
		atdr_log(ATDR_DATUG, "db_conf: line = %s", line);
    if(strlen(line) > 1)
    {
		tknIndx = 0;
		tokens[tknIndx] = strtok(line, " :\n");
		while(tokens[tknIndx])
		{
			tokens[++tknIndx] = strtok(NULL, " :\n");
		}
		keyVar(tokens[0], tokens[1]);
    }
	}
	printf("\ndb_conf: db_user=%s db_password=%s db_host=%s\n",db_user, db_pswrd, db_host);
	atdr_log(ATDR_DATUG, "db_conf: db_user=%s db_password=%s db_host=%s\n",db_user, db_pswrd, db_host);
	return 0;
}


int db_init(char *db_name)
{
	char str[100];
	int num_rows = 0;
	int pid_c = 0, pid_s = 0;
	int i = 0,new_sockfd;

	atdr_log(ATDR_DATUG, "\ndb_init: Initialixing mysql_init\n");

	readConfig();

	db_lock = CreateMutex(
		NULL,              // default security attributes
		FALSE,             // initially not owned
		NULL);             // unnamed mutex
	if (!db_lock)
	{
		atdr_log(ATDR_FATAL, "db_init: Initializing mutex lock failed\n");
		return -1;
	}
	
    mutex_lock();



	
	con = mysql_init(NULL);
	if (con == NULL) 
	{
		finish_with_error(con);
	}

	atdr_log(ATDR_DATUG, "db_init: after mysql_init\n");
  my_bool reconnect = 1;
  mysql_options(con, MYSQL_OPT_RECONNECT, &reconnect);

	if (mysql_real_connect(con, db_host,  db_user, db_pswrd, NULL, 0, NULL, 0) == NULL) 
	{
		fprintf(stderr, "%s\n", mysql_error(con));
		atdr_log(ATDR_ERROR, "db_init: error in connection\n");
		finish_with_error(con);
	}  

	atdr_log(ATDR_DATUG, "db_init: after connection\n");

	sprintf(str, "%s %s",  "CREATE DATABASE IF NOT EXISTS", db_name); 
	if (mysql_query(con, str)) 
	{
		fprintf(stderr, "%s\n", mysql_error(con));
		finish_with_error(con);
	}
	printf("db name = %s\n", db_name);
	sprintf(str, "%s %s",  "use ", db_name); 
	if (mysql_query(con, str)) {
		finish_with_error(con);
	}

	if (mysql_query(con, "CREATE TABLE IF NOT EXISTS Status(Id INT, PARTNERID INT, RELATIONID INT, Src TEXT, Date TEXT,	time TEXT, Percent INT)")) {      
		finish_with_error(con);
	}
	
	if (mysql_query(con, "CREATE TABLE IF NOT EXISTS partner(pid INT, state INT, ip CHAR(15), bandwidth INT)")) {      
		finish_with_error(con);
	}
#if SERVER
	num_rows = mkpartner_from_db_on_server();
	printf("[db_init] num_rows partner server = %d \n", num_rows);
	/* Start new connection if there is any entry in database */
	if(num_rows > 0)
	{
		for (i = 0; i < num_rows; i++)
		{
			pid_s = all_partner_servers[i].id;
			recovery_mode[pid_s] = 1;
		}
	}
#endif

#if CLIENT
	num_rows = mkpartner_from_db_on_client();
	printf("[db_init] num_rows partner client = %d \n", num_rows);
	if(num_rows > 0)
	{
		for (i = 0; i < num_rows; i++)
		{
			pid_c = all_partner_clients[i].id;
			recovery_mode[pid_c] = 1; 
		}
	}
#endif

	if (mysql_query(con, "CREATE TABLE IF NOT EXISTS relation(role  INT, pid INT, rid INT, state INT, disk CHAR(15), bitmap_size INT, grain_size INT, chunk_size INT)")){
		finish_with_error(con);
	}
#if SERVER
	num_rows = mkrelation_from_db_on_server();
#endif
#if CLIENT
	num_rows = mkrelation_from_db_on_client();
#endif

	if (mysql_query(con, "CREATE TABLE IF NOT EXISTS disk(pid INT, name CHAR(15), snap_name CHAR(100), bitmap_size INT, state INT)")){
		finish_with_error(con);
	}
#if SERVER
	num_rows = mkdisk_from_db_on_server();
	printf("[db_init] num_rows disk = %d\n", num_rows);
	if(num_rows > 0)
	{
		for(i = 0; i < num_rows; i++)
		{
			pid_s = all_partner_servers[i].id;
			printf("[db_init] ***** Recovering source disk object ***** \n");
			atdr_disk_init(PRIMARY, atdr_disk_src_obj[pid_s].name, &atdr_disk_src_obj[pid_s]);
			snapshot_disk_init(atdr_disk_src_obj[pid_s].snap_name, pid_s);
			atdr_user_bitmap_init(LOCAL, pid_s); /*14.11.14*/
			replication_init(REP_LOCAL, pid_s);
			replic_server_obj[pid_s].ops->replic_obj_setup(&replic_server_obj[pid_s], pid_s);
			replic_hdr_server_obj[pid_s].grain_size = all_relation_servers[pid_s].grain_size;
			replic_hdr_server_obj[pid_s].chunk_size = all_relation_servers[pid_s].chunk_size;
		}
		pid_s = MAX_CONN + 1;
		/* start server connection */
		printf("[db_init] ------ Restarting server connection from Database ------\n");
		atdr_connection_init(ATDR_CONN_SERVER,pid_s);
		atdr_conn_server[pid_s].conn_ops->do_atdr_conn_setup(&atdr_conn_server[pid_s], NULL, pid_s);
	}
#endif
#if CLIENT
	num_rows = mkdisk_from_db_on_client();
	printf("[db_init] num_rows disk = %d\n", num_rows);

#endif


	/* resync table 09-10-14 */
	if (mysql_query(con, "CREATE TABLE IF NOT EXISTS resync(pid INT, target_disk CHAR(15), last_resynced_bit INT, state INT, status CHAR(128))")) {
		finish_with_error(con);
	}
#if CLIENT
	 num_rows = mkresync_from_db_on_client(); 


	if(num_rows > 0)
	{
		for(i = 0; i < num_rows; i++)
		{
			pid_c = all_partner_clients[i].id;
			/* create new client socket and connect to server */
			printf("[db_init] -------- Restarting client connection from Database -------\n");
			atdr_connection_init(ATDR_CONN_CLIENT, pid_c);
			new_sockfd = atdr_conn_client[pid_c].conn_ops->do_atdr_conn_setup(&atdr_conn_client[pid_c],
				all_partner_clients[pid_c].ip, pid_c);
			printf("[db_init] new client sockfd = %d\n", new_sockfd);
			all_partner_clients[pid_c].socket_fd = new_sockfd;

			replication_init(REP_REMOTE, pid_c);
			replic_client_obj[pid_c].ops->replic_obj_setup(&replic_client_obj[pid_c], pid_c);
			prepare_for_replication(atdr_disk_target_obj[pid_c].name, pid_c);
			// ioctl(atdr_disk_target_obj[pid_c].disk_fd, BLKFLSBUF, 0); SANTHOSH MAJOR CHANGE
			replic_hdr_client_obj[pid_c].grain_size = all_relation_clients[pid_c].grain_size;
			replic_hdr_client_obj[pid_c].chunk_size = all_relation_clients[pid_c].chunk_size;

			if (replic_client_obj[pid_c].rep_state != RESYNC_COMPLETED)
			{
				atdr_user_bitmap_init(REMOTE, pid_c);
				bitmap_client_obj[pid_c].ops->load_bitmap_from_disk(&bitmap_client_obj[pid_c], pid_c);
				atdr_disk_target_obj[pid_c].bitmap = &bitmap_client_obj[pid_c];
				printf("[db_init] recoverd bitmap_area = 0x%lx \n", *(atdr_disk_target_obj[pid_c].bitmap->bitmap_area));
				resync_init(pid_c, replic_client_obj[pid_c].last_resynced_bit);
			} /* end if */
			else
			{
				atdr_log(ATDR_ERROR, "metadata received zero \n");
			}

		}

	}
#endif

	atdr_log(ATDR_DATUG, "after use atdrd");
	mutex_unlock();
	return 0;
}
