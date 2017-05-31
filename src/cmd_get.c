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
private struct
*==========================================================================*/
typedef struct _Counters
  {
  int total_items;
  int get_info_failed;
  int skip_unchanged;
  int download_failed;
  int downloaded;
  } Counters;


/*==========================================================================
Forward
*==========================================================================*/
static void cmd_get_make_directory (const char *_filename);


/*==========================================================================
cmd_get_consider_and_download
*==========================================================================*/
static void cmd_get_consider_and_download (const char *token, 
    const CmdContext *context, 
    const char *source, const char *target, 
    Counters *counters, const char *argv0)
  {
  BOOL dry_run = context->dry_run;

  counters->total_items++;

  log_debug ("Considering downloading %s to %s", source, target);

  BOOL doit = FALSE;

  // The local file need not exist, so we must create it. If it does
  //  exist, we need to check its hash against that of the server file

  if (access (target, R_OK) == 0)
    {
    // Local exists -- check hashes
    char *error = NULL;
    DBStat *stat = dropbox_stat_create();
    dropbox_get_file_info (token, source, stat, &error);
    if (error)
      {
      // Should never happen, unless someone pulls the plug mid-operation
      log_error ("%s: %s: %s", argv0, ERROR_CANTINFOSERVER, error);
      counters->get_info_failed++;
      free (error);
      }
    else
      {
      const char *remote_hash = dropbox_stat_get_hash (stat);
      char local_hash [DBHASH_LENGTH];
      dropbox_hash (target, local_hash, &error); // We ignore error here
      if (strcmp (local_hash, remote_hash) == 0)
        {
        log_info ("Not downloading unchanged file '%s'", source);
        counters->skip_unchanged++;
        doit = FALSE;
        }
      else
        {
        log_info ("Downloading updated file '%s'", source);
        doit = TRUE;
        }
      }
     dropbox_stat_destroy (stat);
     }
  else
    {
    doit = TRUE;
    log_info ("Downloading '%s' because local file does not exist", source);
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
      char *error = NULL;
      cmd_get_make_directory (target);
      dropbox_download (token, source, target, &error); 
      if (error)
        {
        log_error ("%s: %s: %s", argv0, ERROR_DOWNLOAD, error);
        free (error);
        counters->download_failed++;
        }
      else
        {
        counters->downloaded++;
        }
      }
    }
  }


/*==========================================================================
cmd_get_one_remote_spec
*==========================================================================*/
static void cmd_get_one_remote_spec (const char *token, 
    const CmdContext *context, const char *_remote, 
    const char *_local, Counters *counters, BOOL local_is_dir, 
    const char *argv0)
  {
  BOOL recursive = context->recursive;

  char *error = NULL;
  char *remote = strdup (_remote);
  char *path;

  BOOL remote_ended_slash = FALSE;
  if (remote[strlen(remote) - 1] == '/')
    {
    remote_ended_slash = TRUE; 
    remote[strlen(remote) - 1] = 0;
    }

  char *local = strdup (_local);
  if (local_is_dir)
    {
    if (local[strlen(local) - 1] == '/')
      local[strlen(local) - 1] = 0;
    }

  DBStat *stat = dropbox_stat_create();
  dropbox_get_file_info (token, remote, 
    stat, &error);

  if (error)
    {
    log_error ("%s: %s: %s", "get", ERROR_CANTINFOSERVER, error);
    free (error);
    } 
  else
    {
    // How much of the start of the server's path to strip, to
    //  create a relative pathname on the client
    int prefix_len;

    char *spec = NULL;
    if (dropbox_stat_get_type (stat) == DBSTAT_FOLDER)
      {
      path = strdup (remote);

      char *pathcopy = strdup (path);
      char *p = strrchr (pathcopy, '/');
      *p = 0;
      // I'm not at all sure about this. We can't recurse manually on
      //   the server -- only collect a list of matching files. So we
      //   can't build a list that either does, or does not, include the
      //   directory name, as we can when recursing on the client. All we
      //   can do is try to simulate the semantics of rsync by removing
      //   a certain amount of the full pathname, that amount to be decided
      //   by whether the remote path ended in a / or not. Ugh :/
      if (remote_ended_slash)
        prefix_len = strlen (path); 
      else
        prefix_len = strlen (pathcopy); 
      free (pathcopy);
      }
    else if (dropbox_stat_get_type (stat) == DBSTAT_FILE)
      {
      // In principle, we don't even need to proceed to listing
      //  files on the server, since we have identified this as
      //  a single file. However, when we add include/exclude
      //  processing later, it will be easier if we use the same logic
      char *p = strrchr (remote, '/');
      if (p)
        {
        *p = 0; // Is this safe? Do we use it later?
        spec = strdup (p+1);
        path = strdup (remote);
        }
      else
        {
        // This should never happen, as we check before entry
        //   that paths start with /. 
        log_error ("Internal error -- remote path has no /");
        }
      prefix_len = strlen (path);
      }
    else
      {
      char *p = strrchr (remote, '/');
      if (p)
        {
        *p = 0; // Is this safe? Do we use it later?
        spec = strdup (p+1);
        path = strdup (remote);
        }
      else
        {
        // This should never happen, as we check before entry
        //   that paths start with /. 
        log_error ("Internal error -- remote path has no /");
        }
      prefix_len = strlen (path);
      }

    if (spec == NULL)
      spec = strdup ("*");

    log_debug ("path=%s, spec=%s", path, spec);

    List *list = dropbox_stat_create_list();
    dropbox_list_files (token, path, list, FALSE, recursive, &error);

    if (error)
      {
      log_error ("%s: %s: %s", "get", ERROR_CANTLISTSERVER, error);
      free (error);
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
        // TODO include/exclude here
        free (pathcopy);
	} 
      
      l = list_length (globbed_list);
      if (l > 1 && !local_is_dir)
        {
        // TODO
        log_error ("%s: %s", "get", ERROR_MULTIFILE);
        }
      else if (l == 0)
        {
        // TODO
        log_warning ("%s: %s", "get", 
          "No files selected for download");
        }
      else
        {
	for (i = 0; i < l; i++)
	  {
	  const char *remote_path = list_get (globbed_list, i);
	  const char *relative = remote_path + prefix_len;
          char *full_local;
          if (local_is_dir)
            {
            asprintf (&full_local, "%s%s", local, relative); 
            }
          else
            full_local = strdup (local);
          cmd_get_consider_and_download (token, context, remote_path, 
            full_local, counters, argv0);
	  } 
        }
      list_destroy (globbed_list);
      }

    free (path);
    free (remote);
    free (local);
    free (spec);
    list_destroy (list);
    dropbox_stat_destroy (stat);
    } 
  OUT
  }


