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
cmd_move
*==========================================================================*/
int cmd_move (const CmdContext *context, int argc, char **argv)
  {
  int ret = 0;
  char *error = NULL;
  char *old_path = NULL;
  char *new_path = NULL;

  //BOOL force = context->force;
  BOOL recursive = context->recursive;
  //BOOL yes = FALSE;

  log_debug ("Starting move command");

  if ( argc != 3)
    {
    log_error ("Usage: %s {old_path} {new_path}", argv[0]); 
    return EINVAL;
    }

  old_path = argv[1];
  new_path = argv[2];

  if (recursive)
    {
    log_error ("%s: recursuve mode not appropriate here (moves are always recursive)", 
      argv[0]); 
    return EINVAL;
    }

  if (strlen (old_path) == 0) 
    {
    log_error ("%s: zero-length old_path argument: use / for root folder", argv[0]); 
    return EINVAL;
    }

  if (strlen (new_path) == 0) 
    {
    log_error ("%s: zero-length new_path argument: use / for root folder", argv[0]); 
    return EINVAL;
    }

  if (old_path[strlen (old_path) - 1] == '/')
    old_path[strlen (old_path) - 1] = 0; 
      
  if (new_path[strlen (new_path) - 1] == '/')
    new_path[strlen (new_path) - 1] = 0; 

  if (old_path[0] != '/' || new_path[0] != '/') 
    {
    log_error ("%s: paths must begin with /", argv[0]); 
    return EINVAL;
    }

  log_debug ("Move %s to %s", old_path, new_path);

  char *token = token_init (&error);
  if (token)
    {
    dropbox_move (token, old_path, new_path, &error);

     if (error)
       {
       log_error ("%s: %s", argv[0], error);
       free (error);
       ret = -1; // TODO
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

  return ret;
  }









