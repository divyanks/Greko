#ifndef __EBDR_LOGGING_H
#define __EBDR_LOGGING_H
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include "ebdr_daemon.h"

#define FATAL   1
#define ERROR   2
#define WARN    3
#define INFO    4
#define DEBUG   5

#define LEVEL 5


extern time_t time_raw_format;
extern struct tm * ptr_time;
extern char date[20];
extern char timee[20];

#define ebdr_log(prio,  msg, ...) do {\
  int logging_level = thread_params[log_pid].logging_level; \
  FILE *ebdrlog_fp = thread_params[log_pid].ebdrlog_fp; \
  char *logStrng =  thread_params[log_pid].logStrng;\
  time( &time_raw_format );\
  ptr_time = localtime ( &time_raw_format );\
  if(strftime(date,50,"%Y-%m-%d",ptr_time) == 0){\
    perror("Couldn't prepare formatted string");\
  }\
  if(strftime(timee,10,"%T",ptr_time) == 0){\
    perror("Couldn't prepare formatted string");\
  }\
  if(prio < logging_level || prio == logging_level)\
  if (prio == FATAL )\
  logStrng = "FATAL";\
  else if (prio == ERROR )\
  logStrng = "ERR";\
  else if (prio == WARN )\
  logStrng = "WARN";\
  else if (prio == INFO )\
  logStrng = "INFO";\
  else if (prio == DEBUG)\
  logStrng = "DEBUG";\
  if(strcmp(logStrng, "temp") != 0) {\
    printf(" %s : %d : %s : %s : [%s]: "msg" ", \
        __FILE__, __LINE__, date, timee, logStrng,  ##__VA_ARGS__);\
    fprintf(ebdrlog_fp, " %s : %d : %s : %s [%s]: "msg" ",\
        __FILE__, __LINE__, date, timee, logStrng,  ##__VA_ARGS__);\
    fflush(ebdrlog_fp);\
  }\
} while (0)
#endif
