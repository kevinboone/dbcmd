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
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <fnmatch.h>
#include "cJSON.h"
#include "dropbox.h"
#include "token.h"
#include "commands.h"
#include "log.h"
#include "errmsg.h"


/*==========================================================================
cmd_delete_item
*==========================================================================*/
int cmd_delete_item (const CmdContext *context, 
     const char *token, const char *path)
  {
  int ret = 0;
  log_debug ("delete path %s", path);
  if (context->dry_run)
    {
    printf ("Delete %s\n", path);
    }
  else
    {
    char *error = NULL;
    dropbox_delete (token, path, &error);
    if (error)
      {
      log_error ("%s: %s: %s", NAME, "delete", error);
      free (error);
      ret = EINVAL;
      } 
    }
    
  return ret;
  }


/*==========================================================================
cmd_delete_prompt_delete_file 
*==========================================================================*/
int cmd_delete_prompt_delete_file (const CmdContext *context, 
     const char *token, const char *path) 
  {
  IN
  int ret = 0;
  log_debug ("prompt delete file %s", path);
  BOOL doit = FALSE;
  if (context->yes)
    doit = TRUE;
  else
    {
    printf ("Delete this file? (y/N) ");
    int c = getchar();
    if (c == 'y' || c == 'Y')
      doit = TRUE;
    }

  if (doit)
    cmd_delete_item (context, token, path);

  return ret;
  OUT
  }


/*==========================================================================
cmd_delete_prompt_delete_folder
*==========================================================================*/
int cmd_delete_prompt_delete_folder (const CmdContext *context, 
     const char *token, const char *path) 
  {
  IN
  int ret = 0;
  log_debug ("prompt delete folder %s", path);
  BOOL doit = FALSE;
  if (context->yes)
    doit = TRUE;
  else
    {
    printf ("Delete this folder AND ALL CONTENTS? (y/N) ");
    int c = getchar();
    if (c == 'y' || c == 'Y')
      doit = TRUE;
    }

  if (doit)
    cmd_delete_item (context, token, path);

  return ret;
  OUT
  }


/*==========================================================================
cmd_delete_prompt_delete_list 
*==========================================================================*/
int cmd_delete_prompt_delete_list (const CmdContext *context, 
     const char *token, const char *_path)
  {
  IN
  int ret = 0;
  char *path = strdup (_path);
  log_debug ("prompt delete list %s", path);

  BOOL recursive = context->recursive;
  BOOL yes = context->yes;

  char *p = strrchr (path, '/');
  char *spec = NULL;
  char *dir = NULL;
  if (p)
    {
    *p = 0; 
    spec = strdup (p+1);
    dir = strdup (path);
    }
  else
    {
    // This can't happen, because there will always be at least
    //   the initial / character
    }
 
  log_debug ("dir=%s, spec=%s", dir, spec);

  char *error = NULL;
  List *list = dropbox_stat_create_list();
  dropbox_list_files (token, dir, list, FALSE, recursive, &error);

  if (error)
    {
    log_error ("%s: %s: %s", NAME, "delete", error);
    free (error);
    ret = EINVAL;
    } 
  else
    {
    List *globbed_list = list_create (free);
       
    int i, l = list_length (list);
    for (i = 0; i < l; i++)
      {
      const DBStat *stat = list_get (list, i);
      const char *path = dropbox_stat_get_path (stat); 
      char *pathcopy = strdup (path);
      char *filename = basename (pathcopy);
        // Not sure about this logic
      if ((fnmatch (spec, path, 0) == 0)
          || (fnmatch (spec, filename, 0) == 0))
        list_append (globbed_list, strdup (path)); 
      free (pathcopy);
      } 
      
    l = list_length (globbed_list);
    if (l == 0)
      {
      log_error ("%s: %s: no matching items to delete\n", NAME, "delete");
      ret = EINVAL;
      }
    else
      {
      BOOL doit = FALSE;
  
      if (yes)
        doit = TRUE;
      else
        {
        printf ("Delete %d item(s)? (y/N)", l); 
        int c = getchar();
        if (c == 'y' || c == 'Y')
          doit = TRUE;
        }

      if (doit)
        {
	for (i = 0; i < l; i++)
	  {
	  const char *remote_path = list_get (globbed_list, i);
          ret = cmd_delete_item (context, token, remote_path);
          }
        }
      }

    list_destroy (globbed_list);
    }

  free (dir);
  free (spec);
  free (path);

  return ret;
  OUT  
  }


/*==========================================================================
cmd_get
*==========================================================================*/
int cmd_delete (const CmdContext *context, int argc, char **argv)
  {
  IN
  int ret = 0;

  log_debug ("Starting delete command");

  if (argc != 2)
    {
    log_error ("%s: %s: this command takes one argument\n",
      NAME, argv[0]);
    OUT
    return EINVAL;
    }

  char *remote_spec = strdup (argv [1]);
  log_debug ("remote_spec is %s", remote_spec);

  if (remote_spec[0] != '/')
    {
    log_error ("%s: %s: %s\n",
      NAME, argv[0], ERROR_STARTSLASH);
    OUT
    return EINVAL;
    }

  if (remote_spec[strlen (remote_spec) - 1] == '/')
   remote_spec[strlen (remote_spec) - 1] = 0;

  char *error = NULL;
  char *token = token_init (&error);
  if (token)
    {
    DBStat *stat = dropbox_stat_create();
    dropbox_get_file_info (token, remote_spec, stat, &error);
    if (error)
      {
      // Note that "not found" is not an error -- it is a successful
      //   call on the server, which reports a valid state. An error
      //   here will be unable to connect, etc.
      log_error (error);
      free (error);
      }
    else
      {
      switch (dropbox_stat_get_type (stat))
	{
	case DBSTAT_FILE:
	  ret = cmd_delete_prompt_delete_file 
	    (context, token, dropbox_stat_get_path (stat));
	  break;
	case DBSTAT_FOLDER:
	  ret = cmd_delete_prompt_delete_folder 
	    (context, token, dropbox_stat_get_path (stat));
	  break;
	case DBSTAT_NONE:
	  // Expand file list, as we might match wildcards
	  ret = cmd_delete_prompt_delete_list 
	    (context, token, remote_spec);
	  break;
	}
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

  free (remote_spec);

  OUT
  return ret;
  }

