#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <getopt.h>

#include "ebdr_fifo.h"
#include "ebdr_log.h"

__thread FILE *ebdrlog_fp;

char program_name[]="ebdradm";

time_t time_raw_format;
struct tm * ptr_time;
char date[20];
char timee[20];

static struct option long_options[] =
{
	{"serverlog", no_argument, NULL, 'a'},
	{"clientlog", no_argument, NULL, 'b'},
	{"create", no_argument, NULL, 'c'},
	{"disk", no_argument, NULL, 'd'},
	{"grain", no_argument, NULL, 'g'},
	{"halt", no_argument, NULL, 'h'},
	{"metadata", no_argument, NULL, 'm'},
	{"mkpartner", no_argument, NULL, 'p'},
	{"rmpartner", no_argument, NULL, 'k'},
	{"listps", no_argument, NULL, 'e'},
	{"listpc", no_argument, NULL, 'f'},
	{"listrs", no_argument, NULL, 'i'},
	{"listrc", no_argument, NULL, 'j'},
	{"mkrelation", no_argument, NULL, 'x'},
	{"mkrelationpeer", no_argument, NULL, 'y'},
	{"rmrelationpeer", no_argument, NULL, 'o'},
	{"resync", no_argument, NULL, 'r'},
	{"pause", no_argument, NULL, 'n'},
	{"resume", no_argument, NULL, 'l'},
	{"snap", no_argument, NULL, 's'},
	{"help", no_argument, NULL, 'u'},
	{"version", no_argument, NULL, 'v'},
	{"wait", no_argument, NULL, 'w'},
	{"dbinfo", no_argument, NULL, 'z'},
	{NULL, 0, NULL, 0}
};


int usage(int status)
{
	if (status != 0)
	{
		fprintf(stderr, "Try '%s --help' for more information.\n", program_name);
	} else {
		printf("--bgresync (b)\t begins resync\n");
		printf("--create (c)\t creates block device\n");
		printf("--disk (d)\t to set target disk\n");
		printf("--grain (g) \t to set grain size\n");
		printf("--metadata (m) \t to start recieving metadata\n");
		printf("--mkpartner (p) \t make partner with server\n");
		printf("--rmpartner (k) \t remove partner with server\n");
		printf("--listps (e) \t List All Server Partnerships\n");
		printf("--listpc (f) \t List All Client Partnerships\n");
		printf("--listrs (i) \t List All Server Relationships\n");
		printf("--listrc (j) \t List All Client Relationships\n");
		printf("--mkrelation (x) \t make relationship with server\n");
		printf("--mkrelationpeer (y) \t make relationship with server\n");
		printf("--rmrelationpeer (o) \t remove relationship with server\n");
		printf("--resync (r) \t start resync\n");
		printf("--pause (n) \t pause resync\n");
		printf("--resume (l) \t resume resync\n");
		printf("--snap (s) \t begins resync\n");
		printf("--help (u) \t prints help\n");
		printf("--version (v) \t prints version information\n");
		printf("--wait (w) \t server waits for client\n");
		printf("--clinetmakerelation (x) \t client intiates make relation\n");
		printf("--servermakerelation (x) \t server intiates make relation\n");

	}
}

int write_on_server_fifo (char cmd[])
{
	int fd, bytes;
	fd = open (serverfifo, O_WRONLY);
	if (fd == -1)
	{
		perror ("server Unable to open FIFO");
		close (fd);
		exit(0);
		goto file_not_found;
	}
	bytes = write (fd, cmd, strlen (cmd));
	if (bytes == -1)
	{
		perror ("Write to FIFO failed");
		close (fd);
		goto write_failed;
	}
	close (fd);

	return 0;
file_not_found:return -1;
write_failed:return -3;
}

int write_on_client_fifo (char cmd[])
{
	int fd, bytes;
	fd = open (clientfifo, O_WRONLY);
	if (fd == -1)
	{
		perror ("Client Unable to open FIFO");
		close (fd);
		goto file_not_found;
	}
	bytes = write (fd, cmd, strlen (cmd));
	if (bytes == -1)
	{
		perror ("Write to FIFO failed");
		close (fd);
		goto write_failed;
	}
	close (fd);

	return 0;
file_not_found:return -1;
write_failed:return -3;
}

