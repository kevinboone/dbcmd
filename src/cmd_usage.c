/*---------------------------------------------------------------------------
dbcmd
cmd_usage.c
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
#include "dropbox.h"
#include "token.h"
#include "commands.h"
#include "log.h"
#include "errmsg.h"

/*==========================================================================
format_size
*==========================================================================*/
void format_size (int64_t size, char **result)
  {
  if (size < 1024)
    asprintf (result, "%ld bytes", size);
  else if (size < 1024 * 1024)
    asprintf (result, "%.2g kB", size / 1024.0);
  else if (size < 1024 * 1024 * 1024)
    asprintf (result, "%.2g MB", size / 1024.0 / 1024.0);
  else
    asprintf (result, "%.2g GB", size / 1024.0 / 1024.0 / 1024.0);
  }


/*==========================================================================
cmd_usage
*==========================================================================*/
int cmd_usage (const CmdContext *context, int argc, char **argv)
  {
  int ret = 0;
  char *error = NULL;

  log_debug ("Starting usage command");

  if (argc == 1)
    {
    char *token = token_init (&error);
    if (token)
      {
      int64_t usage, quota;
      dropbox_get_usage (token, &quota, &usage, &error);

      if (error)
        {
        log_error ("%s: %s: %s", argv[0], ERROR_CANTUSAGE, error);
        free (error);
        ret = -1; // TODO
	} 
      else
        {
        char *s_quota, *s_usage;
        format_size (quota, &s_quota);
        format_size (usage, &s_usage);
        printf ("Quota: %s\n", s_quota);
        printf ("Usage: %s", s_usage);
        if (quota > 0) 
          {
          printf (" (%ld%%)\n", 100 * usage / quota);
          } 
        else
          printf ("\n");
        free (s_quota); 
        free (s_usage); 
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
    }
  else
    {
    log_error ("%s: %s: %s", argv[0], ERROR_USAGE, 
       "This command takes no arguments"); 
    ret = EINVAL;
    }

  return ret;
  }





