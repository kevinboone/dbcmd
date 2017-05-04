/*---------------------------------------------------------------------------
dbcmd
cmd_info.c
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
#include "curl.h"
#include "dropbox.h"
#include "token.h"
#include "commands.h"
#include "log.h"

/*==========================================================================
cmd_info
*==========================================================================*/
int cmd_info (const CmdContext *context, int argc, char **argv)
  {
  int ret = 0;
  char *error = NULL;
  char *remote_file = NULL;
  BOOL recursive = context->recursive;

  log_debug ("Starting info command");

  if (argc == 2)
    {
    remote_file = strdup (argv[1]);

    if (strlen(remote_file) > 0 && remote_file[0] == '/') 
      {
      if (remote_file[strlen(remote_file) - 1] == '/')
        remote_file[strlen(remote_file) - 1] = 0;

      char *token = token_init (&error);
      if (token)
	{
        DBStat stat;
        dropbox_get_file_info (token, remote_file, 
          &stat, &error);

	if (error)
	  {
	  log_error ("%s: %s", argv[0], error);
	  free (error);
	  ret = -1; // TODO
	  } 
        else
          {
          if (stat.type == DBSTAT_FOLDER)
            {
            printf ("Type: folder\n");
            List *list = list_create (dbstat_free);
            dropbox_get_file_list (token, remote_file, recursive, TRUE, list, &error);
            int i, l = list_length (list);
            int dirs = 0;
            int files = 0;
            int64_t size = 0;
            for (i = 0; i < l; i++)
              {
              const DBStat *stat = list_get (list, i);
              if (stat->type == DBSTAT_FILE) 
                {
                files++;
                size += stat->length;
                }
              else if (stat->type == DBSTAT_FOLDER) 
                {
                dirs++;
                }
              }
            list_destroy (list);
            printf ("Files : %d\n", files);
            printf ("Subfolders : %d\n", dirs);
            printf ("Total size: %ld\n", size);
            }            
          else  if (stat.type == DBSTAT_FILE)
            {
            printf ("Type: file\n");
            printf ("Client modified: %s", ctime (&(stat.client_modified)));
            printf ("Server modified: %s", ctime (&(stat.server_modified)));
            printf ("Hash: %s\n", stat.hash);
            }
          else printf ("File not found\n");
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
      }
    else
      {
      log_error ("%s: path must start with '/'", 
	  argv[0]);
      ret = EINVAL;
      }
    }
  else
    {
    log_error ("Usage: %s {remote_path}", argv[0]); 
    ret = EINVAL;
    }

  if (remote_file) free (remote_file);

  return ret;
  }





