/*---------------------------------------------------------------------------
dbcmd
curl.c
GPL v3.0
---------------------------------------------------------------------------*/

#define _GNU_SOURCE
#include <stdio.h> 
#include <malloc.h> 
#include <stdlib.h> 
#include <errno.h> 
#include <string.h> 
#include <unistd.h> 
#include "config.h"
#include "curl.h"
#include "utils.h"
#include "auth.h"
#include "log.h"



/*---------------------------------------------------------------------------
curl_issue_create_folder
--------------------------------------------------------------------------*/
char *curl_issue_create_folder (const char *token, const char *new_path, 
    char **error)
  {
  char *ret = NULL;
  char *cmd = NULL;

  asprintf (&cmd, "curl " CURL_EXTRA " -s -X "
    "POST https://api.dropboxapi.com/2/files/create_folder "
    "--header \"Authorization: Bearer %s\" "
    "--header \"Content-Type: application/json\" "
    "--data \"{\\\"path\\\": \\\"%s\\\"}\"",
  token, new_path);

  log_debug ("Executing command: %s", cmd);
  FILE *f = popen (cmd, "r");
  if (f)
    {
    ret = utils_read_stream_to_end (f);
    pclose (f);
    }
  else
    {
    if (error) asprintf (error, "Can't run curl: %s", strerror (errno));
    }
  free (cmd);

  return ret;
  }

/*---------------------------------------------------------------------------
curl_issue_delete_path
--------------------------------------------------------------------------*/
char *curl_issue_delete_path (const char *token, const char *path, 
    char **error)
  {
  char *ret = NULL;
  char *cmd = NULL;

  asprintf (&cmd, "curl " CURL_EXTRA " -s -X "
    "POST https://api.dropboxapi.com/2/files/delete "
    "--header \"Authorization: Bearer %s\" "
    "--header \"Content-Type: application/json\" "
    "--data \"{\\\"path\\\": \\\"%s\\\"}\"",
  token, path);

  log_debug ("Executing command: %s", cmd);
  FILE *f = popen (cmd, "r");
  if (f)
    {
    ret = utils_read_stream_to_end (f);
    pclose (f);
    }
  else
    {
    if (error) asprintf (error, "Can't run curl: %s", strerror (errno));
    }
  free (cmd);

  return ret;
  }

/*---------------------------------------------------------------------------
curl_issue_download_file
---------------------------------------------------------------------------*/
void curl_issue_download_file (const char *token, const char *source, 
    const char *target, int *http_code, char **error)
  {
  char *cmd = NULL;
  asprintf (&cmd, "curl " CURL_EXTRA " -X "
    "POST https://content.dropboxapi.com/2/files/download "
    "-o \"%s\" "
    "-w %%{http_code} "
    "--header \"Authorization: Bearer %s\" "
    "--header \"Dropbox-API-Arg: {\\\"path\\\": \\\"%s\\\"}\"",
    target, token, source); 
  log_debug ("Executing command: %s", cmd);
  FILE *f = popen (cmd, "r");
  if (f)
    {
    char *s_resp = utils_read_stream_to_end (f);
    int resp = atoi (s_resp);
    *http_code = resp;
    free (s_resp);
    pclose (f);
    }
  else
    {
    if (error) asprintf (error, "Can't run curl: %s", strerror (errno));
    }
  free (cmd);
  }


/*---------------------------------------------------------------------------
ccurl_issue_get_file_info
---------------------------------------------------------------------------*/
char *curl_issue_get_file_info (const char *token, const char *path, 
    char **error)
  {
  char *ret = NULL;
  char *cmd = NULL;

  asprintf (&cmd, "curl " CURL_EXTRA " -s -X "
    "POST https://api.dropboxapi.com/2/files/get_metadata "
    "--header \"Authorization: Bearer %s\" "
    "--header \"Content-Type: application/json\" "
    "--data \"{\\\"path\\\": \\\"%s\\\"}\"",
  token, path);

  log_debug ("Executing command: %s", cmd);
  FILE *f = popen (cmd, "r");
  if (f)
    {
    ret = utils_read_stream_to_end (f);
    pclose (f);
    }
  else
    {
    if (error) asprintf (error, "Can't run curl: %s", strerror (errno));
    }
  free (cmd);

  return ret;
  }


