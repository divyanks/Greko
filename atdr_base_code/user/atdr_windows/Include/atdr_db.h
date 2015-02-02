#include <Windows.h>
#include "..\Include\sys_common.h"

char db_user[10];
char db_pswrd[10];
char db_host[10];


extern int recovery_mode[MAX_PID];
int update_into_disk(int pid, char *name, char *snap_name, char *db_name);
int insert_into_relation(int role, int pid, int rid, int state, char *disk, unsigned long int bitmap_size,
	unsigned long int grain_size, unsigned long int chunk_size, char *db_name);
int retrieve_from_resync(struct atdr_replication *replic_obj, struct atdr_disk *disk_obj);
int insert_into_disk(int pid, char *name, char *snap_name, unsigned long int bitmap_size, int state, char *db_name);
int insert_into_partner(int pid, enum partner_obj_state_t state, char *ip, unsigned long int bandwidth, char *db_name);
int retrieve_from_partner(struct partner *all_partner);
int insert_into_relation(int role, int pid, int rid, int state, char *disk, unsigned long int bitmap_size, unsigned long int grain_size, unsigned long int chunk_size, char *db_name);
int retrieve_from_resync(struct atdr_replication *replic_obj, struct atdr_disk *disk_obj);
int db_init(char *db_name);

int update_into_partner(int pid, int obj_state, char *db_name);
int update_relation_state(int pid, int state, char *db_name);
int update_disk_state(int pid, int state, char *db_name);
int update_resync_state(int pid, int state, char *status, char *db_name);
int retrieve_from_disk(struct atdr_disk *atdr_dsk);