#include <stdio.h>
#include <fcntl.h>

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <Windows.h>

#include "..\..\user\ebdr_windows\Include\ebdr_fifo.h"
#include "..\..\user\ebdr_windows\Include\ebdr_log.h"

FILE *ebdrlog_fp;

char program_name[] = "ebdr_adm";

time_t time_raw_format;
struct tm * ptr_time;
char date[20];
char timee[20];


#define CMD_MAX_LEN 80

typedef struct cmdFun
{
	char cmd[CMD_MAX_LEN];
	char ch;
} cmdFun_t;

cmdFun_t cmdMap[] =
{
	{ "--serverlog", 'a' },
	{ "--clientlog", 'b' },
	{ "--create", 'c' },
	{ "--disk", 'd' },
	{ "--grain", 'g' },
	{ "--halt", 'h' },
	{ "--metadata", 'm' },
	{ "--mkpartner", 'p' },
	{ "--fullresync", 'q' },
	{ "--rmpartner", 'k' },
	{ "--listps", 'e' },
	{ "--listpc", 'f' },
	{ "--listrs", 'i' },
	{ "--listrc", 'j' },
	{ "--mkrelation", 'x' },
	{ "--mkrelationpeer", 'y' },
	{ "--rmrelationpeer", 'o' },
	{ "--resync", 'r' },
	{ "--pause", 'n' },
	{ "--resume", 'l' },
	{ "--snap", 's' },
	{ "--help", 'u' },
	{ "--version", 'v' },
	{ "--wait", 'w' },
	{ "--dbinfo", 'z' }
};

