/*==========================================================================
dbcmd
log.h
Copyright (c)2017 Kevin Boone, GPLv3.0
*==========================================================================*/

#pragma once

#include "bool.h"

#define ERROR 0
#define WARNING 1
#define INFO 2
#define DEBUG 3
#define TRACE 4 

#define IN log_trace("entering %s",__PRETTY_FUNCTION__);
#define OUT log_trace("leaving %s",__PRETTY_FUNCTION__);


void log_error (const char *fmt,...);
void log_warning (const char *fmt,...);
void log_info (const char *fmt,...);
void log_debug (const char *fmt,...);
void log_trace (const char *fmt,...);
void log_set_level (const int level);

void log_set_log_syslog (const BOOL f);
void log_set_log_console (const BOOL f);