/*---------------------------------------------------------------------------
ccurl_issue_list_files
---------------------------------------------------------------------------*/
char *curl_issue_list_files (const char *token, const char *path, 
    BOOL recursive, const char *cursor, char **error)
  {
  char *cmd = NULL;
  char *ret = NULL;
  log_debug ("On entry to curl_issue_list_files, cursor = %s", cursor);
  if (!cursor)
    {
    asprintf (&cmd, "curl " CURL_EXTRA " -s -X "
      "POST https://api.dropboxapi.com/2/files/list_folder "
      "--header \"Authorization: Bearer %s\" "
      "--header \"Content-Type: application/json\" "
      "--data \"{\\\"path\\\": \\\"%s\\\",\\\"recursive\\\": %s,\\\"include_media_info\\\": false,\\\"include_deleted\\\": false,\\\"include_has_explicit_shared_members\\\": false}\"",
    token, path, recursive?"true":"false");
    }
  else
    {
    asprintf (&cmd, "curl " CURL_EXTRA " -s -X "
      "POST https://api.dropboxapi.com/2/files/list_folder/continue "
      "--header \"Authorization: Bearer %s\" "
      "--header \"Content-Type: application/json\" "
      "--data \"{\\\"cursor\\\": \\\"%s\\\"}\"",
    token, cursor);
    }
  log_debug ("Executing command: %s", cmd);
  FILE *f = popen (cmd, "r");
  if (f)
    {
    ret = utils_read_stream_to_end (f);
    pclose (f);
    }
  else
    {
    if (error) asprintf (error, "Can't run curl: %s", strerror (errno));
    }
  free (cmd);
  return ret;
  }


/*---------------------------------------------------------------------------
curl_issue_move
---------------------------------------------------------------------------*/
char *curl_issue_move (const char *token, const char *old_path, 
    const char *new_path, char **error)
  {
  char *ret = NULL;
  char *cmd = NULL;

  asprintf (&cmd, "curl " CURL_EXTRA " -s -X "
    "POST https://api.dropboxapi.com/2/files/move "
    "--header \"Authorization: Bearer %s\" "
    "--header \"Content-Type: application/json\" "
    "--data \"{\\\"from_path\\\": \\\"%s\\\",\\\"to_path\\\": \\\"%s\\\"}\"",
  token, old_path, new_path);

  log_debug ("Executing command: %s", cmd);
  FILE *f = popen (cmd, "r");
  if (f)
    {
    ret = utils_read_stream_to_end (f);
    pclose (f);
    }
  else
    {
    if (error) asprintf (error, "Can't run curl: %s", strerror (errno));
    }
  free (cmd);

  return ret;
  }


/*---------------------------------------------------------------------------
curl_issue_download_file
Tries to get a token from an auth code
---------------------------------------------------------------------------*/
char *curl_issue_token (const char *code, char **error)
  {
  char *cmd = NULL;
  char *ret = NULL;
  asprintf (&cmd, "curl " CURL_EXTRA " -s -X POST "
    "-u %s:%s "
    "\"https://api.dropboxapi.com/oauth2/token?grant_type=authorization_code&code=%s\"", 
    APP_KEY, APP_SECRET, code);
  log_debug ("Executing command: %s", cmd);
  FILE *f = popen (cmd, "r");
  if (f)
    {
    ret = utils_read_stream_to_end (f);
    pclose (f);
    }
  else
    {
    if (error) asprintf (error, "Can't run curl: %s", strerror (errno));
    }
  free (cmd);

  return ret;
  }


/*---------------------------------------------------------------------------
ccurl_issue_upload_file
---------------------------------------------------------------------------*/
void curl_issue_upload_file (const char *token, const char *source, 
    const char *target, int *http_code, char **error)
  {
  char *cmd = NULL;
  asprintf (&cmd, "curl " CURL_EXTRA " -X "
    "POST https://content.dropboxapi.com/2/files/upload "
    "-o /dev/null "
    "--data-binary @\"%s\" "
    "-w %%{http_code} "
    "--header \"Content-Type: application/octet-stream\" "
    "--header \"Authorization: Bearer %s\" "
    "--header \"Dropbox-API-Arg: {\\\"path\\\": \\\"%s\\\", \\\"mode\\\": \\\"overwrite\\\"}\"",
    source, token, target); 
  log_debug ("Executing command: %s", cmd);
  FILE *f = popen (cmd, "r");
  if (f)
    {
    char *s_resp = utils_read_stream_to_end (f);
    int resp = atoi (s_resp);
    *http_code = resp;
    free (s_resp);
    pclose (f);
    }
  else
    {
    if (error) asprintf (error, "Can't run curl: %s", strerror (errno));
    }
  free (cmd);
  }