int usage(int status)
{
	if (status != 0)
	{
		fprintf(stderr, "Try '%s --help' for more information.\n", program_name);
	}
	else {
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
	return 0;
}

int write_on_server_fifo(char cmd[])
{
	HANDLE hPipe;

	//Connect to the server pipe using CreateFile()
	hPipe = CreateFile(
		SERVER_FIFO,   // pipe name 
		GENERIC_READ |  // read and write access 
		GENERIC_WRITE,
		0,              // no sharing 
		NULL,           // default security attributes
		OPEN_EXISTING,  // opens existing pipe 
		0,              // default attributes 
		NULL);          // no template file 

	if (INVALID_HANDLE_VALUE == hPipe)
	{
		printf("\nError occurred while connecting to the server: %d", GetLastError());
		//One might want to check whether the server pipe is busy
		//This sample will error out if the server pipe is busy
		//Read on ERROR_PIPE_BUSY and WaitNamedPipe() for that
		return 1;  //Error
	}
	else
	{
		printf("\nCreateFile() was successful.");
	}

	//We are done connecting to the server pipe, 
	//we can start communicating with the server using ReadFile()/WriteFile() 
	//on handle - hPipe

	DWORD cbBytes;

	//Send the message to server
	BOOL bResult = WriteFile(
		hPipe,                // handle to pipe 
		cmd,             // buffer to write from 
		CMD_BUFFER_LEN,   // number of bytes to write, include the NULL
		&cbBytes,             // number of bytes written 
		NULL);                // not overlapped I/O 

	if ((!bResult))
	{
		printf("\nError occurred while writing to the server: %d", GetLastError());
		CloseHandle(hPipe);
		return 1;  //Error
	}
	else
	{
		printf("\nWriteFile() was successful.");
		DisconnectNamedPipe(hPipe);
		bResult = CloseHandle(hPipe);
		if (!bResult)
		{
			printf("unable to close the pipe %d\n", GetLastError());
		}
		else
			printf("Fifo close successful\n");

	}
	return 0;
}

int write_on_client_fifo(char cmd[])
{
	HANDLE hPipe;

	//Connect to the server pipe using CreateFile()
	hPipe = CreateFile(
		CLIENT_FIFO,   // pipe name 
		GENERIC_READ |  // read and write access 
		GENERIC_WRITE,
		0,              // no sharing 
		NULL,           // default security attributes
		OPEN_EXISTING,  // opens existing pipe 
		0,              // default attributes 
		NULL);          // no template file 

	if (INVALID_HANDLE_VALUE == hPipe)
	{
		printf("\nError occurred while connecting to the server: %d", GetLastError());
		//One might want to check whether the server pipe is busy
		//This sample will error out if the server pipe is busy
		//Read on ERROR_PIPE_BUSY and WaitNamedPipe() for that
		return 1;  //Error
	}
	else
	{
		printf("\nCreateFile() was successful.");
	}

	//We are done connecting to the server pipe, 
	//we can start communicating with the server using ReadFile()/WriteFile() 
	//on handle - hPipe

	DWORD cbBytes;

	//Send the message to server
	BOOL bResult = WriteFile(
		hPipe,                // handle to pipe 
		cmd,             // buffer to write from 
		CMD_BUFFER_LEN,   // number of bytes to write, include the NULL
		&cbBytes,             // number of bytes written 
		NULL);                // not overlapped I/O 

	if (!bResult)
	{	
		printf("\nError occurred while writing to the server: %d", GetLastError());
		CloseHandle(hPipe);
		return 1;  //Error
	}
	else
	{
		printf("\nWriteFile() was successful.");
		bResult = CloseHandle(hPipe);
		if (!bResult)
		{
			printf("unable to close the pipe %d\n", GetLastError());
		}
		else
			printf("Fifo close successful\n");
	}
	return 0;

}

int main(int argc, char *argv[])
{
	char ch;
	char buf[200];
	
	int i = 0;

	ebdrlog_fp = fopen("C:\\log.txt", "a+");
	if (ebdrlog_fp == NULL)
	{
		perror("error ebdrlog file\n");
		goto file_not_found;
	}
	if (argc < 2)
	{
		printf("Usage: ./ebdradm [command] try giving --help");
		exit(-2);
	}
	for (i = 0; i < sizeof(cmdMap) / sizeof(cmdFun_t); i++)
	{
		if (strcmp(argv[1], cmdMap[i].cmd) == 0)
		{
			ch = cmdMap[i].ch;
			printf(" ch=%d\n", ch);
break;
		}
	}
	switch (ch)
	{
	case 'a':
		if (argc == 4)
		{
			sprintf(buf, "aserverlog %s %s", argv[2], argv[3]);
			write_on_server_fifo(buf);
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
			sprintf(buf, "bclientlog %s %s", argv[2], argv[3]);
			write_on_client_fifo(buf);
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
			sprintf(buf, "create %s %s %s", argv[2], argv[3], argv[4]);
			write_on_server_fifo(buf);
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
			sprintf(buf, "disk %s", argv[2]);
			write_on_client_fifo(buf);
		}
		else
		{
			printf("usage: ./ebdradm --disk [target_disk_name]\n");
			break;
		}
		break;
	case 'e':
		write_on_server_fifo("e");
		break;
	case 'f':
		write_on_client_fifo("f");
		break;
	case 'g':
		if (argc == 4)
		{
			sprintf(buf, "grainset %s %s", argv[2], argv[3]);
			write_on_server_fifo(buf);
		}
		else
		{
			printf("usage: ./ebdradm --grain [grain_size] [bitmap_size]\n");
			break;
		}
		break;
	case 'i':
		write_on_server_fifo("i");
		break;
	case 'j':
		write_on_client_fifo("j");
		break;
	case 'p':
		if (argc == 8)
		{
			sprintf(buf, "partner %s %s %s %s %s %s", argv[2], argv[3], argv[4], argv[5], argv[6], argv[7]);
			
			write_on_client_fifo(buf);
		}
		else
		{
			printf("usage: ./ebdradm --mkpartner [serverip] [bandwidth] \n");
		}
		break;

	case 'q':
		if (argc == 3)
		{
			sprintf(buf, "%s %s","qfullresync", argv[2]);
			printf("buf=%s\n", buf);
			write_on_client_fifo(buf);
		}
		else
		{
			printf("usage: ./ebdradm --fullresync pid\n");
		}
		break;

	case 'k':
		if (argc == 3)
		{
			sprintf(buf, "killpartner %s", argv[2]);
			write_on_client_fifo(buf);
		}
		else
		{
			printf("usage: ./ebdradm --rmpartner [partnerid]\n");
		}
		break;
	case 'o':
		if (argc == 3)
		{
			sprintf(buf, "ormrelation %s", argv[2]);
			write_on_client_fifo(buf);
		}
		else
		{
			printf("usage: ./ebdradm --rmrelationpeer [relationid]\n");
		}
		break;
	case 'h':
		write_on_server_fifo("h");
		break;
	case 'm':
		if (argc == 3)
		{
			sprintf(buf, "metadata %s", argv[2]);
			write_on_client_fifo(buf);
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
			sprintf(buf, "resync %s", argv[2]);
			write_on_client_fifo(buf);
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
			sprintf(buf, "npause %s", argv[2]);
			write_on_client_fifo(buf);
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
			sprintf(buf, "lresume %s", argv[2]);
			write_on_client_fifo(buf);
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
			sprintf(buf, "snap %s %s", argv[2], argv[3]);
			write_on_server_fifo(buf);
		}
		else
		{
			printf("usage: ./ebdradm --snap [source_disk_name] [relation_id]\n");
			break;
		}
		break;
	case 'x':
		if (argc == 8)
		{
			sprintf(buf, "x %s %s %s %s %s %s ", argv[2], argv[3], argv[4], argv[5],
				argv[6], argv[7]);

			write_on_server_fifo(buf);
		}
		else
		{
			printf("usage: ./ebdradm --mkrelation [partner_id] [role] [src_disk] [bitmap_size] \n");
			printf("[grain_size] [chunk_size]\n");
		}
		break;
	case 'y':
		if (argc == 8)
		{
			sprintf(buf, "y %s %s %s %s %s %s ", argv[2], argv[3], argv[4], argv[5],
				argv[6], argv[7]);

			write_on_client_fifo(buf);
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
		write_on_server_fifo("w");
		break;
	case 'z':
		if (argc == 3)
		{
			if (*argv[2] == 'c' || *argv[2] == 'C'){
				write_on_server_fifo("z");
			}
			else {
				write_on_client_fifo("z");
			}
		}
		else
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
		fprintf(stderr, "Unknown option character %d\n", ch);
		goto unknown_option;
		break;
	}

	fclose(ebdrlog_fp);
	return 0;
file_not_found:return -1;
unknown_option:return -2;
}
