#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>


#include "ebdr_replication_header.h"
#include "ebdr_log.h"
#include "ebdr_disk.h"
#include "ebdr_grain_req.h"
#include "ebdr_daemon.h"
#include "ebdr_ioctl_cmds.h"


pid_t ebdr_exec_redirect(
        char *path, char * argn,
        int new_in, int new_out, int new_err
)   
{

pid_t child;

  // Fork new process
  child = fork();
  if (child == 0) {
      char *argv[] = {path, argn, NULL };
      // Child process
      // Redirect standard input
     if (new_in >= 0) {
          close(STDIN_FILENO);
          dup2(new_in, STDIN_FILENO);
     }   

     // Redirect standard output
     if (new_out >= 0) {
         close(STDOUT_FILENO);
         dup2(new_out, STDOUT_FILENO);
     }   
     
     // Redirect standard error
     if (new_err >= 0) {
         close(STDERR_FILENO);
         dup2(new_err, STDERR_FILENO);
     }   
     
     // Execute the command
     execv(path, (char * const *)argv);
  }
  // Parent process
  return child;
 }



//md5 is inout argument
int get_last_md5_values(int fd, replic_header *rep_hdr_rcvd, char *md5 )
{
  char md5_local[64], tmp[64];
  FILE *fp = NULL;
  int pid = rep_hdr_rcvd->partner_id;
  int ret = 0;
  off_t x;

  memset(md5_local, '\0', sizeof(md5_local));
  memset(tmp, '\0', sizeof(tmp));

  fp = fdopen(fd, "w+");
  if(fp == NULL)
  {
    goto out;
  }

  x = lseek(fd, 0, SEEK_SET);

  if(x < 0)
  {
    ebdr_log(INFO, "[chunk_seek]lseek error [%d]\n", errno);
    goto out;
  }

  // Fetches the md5 of the last line of the replic sync that has happened for that pid
  while(!feof(fp)) {
    fscanf(fp, "%s%s", md5_local, tmp);
    if(errno != 0)
    {
      ret = -1;
      goto out;
    }
  }

  if(md5 == NULL) {
    //NULL Only when server is calling it 
    strncpy(rep_hdr_rcvd->md5, md5_local, 64);
    ebdr_log(INFO, "%s %s\n", rep_hdr_rcvd->md5 ,ebdr_disk_src_obj[pid].snap_name);
  }
  else {
    // is being called @ ebdrdc client
    strcpy(md5, md5_local);
    ebdr_log(INFO, "%s %s\n", replic_hdr_client_obj[pid].md5 ,ebdr_disk_src_obj[pid].snap_name);
  }

  close(fd);


out:
  return ret;
}


int ebdr_log_setup(int log_pid, char *file_name)
{
  int ret = 0;
  thread_params[log_pid].ebdrlog_fp = fopen(file_name, "a+");
  
  thread_params[log_pid].pid = log_pid;
  thread_params[log_pid].logging_level = 4;
  strcpy(thread_params[log_pid].logStrng, "DEBUG");
  strcpy(log_path, tmp_log_path);

  return ret;
}

int read_from_fifo(int fifo_fd)
{
	int bytes;

	bytes = read(fifo_fd, str, sizeof(str));

	if (bytes < 0)
	{
		perror("read from FIFO failed");
		close(fifo_fd);
		return -1;
	}
	return bytes;
}



int open_fd_for_ioctl(void)
{
	ioctl_fd = open(EBDR_CTL_DEVICE, O_RDWR);
	if(ioctl_fd < 0)
	{
		ebdr_log(FATAL, "File cannot open %s\n", EBDR_CTL_DEVICE);
		stop_work("[open_fd_for_ioctl] open failed ");
	}
	return 0;
}

void create_fifo(void)
{
	printf("inside create_Fifo\n");
	mkfifo(serverfifo, 0666);
	mkfifo(clientfifo, 0666);
	printf("after create_Fifo\n");
}
