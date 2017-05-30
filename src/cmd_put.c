/*---------------------------------------------------------------------------
dbcmd
cmd_put.c
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
private struct
*==========================================================================*/
typedef struct _Counters
  {
  int total_items;
  int stat_failed;
  int get_info_failed;
  int read_local_failed;
  int upload_failed;
  int uploaded;
  int skip_not_file_or_dir;
  int skip_not_recursive;
  int skip_unchanged;
  int directories_could_not_be_expanded;
  } Counters;


/*==========================================================================
cmd_put_consider_and_upload
*==========================================================================*/
static void cmd_put_consider_and_upload (const char *token, 
    const CmdContext *context, 
    const char *source, const char *target, 
    Counters *counters)
  {
  BOOL dry_run = context->dry_run;

  counters->total_items++;

  log_debug ("Considering uploading %s to %s", source, target);

  BOOL doit = FALSE;

  DBStat *stat = dropbox_stat_create();
  char *error = NULL;
  dropbox_get_file_info (token, target, stat, &error);
  if (error)
    {
    log_error (error);
    counters->get_info_failed++;
    free (error);
    }
  else
    {
    if (dropbox_stat_get_type (stat) == DBSTAT_FILE)
      {
      const char *remote_hash = dropbox_stat_get_hash (stat);
      char local_hash [DBHASH_LENGTH];
      dropbox_hash (source, local_hash, &error); 
      if (error)
        {
        log_error (error);
        free (error);
        counters->read_local_failed++;
        }
      else
        {
        if (strcmp (remote_hash, local_hash) != 0)
          {
          log_debug ("Will upload, as hashes are different");
          log_info ("Uploading updated file '%s' to server", source);
          doit = TRUE;
          }
        else
          {
          log_info 
             ("Skipping file '%s' that is identical on client and server",
              source);
          counters->skip_unchanged++;
          }
        }
      }
    else 
      {
      log_info ("Uploading new file '%s' to server", source);
      log_debug ("Will upload '%s', as it does not exist on the server", 
         source);
      doit = TRUE;
      }
    dropbox_stat_destroy (stat); 
    }

  if (doit)
    {
    if (dry_run)
      {
      printf ("Source: %s\n", source);
      printf ("Destination: %s\n\n", target);
      }
    else
      {
      dropbox_upload (token, source, target, &error); 
      if (error)
        {
        log_error (error);
        free (error);
        counters->upload_failed++;
        }
      else
        {
        counters->uploaded++;
        }
      }
    }
  }


/*==========================================================================
put_one_item
*==========================================================================*/
static void put_one_item (const char *token, const CmdContext *context, 
    const char *_base, const char *_relative, const char *remote, 
    Counters *counters, BOOL remote_is_dir)
  {
  IN

  BOOL recursive = context->recursive;

  char *base = strdup (_base);
  char *relative = strdup (_relative);
  if (strcmp (relative, ".") == 0)
    relative[0] = 0;

  char *full_local;

  char *exp_base;
  if (base[strlen(base) - 1] == '/')
    exp_base = strdup (base);
  else
    asprintf (&exp_base, "%s/", base);

  asprintf (&full_local, "%s%s", exp_base, relative);

  free (exp_base);

  struct stat sb;
  if (stat (full_local, &sb) == 0)
    {
    if (S_ISREG (sb.st_mode))
      {
      // FILE 
      char *fullremote;

      if (remote_is_dir)
        asprintf (&fullremote, "%s/%s", remote, relative);
      else
        asprintf (&fullremote, "%s", remote);

      cmd_put_consider_and_upload (token, context, full_local, fullremote,
        counters);

      free (fullremote);
      }
    else if (S_ISDIR (sb.st_mode))
      {
      // DIRECTORY
      log_debug ("%s is a directory", full_local);
      if (recursive)
        {
        DIR *d = opendir (full_local);
        if (d)
          {
          struct dirent *de;
          while ((de = readdir (d)) != NULL)
            {
            const char *name = de->d_name;
            if (strcmp (name, ".") == 0) continue;
            if (strcmp (name, "..") == 0) continue;
            // TODO hidden files

            char *newrel;

            if (strlen (relative) == 0)
              {
              asprintf (&newrel, "%s", name);
              }
            else
              {
              if (relative[strlen(relative) - 1] == '/')
                relative[strlen(relative) - 1] = 0;
              asprintf (&newrel, "%s/%s", relative, name);
              }

            put_one_item (token, context, base, newrel, remote, counters, 
              remote_is_dir);

            free (newrel);
            }
          closedir (d);
          }
        else
          {
	  log_warning 
	    ("Directory %s cannot be expanded: %s", 
	      full_local, strerror (errno));
	  counters->directories_could_not_be_expanded++; 
          } 
        } 
      else
        {
	log_warning 
	  ("skipping directory %s because recursive mode was not specified ", 
	    full_local);
	counters->skip_not_recursive++; 
	}
      }
    else
      {
      // NOT REGULAR 
      log_warning ("%s in not a regular file or directory", full_local);
      counters->skip_not_file_or_dir++; 
      }
    }
  else
    {
    log_warning ("can't get attributes of %s: %s", full_local, 
      strerror (errno));
    counters->stat_failed++; 
    }

  free (full_local);
  free (base);
  free (relative);
  OUT
  }


