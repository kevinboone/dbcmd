/*---------------------------------------------------------------------------
dbcmd
cmd_get.c
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
cmd_get
*==========================================================================*/
int cmd_get (const CmdContext *context, int argc, char **argv)
  {
  int ret = 0;
  char *error = NULL;
  char *remote_spec = NULL;
  char *remote_path = NULL;
  char *local_path = NULL;
  char *pattern = NULL;

  //BOOL force = context->force;
  BOOL recursive = context->recursive;

  log_debug ("Starting get command");

  if (argc == 2 || argc == 3)
    {
    if (argc == 2)
      {
      remote_spec = strdup (argv[1]);
      local_path = strdup (".");
      }
    else
      {
      remote_spec = strdup (argv[1]);
      local_path = strdup (argv[2]);
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

    if (remote_spec [strlen (remote_spec) - 1] == '/')
      {
      char *new_remote_spec = malloc (strlen (remote_spec) + 5);
      strcpy (new_remote_spec, remote_spec);
      strcat (new_remote_spec, "*");
      free (remote_spec);
      remote_spec = new_remote_spec;
      }

    pattern = strdup (basename (remote_spec));
    char *_remote_path = dirname (remote_spec);
    remote_path = malloc (strlen (_remote_path) + 3);
    strcpy (remote_path, _remote_path);
    if (remote_path[strlen(remote_path) - 1] != '/')
      strcat (remote_path, "/"); 

    BOOL proceed = TRUE;
    BOOL local_is_dir = FALSE;

    if (utils_is_directory (local_path))
      {
      local_is_dir = TRUE;
      }
    else
      {
      if (utils_contains_wildcards (pattern))
        {
        log_error 
          ("%s: if wildcards are specified, or the source is a directory, the target argument must be an existing directory\n",
          argv[0]);
        ret = EINVAL;
        proceed = FALSE;
        } 
      }

    if (proceed)
      {
      char *token = token_init (&error);
       if (token)
	{
        List *list = list_create (dbstat_free);
        dropbox_get_file_list (token, remote_path, recursive, FALSE, list, &error); 
        if (error)
          {
	  log_error ("%s: Can't get file list from server: %s \n", 
	    argv[0], error);
	  free (error);
	  ret = EBADRQC;
          }
        else
          {
          int i, l = list_length (list);
          int matching = 0;
          int skip_error = 0;
          int skip_unchanged = 0;
          int copy_fail = 0;
          int copy_success = 0;
          log_debug ("Server returned %d file(s) in directory", l);
          for (i = 0; i < l; i++)
            {
            const DBStat *stat = list_get (list, i);
            char *path = strdup (stat->path);
            char *filename = basename (path);
            if (fnmatch (pattern, filename, 0) == 0)
              {
              // Get the local relative path by removing the remote dir part
              //   from the remote full path
              char *local_rel_path = strdup (stat->path + strlen (remote_path));
              char *full_local_path;
              if (local_is_dir)
                {
                full_local_path = malloc (strlen (local_path) + strlen (local_rel_path) + 4); 
                strcpy (full_local_path, local_path);
                if (full_local_path[strlen (full_local_path) - 1] != '/')
                  strcat (full_local_path, "/");
                strcat (full_local_path, local_rel_path); 
                }
              else
                full_local_path = strdup (local_path);

              log_info ("Remote file: %s, local file: %s", path, full_local_path);

              matching++;         
              
              char *error = NULL;
              DBCompRes res;
              dropbox_compare_files (token, full_local_path, path, &res, &error);
              if (error)
                {
                log_error ("Can't compare file details: %s", error);
                free (error); 
                skip_error++; 
                }
              else
                {
                // We managed to get local and remote file details --
                //  now check whether they mean we need to copy
                BOOL copy = FALSE;
                if (res.local_file_exists)
                  {
                  if (res.local_time < res.remote_time)
                    {
                    log_info ("File contents differ -- will download");
                    copy = TRUE;
                    }
                  else
                    {
                    //if (force)
                    //  {
                    //  log_info ("Download forced, even though local file is up to date");
                    //  copy = TRUE;
                    //  }
                    //else
                      {
                      log_info ("Files have the same contents -- will not download");
                      skip_unchanged++;
                      }
                    }
                  }
                else
                  {
                  log_info ("Will download, as local file does not exist");
                  copy = TRUE;
                  }

                if (copy)
                  {
                  utils_make_directory (full_local_path);
                  char *error = NULL;
                  log_info ("Downloading %s", path);
                  dropbox_download_file (token, path, full_local_path, &error); 
                  if (error)
                    {
                    log_error ("Can't download file: %s\n", error);
                    free (error);
                    copy_fail++;
                    }
                  else
                    copy_success++;
                  }

                }
             

              free (full_local_path);
              free (local_rel_path);
              }
            free (path);
            }
          log_info ("%d file(s) matched pattern '%s'", matching, pattern);
          log_info ("%d file(s) skipped because of error", skip_error);
          log_info ("%d file(s) skipped because cotents are identical", skip_unchanged);
          log_info ("%d file(s) could not be copied", copy_fail);
          log_info ("%d file(s) copied", copy_success); 
          }
        list_destroy (list);
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
    }
  else
    {
    log_error ("Usage: %s {remote_path} [local_path]", argv[0]); 
    ret = EINVAL;
    }

  if (remote_path) free (remote_path);
  if (remote_spec) free (remote_spec);
  if (local_path) free (local_path);
  if (pattern) free (pattern);

  return ret;
  }







