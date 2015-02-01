#include "ebdr_sys_common.h"
#include "ebdr_partner.h"
#include "ebdr_log.h"
#include "ebdr_db.h"
#include "ebdr_relation.h"
#include "ebdr_replication_header.h"
#include "ebdr_replication.h" /* resync table 09-10-14 */
#include "ebdr_disk.h"

#define MAX_DB_ERR_LEN 256

int recovery_mode = 0;
extern int client_connected;
extern int errno;
MYSQL *con;
THREAD_LOCK_T db_lock;

void finish_with_error(MYSQL *con)
{
	char str[256];
	memset(str, '\0', 256);
	if(con != NULL)
	{
		sprintf(str, "%s\n", mysql_error(con));
		mysql_close(con);
	}
	pthread_mutex_unlock(&db_lock);
	stop_work("finish_with_error %s", str);
}

/* resync table 09-10-14 */
int retrieve_from_resync(struct ebdr_replication *replic_obj, struct ebdr_disk *disk_obj)
{
	int num_fields,i=0;
	MYSQL_ROW row;
	MYSQL_RES *res;

	if (mysql_query(con, "select * from resync"))
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
		printf("[retrieve_resync] target disk name = %s last_resynced_bit = %lu\n", disk_obj[i].name, 
				replic_obj[i].last_resynced_bit);
		i++;
	}
	return i;
}

int update_into_resync(int pid, char *target_disk, unsigned long int last_resynced_bit, char *db_name)
{
	char str[100];

	sprintf(str, "%s %s", "use ", db_name);
	pthread_mutex_lock(&db_lock);
	if (mysql_query(con, str)) {
		finish_with_error(con);
	}

	sprintf(str, "%s%lu%s%s%d","UPDATE resync SET last_resynced_bit = ", last_resynced_bit ," ","WHERE pid = ", pid);
	printf("[update_resync] str = %s \n", str);
	if (mysql_query(con, str)) {
		finish_with_error(con);
	}
	pthread_mutex_unlock(&db_lock);

}

int insert_into_resync(int pid, char *target_disk, unsigned long int last_resynced_bit, char *db_name)
{
	char str[100];

	sprintf(str, "%s %s", "use ", db_name);
	pthread_mutex_lock(&db_lock);
	if (mysql_query(con, str)) {
		finish_with_error(con);
	}

	sprintf(str, "%s%d%s%s%s%lu%s","INSERT INTO resync VALUES(", pid,",'", target_disk,"',", last_resynced_bit ,")");

	printf("[insert_resync] str = %s \n", str);
	if (mysql_query(con, str)) {
		finish_with_error(con);
	}
	pthread_mutex_unlock(&db_lock);

}


int update_into_disk(int pid, char *name, char *snap_name, char *db_name )
{
	char str[100];

	sprintf(str, "%s %s", "use ", db_name);
	printf("[update_into_disk] str = %s \n", str);
//	pthread_mutex_lock(&db_lock);
	if (mysql_query(con, str)) {
		finish_with_error(con);
	}

	sprintf(str, "%s%d%s%s%s%s%s%s%s%s","UPDATE disk SET pid = ", pid, ",snap_name = ", "'",snap_name , "'"," WHERE name = ", "'",name, "'");
	printf("[update_into_disk] str = %s \n", str);
	if (mysql_query(con, str)) {
		finish_with_error(con);
	}
//	pthread_mutex_unlock(&db_lock);

}


int retrieve_from_disk(struct ebdr_disk *ebdr_dsk)
{
	int num_fields,i=0;
	MYSQL_ROW row;
	MYSQL_RES *res;

	if (mysql_query(con, "select * from disk"))
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
		sscanf(row[1], "%s", ebdr_dsk[i].name );
		sscanf(row[2], "%s", ebdr_dsk[i].snap_name);
		sscanf(row[3], "%lu", &ebdr_dsk[i].bitmap_size);
		printf("[retrieve_from_disk] disk_name = %s, snap_name = %s bitmap_size = %lu\n", ebdr_dsk[i].name, 
				ebdr_dsk[i].snap_name, ebdr_dsk[i].bitmap_size);	
		i++;
	}
	return i;
}

