/*---------------------------------------------------------------------------
dbcmd
cmd_list.c
GPL v3.0
---------------------------------------------------------------------------*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include "cJSON.h"
#include "dropbox.h"
#include "token.h"
#include "commands.h"
#include "log.h"
#include "errmsg.h"

/*==========================================================================
make_display_time
*==========================================================================*/
static char *make_display_time (time_t t)
  {
  time_t now = time (NULL);

  struct tm tm_now, tm_t;

  localtime_r (&now, &tm_now);
  localtime_r (&t, &tm_t);

  char buff[50];
  if (tm_now.tm_year != tm_t.tm_year)
    strftime (buff, sizeof (buff) - 1, "%H:%M %b %d %Y", &tm_t);
  else if (tm_now.tm_mday != tm_t.tm_mday || tm_now.tm_mon != tm_t.tm_mon)
    strftime (buff, sizeof (buff) - 1, "%H:%M %b %d", &tm_t);
  else
    strftime (buff, sizeof (buff) - 1, "%H:%M today", &tm_t);

  return strdup (buff);;
  }


/*==========================================================================
make_display_name
*==========================================================================*/
static char *make_display_name (const DBStat *stat, BOOL recursive)
  {
  char *ret;
  if (recursive)
    {
    if (stat->type == DBSTAT_FOLDER)
      asprintf (&ret, "%s/", stat->path);
    else
      asprintf (&ret, "%s", stat->path);
    }
  else
    {
    if (stat->type == DBSTAT_FOLDER)
      asprintf (&ret, "%s/", stat->name);
    else
      asprintf (&ret, "%s", stat->name);
    }
  return ret;
  }


/*==========================================================================
display_list_long
*==========================================================================*/
static void display_list_long (List *list, BOOL recursive)
  {
  int i, l = list_length (list);
  int name_max = 0;
  int size_max = 0;
  int time_max = 0;
  for (i = 0; i < l; i++)
    {
    const DBStat *stat = list_get (list, i);
    char *name = make_display_name (stat, recursive);
    char buff[20];
    sprintf (buff, "%ld", stat->length);
    int ll = strlen (name);
    if (ll > name_max) name_max = ll;
    ll = strlen (buff);
    if (ll > size_max) size_max = ll;
    if (stat->type == DBSTAT_FILE)
      {      
      char *display_time = make_display_time (stat->server_modified);
      ll = strlen (display_time); 
      if (ll > time_max) time_max = ll;
      free (display_time);
      }
    free (name);
    }
  for (i = 0; i < l; i++)
    {
    const DBStat *stat = list_get (list, i);
    char *name = make_display_name (stat, recursive);
    char buff[20];
    int j;
    if (stat->type == DBSTAT_FILE)
      {
      char *display_time = make_display_time (stat->server_modified);
      printf ("file ");
      sprintf (buff, "%ld", stat->length);
      printf ("%s", buff);
      for (j = strlen (buff); j <= size_max + 1; j++)
        printf (" ");
      printf ("%s", display_time);
      for (j = strlen (display_time); j <= time_max + 1; j++)
        printf (" ");
      free (display_time);
      }
    else
      {
      printf ("fldr ");
      for (j = 0; j <= size_max + 1; j++)
        printf (" ");
      for (j = 0; j <= time_max + 1; j++)
        printf (" ");
      }
    printf ("%s\n", name);
    free (name);
    }
  }


/*==========================================================================
pad
*==========================================================================*/
void pad (int n)
  {
  int j;
  for (j = 0; j < n; j++)
    fputs (" ", stdout);
  }


/*==========================================================================
calc_line_len
Works out the maximum line length with a specified number of columns
*==========================================================================*/
int calc_line_len (List *list, int cols)
  {
  int i, l = list_length (list);

  int *col_widths = malloc (cols * sizeof (int));

  for (i = 0; i < cols; i++)
    {
    col_widths[i] = 0;
    }

  int col = 0;
  for (i = 0; i < l; i++)
    {
    const DBStat *stat = list_get (list, i);
    char *name = make_display_name (stat, FALSE);
    int namelen = strlen (name);
    if (namelen > col_widths[col]) col_widths[col] = namelen;
    free (name);
    col++;
    if (col == cols)
      {
      col = 0;
      }
    }

  int line_length = 0;
  for (i = 0; i < cols; i++)
    {
    line_length += col_widths[i] + 1;
    }

  free (col_widths);

  return line_length;
  }

/*==========================================================================
display_list_short
*==========================================================================*/
static void display_list_short (List *list, int screen_width)
  {
  int i, l = list_length (list);

  BOOL stop = FALSE;
  int cols = 1;
  if (isatty (STDOUT_FILENO))
    {
    while (!stop /*&& cols < l*/)
      {
      cols++;
      int line_length = calc_line_len (list, cols);
      if (line_length >= screen_width - 1)
	stop = TRUE;
      if (cols >= screen_width / 2)
	stop = TRUE;
      }
    cols--;
    }

  int *col_widths = malloc (cols * sizeof (int));
  for (i = 0; i < cols; i++)
    {
    col_widths[i] = 0;
    }

  int col = 0;
  for (i = 0; i < l; i++)
    {
    const DBStat *stat = list_get (list, i);
    char *name = make_display_name (stat, FALSE);
    int namelen = strlen (name);
    if (namelen > col_widths[col]) col_widths[col] = namelen;
    free (name);
    col++;
    if (col == cols)
      {
      col = 0;
      }
    }

  col = 0;
  for (i = 0; i < l; i++)
    {
    const DBStat *stat = list_get (list, i);
    char *name = make_display_name (stat, FALSE);
    int namelen = strlen (name);
    fputs (name, stdout);
    pad (col_widths[col] - namelen);
    pad (1);
    
    free (name);
    col++;
    if (col == cols)
      {
      puts ("");
      col = 0;
      }
    }

  free (col_widths);
  puts ("");
  }


/*==========================================================================
cmd_list
*==========================================================================*/
int cmd_list (const CmdContext *context, int argc, char **argv)
  {
  int ret = 0;
  char *error = NULL;
  char *path = NULL; 
  int screen_width = context->screen_width;

  log_debug ("Starting list command");

  BOOL recursive = context->recursive;
  BOOL long_ = context->long_;

  if (argc < 2)
    path = strdup (""); // Root
  else
    { 
    const char *patharg = argv[1];
    if (strcmp (patharg, "/") == 0)
      path = strdup (""); // Not / for root on DB, for some reason
    else
      path = strdup (patharg);
    }
  
  if (path[0] == 0 || path[0] == '/') 
    {
    char *token = token_init (&error);
    if (token)
      {
      List *list = dropbox_stat_create_list();
      dropbox_list_files (token, path, list, TRUE, recursive, &error);

      if (error)
	{
	log_error ("%s: %s", argv[0], error);
	free (error);
        ret = -1;
	} 
      else
        {
        if (long_ || recursive )
          display_list_long (list, recursive);
        else
          display_list_short (list, screen_width);
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
  else
    {
    log_error ("%s: %s: %s", 
      NAME, argv[0], ERROR_STARTSLASH); 
    ret = EINVAL;
    }

  if (path) free (path);
  return ret;
  }



