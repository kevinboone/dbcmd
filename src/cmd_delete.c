/*---------------------------------------------------------------------------
dbcmd
cmd_delete.c
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
cmd_delete
*==========================================================================*/
int cmd_delete (const CmdContext *context, int argc, char **argv)
  {
  int ret = 0;
  char *error = NULL;
  char *remote_spec = NULL;

  //BOOL force = context->force;
  BOOL recursive = context->recursive;
  BOOL yes = context->yes;

  remote_spec = strdup (argv[1]);
  if (remote_spec[strlen (remote_spec) - 1] == '/')
    remote_spec[strlen (remote_spec) - 1] = 0; 


  log_debug ("Starting delete command");

  if ( argc != 2)
    {
    log_error ("Usage: %s {remote_path}", argv[0]); 
    return EINVAL;
    }

  if (recursive)
    {
    log_error ("%s: recursuve mode not appropriate here (deletes are always recursive)", 
      argv[0]); 
    return EINVAL;
    }

  if (strlen (remote_spec) == 0) 
    {
    log_error ("%s: zero-length remote path argument: use / for root folder", argv[0]); 
    return EINVAL;
    }

  if (remote_spec[0] != '/') 
    {
    log_error ("%s: remote path must begin with /", argv[0]); 
    return EINVAL;
    }


  char *token = token_init (&error);
  if (token)
    {
    DBStat stat;
    dropbox_get_file_info (token, remote_spec, 
      &stat, &error);

     if (error)
       {
       log_error ("%s: %s\n", argv[0], error);
       free (error);
       ret = -1; // TODO
       } 
     else
      {
      if (stat.type == DBSTAT_FOLDER)
        {
        BOOL delete = FALSE;
        if (yes)
          delete = TRUE;
        else
          {
          printf ("Delete this folder and all its contents? (y/N) ");
          int c = getchar();
          if (c == 'y' || c == 'Y')
            delete = TRUE;
          }
        if (delete) 
          {
          char *error = NULL;
          dropbox_delete (token, remote_spec, &error);
          if (error)
            {
            log_error ("%s: can't delete: %s", argv[0], error);
            free (error);
            ret = -1; // TODO 
            }
          }
        }
      else  if (stat.type == DBSTAT_FILE)
        {
        char *error = NULL;
        dropbox_delete (token, remote_spec, &error);
        if (error)
          {
          log_error ("%s: can't delete: %s", argv[0], error);
          free (error);
          ret = -1; // TODO 
          }
        }
      else log_error ("%s: not found: %s", argv[0], remote_spec);
      }
    free (token);
    }
  else
    {
    log_error ("%s: Can't initialize access token: %s", 
	  argv[0], error);
    free (error);
    ret = EBADRQC;
    }

  if (remote_spec) free (remote_spec);

  return ret;
  }