int main (int argc, char *argv[])
{
	int ch;
	char buf[200];
	int longindex;

	ebdrlog_fp = fopen ("/tmp/log.txt", "a");
	if (ebdrlog_fp == NULL)
	{
		perror ("error ebdrlog file\n");
		fclose (ebdrlog_fp);
		goto file_not_found;
	}

	ch = getopt_long(argc, argv, "abcdefghijklmnoprsuvwxyz", long_options, &longindex);
	switch (ch)
	{
	case 'a':
			if (argc == 4)
			{
				sprintf (buf, "aserverlog %s %s", argv[2], argv[3]);
				write_on_server_fifo (buf);
			}
			else
			{
				printf("usage: ./ebdradm --serverlog [thread id] [log level]\n");
				break;
			}
			break;
 	case 'b':
			if (argc == 4)
			{
				sprintf (buf, "bclientlog %s %s", argv[2], argv[3]);
				write_on_client_fifo (buf);
			}
			else
			{
				printf("usage: ./ebdradm --clientlog [thread id] [log level]\n");
				break;
			}
			break;


    case 'c':
			if (argc == 5)
			{
				sprintf (buf, "create %s %s %s", argv[2], argv[3], argv[4]);
				write_on_server_fifo (buf);
			}
			else
			{
				printf("usage: ./ebdradm --create [src_disk] [grain_size] [bitmap_size]\n");
				break;
			}
			break;
		case 'd':
			if (argc == 3)
			{
				sprintf (buf, "disk %s", argv[2]);
				write_on_client_fifo (buf);
			}
			else
			{
				printf("usage: ./ebdradm --disk [target_disk_name]\n");
				break;
			}
			break;
		case 'e':
			write_on_server_fifo ("e");
			break;
		case 'f':
			write_on_client_fifo ("f");
			break;
		case 'g':
			if (argc == 4)
			{
				sprintf (buf, "grainset %s %s", argv[2], argv[3]);
				write_on_server_fifo (buf);
			}
			else
			{
				printf("usage: ./ebdradm --grain [grain_size] [bitmap_size]\n");
				break;
			}
			break;
		case 'i':
			write_on_server_fifo ("i");
			break;
		case 'j':
			write_on_client_fifo ("j");
			break;
		case 'p':
			if(argc == 8)
			{
				sprintf (buf, "partner %s %s %s %s %s %s", argv[2], argv[3], argv[4], argv[5], argv[6], argv[7]);
				write_on_client_fifo (buf);
			}
			else
			{
				printf("usage: ./ebdradm --mkpartner [serverip] [bandwidth] \n");
			}
			break;
		case 'k':
			if(argc == 3)
			{
				sprintf (buf, "killpartner %s", argv[2]);
				write_on_client_fifo (buf);
			}
			else
			{
				printf("usage: ./ebdradm --rmpartner [partnerid]\n");
			}
			break;
		case 'o':
			if(argc == 3)
			{
				sprintf (buf, "ormrelation %s", argv[2]);
				write_on_client_fifo (buf);
			}
			else
			{
				printf("usage: ./ebdradm --rmrelationpeer [relationid]\n");
			}
			break;
		case 'h':
			write_on_server_fifo ("h");
			break;
		case 'm':
			if (argc == 3)
			{
				sprintf (buf, "metadata %s", argv[2]);
				write_on_client_fifo (buf);
			}
			else
			{
				printf("usage: ./ebdradm --metadata [relation_id]\n");
				break;
			}
			break;
		case 'r':
			if (argc == 3)
			{
				sprintf (buf, "resync %s", argv[2]);
				write_on_client_fifo (buf);
			}
			else
			{
				printf("usage: ./ebdradm --resync [relation_id]\n");
				break;
			}
			break;
		case 'n':
			if (argc == 3)
			{
				sprintf (buf, "npause %s", argv[2]);
				write_on_client_fifo (buf);
			}
			else
			{
				printf("usage: ./ebdradm --pause [relation_id]\n");
				break;
			}
			break;
		case 'l':
			if (argc == 3)
			{
				sprintf (buf, "lresume %s", argv[2]);
				write_on_client_fifo (buf);
			}
			else
			{
				printf("usage: ./ebdradm --resume [relation_id]\n");
				break;
			}
			break;
		case 's':
			if (argc == 4)
			{
				sprintf (buf, "snap %s %s", argv[2], argv[3]);
				write_on_server_fifo (buf);
			}
			else
			{
				printf("usage: ./ebdradm --snap [source_disk_name] [relation_id]\n");
				break;
			}
			break;
		case 'x':
			if(argc == 8)
			{
				sprintf (buf, "x %s %s %s %s %s %s ", argv[2], argv[3], argv[4], argv[5],
						argv[6], argv[7] );

				write_on_server_fifo (buf);
			}
			else
			{
				printf("usage: ./ebdradm --mkrelation [partner_id] [role] [src_disk] [bitmap_size] \n");
				printf("[grain_size] [chunk_size]\n");
			}
			break;
		case 'y':
			if(argc == 8)
			{
				sprintf (buf, "y %s %s %s %s %s %s ", argv[2], argv[3], argv[4], argv[5],
						argv[6], argv[7] );

				write_on_client_fifo (buf);
			}
			else
			{
				printf("usage: ./ebdradm --mkrelationpeer [partner_id] [role] [target_disk] [bitmap_size] \n");
				printf("[grain_size] [chunk_size]\n");
			}
			break;

		case 'u':
			printf("inside help\n");
			usage(0);
			break;
		case 'v':
			printf("EBDR V0.1 version\n");
			break;
		case 'w':
			write_on_server_fifo ("w");
			break;
    case 'z':
			if(argc == 3)
      {
        if ( *argv[2] =='c' || *argv[2] =='C'){
          write_on_server_fifo("z");
        } else {
          write_on_client_fifo("z");
        }
      } else
			{
				printf("usage: ./ebdradm -z [client | server] \n");
			}
      break;
		case '?':
			printf("usage: ./app [OPTION] {|ARG1|ARG2}\n");
			printf("c - Create eBDR Device\n");
			printf("s - Bitmap Snapshot\n");
			printf("p - Protocol Negotiation\n");
			printf("m - Get Metadata\n");
			printf("w - Server Connection Establishment\n");
			printf("n - Client Connection Establishment\n");
			printf("d - Target disk name\n");
			printf("r - Start Resync [Replication]\n");
			printf("b - Begin Resync [Migration]\n");
			printf("h - Halt Resync  [Migration]\n");
			break;
		default:
			fprintf (stderr, "Unknown option character %d\n", ch);
			goto unknown_option;
			break;
	}

	fclose (ebdrlog_fp);
	return 0;
file_not_found:return -1;
unknown_option:return -2;
}
