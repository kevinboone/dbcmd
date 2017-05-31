/*---------------------------------------------------------------------------
dbcmd
cmd_newfolder.c
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
cmd_newfolder
*==========================================================================*/
int cmd_newfolder (const CmdContext *context, int argc, char **argv)
  {
  IN
  int ret = 0;

  log_debug ("Starting newfolder command");

  if (argc < 2)
    {
    log_error ("%s: this command takes one or more arguments\n",
      argv[0]);
    OUT
    return EINVAL;
    }


  char *error = NULL;
  char *token = token_init (&error);
  if (token)
    {
    int i;
    for (i = 1; i < argc; i++)
      {
      char *folder = strdup (argv[i]);
      if (folder[0] == '/')
        {
        if (folder[strlen(folder) - 1] == '/')
          folder[strlen(folder) - 1] = 0;
        dropbox_newfolder (token, folder, &error); 
        if (error)
          {
          log_error ("%s: %s: %s\n", argv[0], ERROR_CREATEFOLDER, error);
          free (error);
          ret = EINVAL;
          }
        }
      else
        {
        log_error ("%s: %s\n", argv[0],  
          ERROR_STARTSLASH);
        free (error);
        }
      free (folder);
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







