CC=gcc
CFLAGS+=-g -DLINUX 
LDFLAGS+=-pthread -lm
LARGEFILE_FLAG+=-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 
src:=ebdr_daemon.c\
	ebdr_conn.c	ebdr_conn_server.c ebdr_conn_client.c\
	ebdr_partner.c ebdr_partner_server.c ebdr_partner_client.c\
	ebdr_replication_header.c ebdr_replic_header_server.c ebdr_replic_header_client.c\
	ebdr_relation.c	ebdr_relation_server.c ebdr_relation_client.c\
	ebdr_disk.c	ebdr_disk_source.c ebdr_disk_target.c\
	ebdr_replication.c ebdr_replication_server.c ebdr_replication_client.c\
	ebdr_bitmap_user.c ebdr_bitmap_server.c ebdr_bitmap_client.c\
	ebdr_io_engine.c ebdr_io_engine_server.c ebdr_io_engine_client.c\
	ebdr_ip.c ebdr_ip_server.c ebdr_ip_client.c\
	ebdr_resync.c replic_server.c replic_client.c ebdr_db.c ebdr_recovery.c ebdr_utils.c

objs:=ebdr_daemon.o\
	ebdr_conn.o	ebdr_conn_server.o ebdr_conn_client.o\
	ebdr_partner.o ebdr_partner_server.o ebdr_partner_client.o\
	ebdr_replication_header.o ebdr_replic_header_server.o ebdr_replic_header_client.o\
	ebdr_relation.o ebdr_relation_server.o ebdr_relation_client.o\
	ebdr_disk.o ebdr_disk_source.o ebdr_disk_target.o\
	ebdr_replication.c ebdr_replication_server.o ebdr_replication_client.o\
	ebdr_bitmap_user.o ebdr_bitmap_server.o ebdr_bitmap_client.o\
	ebdr_io_engine.o ebdr_io_engine_server.o ebdr_io_engine_client.o\
	ebdr_ip.o ebdr_ip_server.o ebdr_ip_client.o\
	ebdr_resync.o replic_server.o replic_client.o ebdr_db.o ebdr_recovery.o ebdr_utils.o

all: ebdrds ebdrdc ebdradm

ebdrds:
	$(CC) -c -D_GNU_SOURCE -DSERVER ${CFLAGS} ${LDFLAGS} ${LARGEFILE_FLAG} ${src}  `mysql_config --cflags --libs`
	$(CC) ${CFLAGS} ${LDFLAGS} -o ebdrds ${objs}  `mysql_config --cflags --libs`

ebdrdc: 
	$(CC) -c -D_GNU_SOURCE -DCLIENT ${CFLAGS} ${LDFLAGS} ${LARGEFILE_FLAG} ${src}  `mysql_config --cflags --libs`
	$(CC) ${CFLAGS} ${LDFLAGS} -o ebdrdc ${objs}  `mysql_config --cflags --libs`

ebdradm:
	$(CC) ${CFLAGS} -o ../../utils/ebdr_linux/ebdradm ../../utils/ebdr_linux/ebdr_adm.c 
	
clean:
	rm ebdrds ebdrdc ../../utils/ebdr_linux/ebdradm *.o
