/*---------------------------------------------------------------------------
dbcmd
misc.c
GPL v3.0
---------------------------------------------------------------------------*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "cJSON.h"
#include "dropbox.h"
#include "token.h"
#include "commands.h"
#include "log.h"
#include "errmsg.h"

/*==========================================================================
misc_format_size
*==========================================================================*/
void misc_format_size (int64_t size, char **result)
  {
  if (size < 1024)
    asprintf (result, "%ld B", size);
  else if (size < 1024 * 1024)
    asprintf (result, "%.2f kB", size / 1024.0);
  else if (size < 1024 * 1024 * 1024)
    asprintf (result, "%.2f MB", size / 1024.0 / 1024.0);
  else
    asprintf (result, "%.2f GB", size / 1024.0 / 1024.0 / 1024.0);
  }


