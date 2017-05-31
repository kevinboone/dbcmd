/*---------------------------------------------------------------------------
dbcmd
cmd_move.c
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
#include "errmsg.h"



/*==========================================================================
cmd_move
*==========================================================================*/
int cmd_move (const CmdContext *context, int argc, char **argv)
  {
  IN
  int ret = 0;

  log_debug ("Starting move command");

  if (argc != 3)
    {
    log_error ("%s: this command takes two arguments\n",
      argv[0]);
    OUT
    return EINVAL;
    }

  const char *from = argv[1];
  const char *to = argv[2];

  if (from[0] != '/' || to[0] != '/')
    {
    log_error ("%s: %s\n",
      argv[0], ERROR_STARTSLASH);
    OUT
    return EINVAL;
    }


  char *error = NULL;
  char *token = token_init (&error);
  if (token)
    {
    if (context->dry_run)
      {
      printf ("Move %s to %s\n", from, to); 
      }
    else
      {
      dropbox_move (token, from, to, &error); 
      if (error)
	{
	log_error ("%s: %s: %s\n", argv[0], ERROR_MOVE, error);
	free (error);
	ret = EINVAL;
	}
      }
    free (token);
    }
  else
    {
    log_error ("%s: %s: %s", 
      argv[0], ERROR_INITTOKEN, error);
    free (error);
    ret = EBADRQC;
    }

  OUT
  return ret;
  }








