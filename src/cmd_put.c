/*---------------------------------------------------------------------------
dbcmd
cmd.put.c
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
#include "cJSON.h"
#include "curl.h"
#include "dropbox.h"
#include "token.h"
#include "commands.h"
#include "log.h"
#include "utils.h"

/*==========================================================================
cmd_get
*==========================================================================*/
int cmd_put (const CmdContext *context, int argc, char **argv)
  {
  int ret = 0;
  char *remote_pathspec = NULL;
  char *local_pathspec = NULL;
  BOOL remote_is_dir = FALSE;

  log_debug ("Starting put command");

  //BOOL force = context->force;
  BOOL recursive = context->recursive;

  if (argc != 3)
    {
    log_error ("Usage: %s {local_path} {remote_path}", argv[0]); 
    return EINVAL;
    }

  local_pathspec = argv[1];
  remote_pathspec = argv[2];
  
  if (strlen (remote_pathspec) == 0) 
    {
    log_error ("%s: zero-length remote path argument: use / for root folder", argv[0]); 
    return EINVAL;
    }

  if (remote_pathspec[0] != '/') 
    {
    log_error ("%s: remote path must begin with /", argv[0]); 
    return EINVAL;
    }

  if (remote_pathspec[strlen(remote_pathspec) - 1] == '/')
    {
    remote_is_dir = TRUE;
    }

  if (utils_contains_wildcards (local_pathspec) || 
       local_pathspec[strlen (local_pathspec) - 1] == '/')
    {
    if (!remote_is_dir)
      {
      log_error 
        ("%s: if wildcards are specified, or the source is a directory, the target argument must be a directory\n",
        argv[0]);
      return EINVAL;
      }
    }

  char *working_local_pathspec;
  if (local_pathspec[strlen(local_pathspec) - 1] == '/')
    {
    working_local_pathspec = malloc (strlen (local_pathspec) + 5);
    strcpy (working_local_pathspec, local_pathspec);
    strcat (working_local_pathspec, "*");
    }
  else
    working_local_pathspec = strdup (local_pathspec);

  char *working_local_pathspec_copy1 = strdup (working_local_pathspec);

  char *pattern = basename  (working_local_pathspec_copy1);
  char *local_dir = dirname (working_local_pathspec);

  char *working_local_dir = malloc (strlen (local_dir) + 5);
  strcpy (working_local_dir, local_dir);
  if (local_dir[strlen(local_dir) - 1] != '/')
    strcat (working_local_dir, "/");

  List *local_paths = list_create (free);
  
  char *error = NULL;
  utils_expand_path (working_local_dir, pattern, local_paths, recursive);
  
  int i, l = list_length (local_paths);
  int matching = l;
  log_debug ("Found %d file(s) in local directory", l);

  if (matching > 0)
    {
    char *token = token_init (&error);
    if (token)
      { 
      int skip_error = 0;
      int skip_unchanged = 0;
      int skip_too_large = 0;
      int copy_fail = 0;
      int copy_success = 0;
      for (i = 0; i < l; i++)
	{
	const char *local_full_path = list_get (local_paths, i);
	const char *local_rel_file = local_full_path + strlen (working_local_dir);
	const char *source_file = local_full_path;
	char *target_file;
	if (remote_pathspec[strlen(remote_pathspec) - 1] == '/')
	  {
	  target_file = malloc (strlen (remote_pathspec) 
	     + strlen (local_rel_file) + 5); 
	  strcpy (target_file, remote_pathspec);
	  strcat (target_file, local_rel_file);
	  }
	else
	  {
	  // We should have checked earlier that the source argument is
	  //  a single file, before assuming the target has the same name
	  target_file = strdup (remote_pathspec);
	  }
	log_info ("Local file: %s, remote file: %s", source_file, target_file); 

        DBCompRes res;
        dropbox_compare_files (token, source_file, target_file, &res, &error);
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
     
          if (res.remote_file_exists)
            {
            if (strncmp (res.local_hash, res.remote_hash, DBHASH_LENGTH) != 0)
              {
              log_info ("File contents differ -- will upload");
              copy = TRUE;
              }
            else
              {
              log_info ("Files have the same contents -- will not upload");
              skip_unchanged++;
              }
            }
          else
            {
            log_info ("Will upload, as remote file does not exist");
            copy = TRUE;
            }

          if (copy)
            {
            struct stat sb;
            stat (source_file, &sb);
            if (sb.st_size < 150000000)
              {
	      log_info ("Uploading %s", source_file);
	      dropbox_upload_file (token, source_file, target_file, &error); 
	      if (error)
		{
		log_error ("Can't upload file: %s", error);
		free (error);
		copy_fail++;
		}
	      else
		copy_success++;
              }
            else
              {
              log_error ("Can't upload file '%s' because it is too large", source_file);
              skip_too_large ++;
              }
            }
          }

	free (target_file);
	}
      free (token);

      log_info ("%d file(s) matched pattern '%s'", matching, pattern);
      log_info ("%d file(s) skipped because of error", skip_error);
      log_info ("%d file(s) skipped because they are too large", skip_too_large);
      log_info ("%d file(s) skipped because contents are identical", skip_unchanged);
      log_info ("%d file(s) could not be copied", copy_fail);
      log_info ("%d file(s) copied", copy_success); 
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
    log_error ("No local files to copy");
    }

  list_destroy (local_paths);

  free (working_local_dir);
  free (working_local_pathspec_copy1);
  free (working_local_pathspec);

  return ret;
  }