/*==========================================================================
cmd_get
*==========================================================================*/
int cmd_get (const CmdContext *context, int argc, char **argv)
  {
  IN
  int ret = 0;

  log_debug ("Starting get command");

  if (argc < 3)
    {
    log_error ("%s: %s: this command takes two or more arguments\n",
      argv[0], ERROR_USAGE);
    OUT
    return EINVAL;
    }

  char *dest_spec = strdup (argv [argc - 1]);
  log_debug ("dest_spec is %s", dest_spec);

  BOOL local_is_dir = FALSE;

  struct stat sb;
  if (stat (dest_spec, &sb) == 0)
    {
    if (S_ISDIR (sb.st_mode))
      {
      local_is_dir = TRUE;
      }
    }

   if (TRUE)
    {
    // This is not sufficient -- we need to expand the file list, not
    //  just count the number of arguments. If an argument contains 
    //  multiple files, it will still need the target to be a folder
    if (argc > 3 && !local_is_dir)
      {
      log_error ("%s: %s", argv[0], ERROR_MULTIFILE); 
      }
    else
      {
      if (dest_spec[strlen(dest_spec) - 1] == '/')
        dest_spec[strlen(dest_spec) - 1] = 0;

      char *error = NULL;
      char *token = token_init (&error);
      if (token)
	{
	Counters *counters = malloc (sizeof (Counters));
	memset (counters, 0, sizeof (Counters));

	int i;
	for (i = 1; i < argc - 1; i++)
	  {
          if (argv[i][0] == '/')
            {
	    cmd_get_one_remote_spec 
              (token, context, argv[i], dest_spec, counters, 
                local_is_dir, argv[0]);
            }
          else
            {
            log_error ("%s: %s", 
              argv[0], ERROR_STARTSLASH); 
            }
	  }
   
	printf ("Files considered: %d\n", counters->total_items);
	printf ("Downloaded: %d\n", counters->downloaded); 
	if (counters->skip_unchanged > 0)
	  printf ("Skipped because unchanged: %d\n", counters->skip_unchanged); 
	int total_errors = counters->get_info_failed
	   + counters->download_failed;
	if (total_errors > 0)
	  {
	  printf ("Errors: %d\n", total_errors); 
	  printf ("  Could not get remote file metadata: %d\n", 
	   counters->get_info_failed); 
	  printf ("  Download failed: %d\n", 
	   counters->download_failed); 
	  }
	free (counters);
        free (token);
        }
      else
        {
	log_error ("%s: %s: %s", 
	  argv[0], ERROR_INITTOKEN, error);
	free (error);
	ret = EBADRQC;
        }
      }
    }

  OUT
  return ret;
  }

/*==========================================================================
cmd_get_make_directory
Make the directory for the specified file, including parents, if possible
There is no error return -- we'll find out whether the operation succeeded
when we try to use the directory
*==========================================================================*/
static void cmd_get_make_directory (const char *_filename)
  {
  char *filename = strdup (_filename);
  char *dir = dirname (filename);
  log_debug ("Ensuring directory %s exists", dir);
  char *cmd = NULL;
  asprintf (&cmd, "mkdir -p \"%s\"", dir);
  system (cmd);
  free (cmd);
  free (filename);
  }

