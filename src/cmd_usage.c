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
#include "misc.h"


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
        char *s_quota, *s_usage, *s_remaining;
        misc_format_size (quota, &s_quota);
        misc_format_size (usage, &s_usage);
        int64_t remaining = quota - usage;
        misc_format_size (remaining, &s_remaining);
        printf ("Quota: %s\n", s_quota);
        printf ("Usage: %s", s_usage);
        if (quota > 0) 
          {
          printf (" (%ld%%)\n", 100 * usage / quota);
          } 
        else
          printf ("\n");
        printf ("Remaining: %s", s_remaining);
        if (quota > 0) 
          {
          printf (" (%ld%%)\n", 100 * remaining / quota);
          } 
        else
          printf ("\n");
        free (s_quota); 
        free (s_usage); 
        free (s_remaining); 
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





