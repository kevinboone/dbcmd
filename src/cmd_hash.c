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
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include "cJSON.h"
#include "dropbox.h"
#include "token.h"
#include "commands.h"
#include "log.h"



/*==========================================================================
cmd_hash
*==========================================================================*/
int cmd_hash (const CmdContext *context, int argc, char **argv)
  {
  IN
  int ret = 0;

  log_debug ("Starting hash command");

  if (argc < 2)
    {
    log_error ("%s: %s: this command takes one or more arguments\n",
      NAME, argv[0]);
    OUT
    return EINVAL;
    }


  //char *error = NULL;
  //char *token = token_init (&error);
  //if (token)
    {
    int i;
    for (i = 1; i < argc; i++)
      {
      char hash [DBHASH_LENGTH];
      char *error = NULL;
      ret = dropbox_hash (argv[i], hash, &error); 
      if (error)
        {
        log_error ("%s: %s: %s\n", NAME, argv[0], error);
        free (error);
        }
      else
        {
        printf ("%s: %s\n", argv[i], hash);
        }
      }
    //free (token);
    }
  //else
  //  {
  //  log_error ("%s: Can't initialize access token: %s", 
  //    argv[0], error);
  //  free (error);
  //  ret = EBADRQC;
  //  }

  OUT
  return ret;
  }