int insert_into_disk(int pid, char *name, char *snap_name, unsigned long int bitmap_size, char *db_name )
{
	char str[100];

	sprintf(str, "%s %s", "use ", db_name);
	pthread_mutex_lock(&db_lock);
	if (mysql_query(con, str)) {
		finish_with_error(con);
	}

	sprintf(str, "%s%d%s%s%s%s%s%s%s%lu%s","INSERT INTO disk VALUES(", pid, ",'",name , "'",",","'" , snap_name ,"',", bitmap_size , ")");
	printf("[insert_into_disk] str = %s \n", str);
	if (mysql_query(con, str)) {
		finish_with_error(con);
	}
	pthread_mutex_unlock(&db_lock);

}

int retrieve_from_partner(partner_t *all_partner)
{
	int num_fields,i=0;
	MYSQL_ROW row;
	MYSQL_RES *res;
	if (mysql_query(con, "select * from partner"))
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
	pthread_mutex_lock(&db_lock);
	if (mysql_query(con, str)) {
		finish_with_error(con);
	}

	sprintf(str, "%s%d%s%d%s%s%s%lu%s" ,"INSERT INTO partner VALUES(", pid, "," , state ,",'",ip,"',",bandwidth, ")");
	if (mysql_query(con, str)) {
		finish_with_error(con);
	}
	pthread_mutex_unlock(&db_lock);
}



int retrieve_from_relation(relation_t *all_relation)
{
	int num_fields,i=0;
	MYSQL_ROW row;
	MYSQL_RES *res;
	if (mysql_query(con, "select * from relation"))
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
	pthread_mutex_lock(&db_lock);
	if (mysql_query(con, str)) {
		finish_with_error(con);
	}

	sprintf(str, "%s%d%s%d%s%d%s%d%s%s%s%lu%s%lu%s%lu%s" ,"INSERT INTO relation VALUES(", role, "," , pid, "," , rid , "," , state ,",'",disk,"',",bitmap_size,"," ,grain_size,",",chunk_size,")");
	if (mysql_query(con, str)) {
		finish_with_error(con);
	}
	pthread_mutex_unlock(&db_lock);
}


int insert_into_db(int percent, int partner_id, int relation_id, char *disk_name, char *db_name)
{
	char str[200];

	sprintf(str, "%s %s", "use ", db_name);
	pthread_mutex_lock(&db_lock);

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

	pthread_mutex_unlock(&db_lock);
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
			ebdr_log(DEBUG, "Arr[%d].key=%s\n",i,Arr[i].key);
			strcpy(Arr[i].var,  var);
			// printf("Arr[%d].var=%s\n",i,Arr[i].var);
		}
	}
}


