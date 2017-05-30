/*===========================================================================
dbcmd
log.c
Copyright (c)2017 Kevin Boone, GPL v3.0
===========================================================================*/

#define _GNU_SOURCE 1
#include <syslog.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "log.h"
#include "bool.h"


static int log_level = DEBUG;
static BOOL log_syslog = FALSE;
static BOOL log_console = TRUE;

/*==========================================================================
logging_set_level
*==========================================================================*/
void log_set_level (const int level)
  {
  log_level = level;
  }

/*==========================================================================
logging_set_log_syslog
*==========================================================================*/
void log_set_log_syslog (const BOOL f)
  {
  log_syslog = f;
  }

/*==========================================================================
logging_set_log_console
*==========================================================================*/
void log_set_log_console (const BOOL f)
  {
  log_console = f;
  }

/*==========================================================================
log_vprintf
*==========================================================================*/
void log_vprintf (const int level, const char *fmt, va_list ap)
  {
  if (log_console)
    {
    if (level > log_level) return;
    char *str = NULL;
    vasprintf (&str, fmt, ap);
    const char *s;
    if (level == DEBUG) s = "DEBUG";
    else if (level == INFO) s = "INFO";
    else if (level == WARNING) s = "WARNING";
    else if (level == TRACE) s = "TRACE";
    else s = "ERROR";
    // I frequently put a spurious \n on the end of error messages.
    // It's easier to fix that here than in the rest of the code ;) 
    if (str[strlen(str) - 1] == '\n')
      str[strlen(str) - 1] = 0;
    printf ("%s %s\n", s, str);
    free (str);
    }
  }


/*==========================================================================
log_error
*==========================================================================*/
void log_error (const char *fmt,...)
  {
  va_list ap;
  va_start (ap, fmt);
  log_vprintf (ERROR,  fmt, ap);
  va_end (ap);
  if (log_syslog)
    {
    va_start (ap, fmt);
    vsyslog (LOG_ERR, fmt, ap);
    va_end (ap);
    }
  }


/*==========================================================================
log_warn
*==========================================================================*/
void log_warning (const char *fmt,...)
  {
  va_list ap;
  va_start (ap, fmt);
  log_vprintf (WARNING,  fmt, ap);
  va_end (ap);
  if (log_syslog)
    {
    va_start (ap, fmt);
    vsyslog (LOG_WARNING, fmt, ap);
    va_end (ap);
    }
  }


/*==========================================================================
log_info
*==========================================================================*/
void log_info (const char *fmt,...)
  {
  va_list ap;
  va_start (ap, fmt);
  log_vprintf (INFO,  fmt, ap);
  va_end (ap);
  if (log_syslog)
    {
    va_start (ap, fmt);
    vsyslog (LOG_INFO, fmt, ap);
    va_end (ap);
    }
  }


/*==========================================================================
log_debug
*==========================================================================*/
void log_debug (const char *fmt,...)
  {
  va_list ap;
  va_start (ap, fmt);
  log_vprintf (DEBUG,  fmt, ap);
  va_end (ap);
  }


/*==========================================================================
log_trace
*==========================================================================*/
void log_trace (const char *fmt,...)
  {
  va_list ap;
  va_start (ap, fmt);
  log_vprintf (TRACE,  fmt, ap);
  va_end (ap);
  }



