/*---------------------------------------------------------------------------
dbcmd
cmd_hash.c
GPL v3.0
---------------------------------------------------------------------------*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>
#include <fnmatch.h>
#include "cJSON.h"
#include "curl.h"
#include "dropbox.h"
#include "token.h"
#include "commands.h"
#include "log.h"
#include "utils.h"
#include "list.h"

/*==========================================================================
cmd_hash
*==========================================================================*/
int cmd_hash (const CmdContext *context, int argc, char **argv)
  {
  int ret = 0;
  char *error = NULL;

  BOOL recursive = context->recursive;

  log_debug ("Starting hash command");

  if ( argc != 2)
    {
    log_error ("Usage: %s {local_file}", argv[0]); 
    return EINVAL;
    }

  char *local_file = argv[1];

  if (recursive)
    {
    log_error ("%s: recursuve mode not appropriate here)", 
      argv[0]); 
    return EINVAL;
    }

  if (strlen (local_file) == 0) 
    {
    log_error ("%s: zero-length file argument", argv[0]); 
    return EINVAL;
    }

  char hash[DBHASH_LENGTH];
  dropbox_hash (local_file, hash, &error);
  if (error)
    {
    log_error ("%s: %s", argv[1], error);
    free (error);
    ret = -1;
    }
  else
    {
    puts (hash);
    }

  return ret;
  }