/*==========================================================================
cmd_put_one_local_spec
*==========================================================================*/
static void cmd_put_one_local_spec (const char *token, 
    const CmdContext *context, 
    const char *local, const char *remote, Counters *counters, 
    BOOL remote_is_dir)
  {
  if (local[strlen(local) - 1] == '/')
    {
    put_one_item (token, context, local, ".", remote, counters, remote_is_dir);
    }
  else
    {
    char *abspath = realpath (local, NULL);
    char *_local = strdup (abspath);
    char *__local = strdup (abspath);
    char *filename = basename (_local);
    char *dir = dirname (__local);
    put_one_item (token, context, dir, filename, remote, counters, 
      remote_is_dir);
    free (_local);
    free (__local);
    free (abspath);
    }
  }


/*==========================================================================
cmd_put
*==========================================================================*/
int cmd_put (const CmdContext *context, int argc, char **argv)
  {
  IN
  int ret = 0;

  log_debug ("Starting put command");

  if (argc < 3)
    {
    log_error ("%s: %s: this command takes two or more arguments\n",
      NAME, argv[0]);
    OUT
    return EINVAL;
    }

  char *dest_spec = strdup (argv [argc - 1]);
  log_debug ("dest_spec is %s", dest_spec);

  if (dest_spec[0] != '/')
    {
    log_error ("%s: %s: %s\n",
      NAME, argv[0], ERROR_STARTSLASH);
    free (dest_spec);
    OUT
    return EINVAL;
    }

  BOOL remote_is_dir = FALSE;

  if (dest_spec[strlen(dest_spec) - 1] == '/')
    {
    remote_is_dir = TRUE;
    dest_spec[strlen(dest_spec) - 1] = 0;
    }

  char *error = NULL;
  char *token = token_init (&error);
  if (token)
    {
    // If the user has already indicated that the remote path is a
    //   directory, don't check it on the server
    if (remote_is_dir == FALSE)
      {
      DBStat *stat = dropbox_stat_create();
      dropbox_get_file_info (token, dest_spec, stat, &error);
      if (dropbox_stat_get_type (stat) == DBSTAT_FOLDER)
        remote_is_dir = TRUE;
      else
        remote_is_dir = FALSE;
      dropbox_stat_destroy (stat);
      }

    // This is not sufficient -- we need to expand the file list, not
    //  just count the number of arguments. If an argument contains 
    //  multiple files, it will still need the target to be a folder
    if (argc > 3 && !remote_is_dir)
      {
      log_error ("%s: %s: if multiple source files are specified, the target argument must refer to a pre-existing directory\n", NAME, argv[0]); 
      }
    else
      {
      // TODO process include/exclude 

      Counters *counters = malloc (sizeof (Counters));
      memset (counters, 0, sizeof (Counters));

      int i;
      for (i = 1; i < argc - 1; i++)
	{
	cmd_put_one_local_spec (token, context, argv[i], dest_spec, counters, 
          remote_is_dir);
	}
 
      printf ("Files considered: %d\n", counters->total_items);
      printf ("Uploaded: %d\n", counters->uploaded); 
      int total_skips = counters->skip_not_file_or_dir +  
            counters->skip_not_recursive + counters->skip_unchanged;
      if (total_skips > 0)
        {
        printf ("Skipped: %d\n", total_skips); 
        printf ("  Unchanged: %d\n", counters->skip_unchanged);
        printf ("  Not regular file/directory: %d\n", 
           counters->skip_not_file_or_dir);
        printf ("  Not expanded without recursive mode: %d\n", 
          counters->skip_not_recursive);
        }
      int total_errors = counters->stat_failed + counters->get_info_failed
         + counters->read_local_failed + counters->upload_failed + 
         counters->directories_could_not_be_expanded;
      if (total_errors)
        {
        printf ("Errors: %d\n", total_errors); 
        printf ("  Could not get local file metadata: %d\n", 
         counters->stat_failed); 
        printf ("  Could not expand local directory: %d\n", 
         counters->directories_could_not_be_expanded); 
        printf ("  Could not get remote file metadata: %d\n", 
         counters->get_info_failed); 
        printf ("  Could not read local file: %d\n", 
         counters->read_local_failed); 
        printf ("  Upload failed: %d\n", 
         counters->upload_failed); 
        }

      free (counters);
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

  free (dest_spec);
  OUT
  return ret;
  }