static char *tokens[20];
int readConfig()
{
	FILE *dbfp;
	char line[80];
	int tknIndx = 0;
	int fd;
	char filename[60];
	
	strcpy(filename, "/root/ebdr_db.conf");
	dbfp = fopen(filename, "r");
	if(!dbfp)
	{
		ebdr_log(FATAL, "db_conf: Opening ebdr_db conf file = %s  not successful errno=%d ", filename, errno);
		ebdr_log(FATAL, "db_conf: err string is =%s ", strerror(errno));
        exit(0);
	}
	else
	{
		ebdr_log(DEBUG, "db_conf: file =%s open successful", filename);
	}
	
	while(fgets(line, 80, dbfp))
	{
		ebdr_log(DEBUG, "db_conf: line = %s", line);
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
	ebdr_log(DEBUG, "db_conf: db_user=%s db_password=%s db_host=%s\n",db_user, db_pswrd, db_host);
	return 0;
}


int db_init(char *db_name)
{  
	char str[100];
	int num_rows = 0;
	int i = 0;
	int new_sockfd;

	ebdr_log(DEBUG, "\ndb_init: Initialixing mysql_init\n");

	readConfig();

	printf("db_init: Initializing mutex lock\n");

	INITIALIZE_MUTEX(db_lock);

	pthread_mutex_lock(&db_lock); 
	
	con = mysql_init(NULL);
	if (con == NULL) 
	{
		finish_with_error(con);
	}

	ebdr_log(DEBUG, "db_init: after mysql_init\n");
  my_bool reconnect = 1;
  mysql_options(con, MYSQL_OPT_RECONNECT, &reconnect);

	if (mysql_real_connect(con, db_host,  db_user, db_pswrd, NULL, 0, NULL, 0) == NULL) 
	{
		fprintf(stderr, "%s\n", mysql_error(con));
		ebdr_log(ERROR, "db_init: error in connection\n");
		finish_with_error(con);
	}  

	ebdr_log(DEBUG, "db_init: after connection\n");

	sprintf(str, "%s %s",  "CREATE DATABASE IF NOT EXISTS", db_name); 
	if (mysql_query(con, str)) 
	{
		fprintf(stderr, "%s\n", mysql_error(con));
		finish_with_error(con);
	}

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
		recovery_mode = 1;
	}
#endif

#if CLIENT
	num_rows = mkpartner_from_db_on_client();
	printf("[db_init] num_rows partner client = %d \n", num_rows);
	if(num_rows > 0)
	{
		recovery_mode = 1;
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

	if (mysql_query(con, "CREATE TABLE IF NOT EXISTS disk(pid INT, name CHAR(15), snap_name CHAR(15), bitmap_size INT)")){
		finish_with_error(con);
	}
#if SERVER
	num_rows = mkdisk_from_db_on_server();
	printf("[db_init] num_rows disk = %d\n", num_rows);
	if(num_rows > 0)
	{
		for(i = 0; i < num_rows; i++)
		{
			printf("[db_init] ***** Recovering source disk object ***** \n");
			ebdr_disk_init(PRIMARY, ebdr_disk_src_obj[i].name, &ebdr_disk_src_obj[i]);
			snapshot_disk_init(ebdr_disk_src_obj[i].snap_name, i);
			replication_init(REP_LOCAL, i);
			replic_server_obj[i].ops->replic_obj_setup(&replic_server_obj[i], i);
			replic_hdr_server_obj[i].grain_size = all_relation_servers[i].grain_size;
			replic_hdr_server_obj[i].chunk_size = all_relation_servers[i].chunk_size;
		}

		i = MAX_CONN + 1;
		/* start server connection */
		printf("[db_init] ------ Restarting server connection from Database ------\n");
		ebdr_connection_init(EBDR_CONN_SERVER,i);
		ebdr_conn_server[i].conn_ops->do_ebdr_conn_setup(&ebdr_conn_server[i], NULL, i);
		printf("[db_init] client_connected = %d \n", client_connected);

	}
#endif


	/* resync table 09-10-14 */
	if (mysql_query(con, "CREATE TABLE IF NOT EXISTS resync(pid INT, target_disk CHAR(15), last_resynced_bit INT)")) {
		finish_with_error(con);
	}
#if CLIENT
	num_rows = mkresync_from_db_on_client();
	if(num_rows > 0)
	{
		for(i = 0; i < num_rows; i++)
		{
			/* create new client socket and connect to server */
			printf("[db_init] -------- Restarting client connection from Database -------\n");
			ebdr_connection_init(EBDR_CONN_CLIENT, i);
			new_sockfd = ebdr_conn_client[i].conn_ops->do_ebdr_conn_setup(&ebdr_conn_client[i],
					all_partner_clients[i].ip, i);
			printf("[db_init] new client sockfd = %d\n", new_sockfd);
			all_partner_clients[i].socket_fd = new_sockfd;

			replication_init(REP_REMOTE, i);
			replic_client_obj[i].ops->replic_obj_setup(&replic_client_obj[i], i);
			prepare_for_replication(ebdr_disk_target_obj[i].name, i);
			ioctl(ebdr_disk_target_obj[i].disk_fd, BLKFLSBUF, 0);
			replic_hdr_client_obj[i].grain_size = all_relation_clients[i].grain_size;
			replic_hdr_client_obj[i].chunk_size = all_relation_clients[i].chunk_size;
			request_server_for_metadata(i);
			resync_init(i);
		}

	}
#endif

	ebdr_log(DEBUG, "after use ebdrd");
	pthread_mutex_unlock(&db_lock);
}
