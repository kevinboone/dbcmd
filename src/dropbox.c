/*---------------------------------------------------------------------------
dbcmd
dropbox.c
GPL v3.0
---------------------------------------------------------------------------*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "cJSON.h"
#include "curl.h"
#include "dropbox.h"
#include "bool.h"
#include "log.h"
#include "sha256.h"

#define HASH_BUFSZ (4 * 1024 * 1024)

static char *make_parse_error_message (void);
static const char *dropbox_reason (int code);
static time_t parse_db_timestamp (const char *s);

/*---------------------------------------------------------------------------
dbstat_free
---------------------------------------------------------------------------*/
void dbstat_free (void *_self)
  {
  DBStat *self = _self;
  if (self)
    {
    if (self->path)
      free (self->path); 
    if (self->name)
      free (self->name); 
    free (self);
    }
  }

/*---------------------------------------------------------------------------
Ugh. There seems to be no way of knowing when DB will return a JSON-format
error, or a plain text error message.
---------------------------------------------------------------------------*/
static char *decode_server_error (const char *text)
  {
  char *ret = NULL;

  cJSON *root = cJSON_Parse (text); 
  if (root)
    {
    cJSON *j  = cJSON_GetObjectItem (root, "error_summary");
    if (j)
      {
      if (strstr (j->valuestring, "path/not_found"))
        asprintf (&ret, "Path or file not found");
      else if (strstr (j->valuestring, "path/not_folder"))
        asprintf (&ret, "Path supplied was not a folder");
      else
        asprintf (&ret, "%s", j->valuestring);
      }
    else
      {
      asprintf (&ret, "%s", text);
      }
    cJSON_Delete (root);
    }
  else
    {
    asprintf (&ret, "%s", text);
    }

  return ret;
  }


/*---------------------------------------------------------------------------
dropbox_create_folder
---------------------------------------------------------------------------*/
BOOL dropbox_create_folder (const char *token, const char *new_path,
    char **error)
  {
  BOOL ret = FALSE;
  char *text = curl_issue_create_folder (token, new_path, error); 
  if (text)
    {
    cJSON *root = cJSON_Parse (text); 
    if (root)
      {
      cJSON *j_name  = cJSON_GetObjectItem (root, "name");
      if (j_name)
	{
        ret = TRUE;
        // That's good enough to assume success
	}
      else
        {
	*error = decode_server_error (text);
	}
      cJSON_Delete (root);
      }
    else
      {
      *error = make_parse_error_message();
      }

    free (text);
    }

  return ret;
  }


/*---------------------------------------------------------------------------
dropbox_compare_files
---------------------------------------------------------------------------*/
BOOL dropbox_compare_files (const char *token, const char *local_file, 
    const char *remote_file, DBCompRes *res,  char **error)
  {
  BOOL ret = TRUE;
  log_debug ("Comparing file timestamps");

  memset (res, 0, sizeof (DBCompRes));

  struct stat sb;
  if (stat (local_file, &sb) == 0)
    {
    log_debug ("Local file %s exists", local_file);
    res->local_file_exists = TRUE;
    res->local_time = sb.st_mtime;
    dropbox_hash (local_file, res->local_hash, error);
    }
  else
    log_debug ("Local file %s does not exist", local_file);

  DBStat dbstat;
  if (dropbox_get_file_info (token, remote_file, &dbstat, error))
    {
    if (dbstat.type == DBSTAT_FILE)
      {
      log_debug ("Remote file %s exists", remote_file);
      res->remote_file_exists = TRUE;
      res->remote_time = dbstat.server_modified; 
      strncpy (res->remote_hash, dbstat.hash, DBHASH_LENGTH);
      }
    else if (dbstat.type == DBSTAT_FOLDER)
      {
      ret = FALSE;
      *error = strdup ("Attempt to get file attributes for a folder");
      }
    else
      {
      log_debug ("Remote file %s does not exist", remote_file);
      }
    }
  else
    {
    // Do nothing here -- propagate the error to the caller
    ret = FALSE;
    }
  return ret;
  }


/*---------------------------------------------------------------------------
dropbox_delete
Delete file or directory. NOTE **recursive** (this behaviour is part of
the DB API and cannot be changed)
---------------------------------------------------------------------------*/
BOOL dropbox_delete (const char *token, const char *path, 
    char **error)
  {
  BOOL ret = FALSE;
  char *text = curl_issue_delete_path (token, path, error); 
  if (text)
    {
    cJSON *root = cJSON_Parse (text); 
    if (root)
      {
      cJSON *j_tag  = cJSON_GetObjectItem (root, ".tag");
      if (j_tag)
	{
        ret = TRUE;
        // That's good enough to assume success
	}
      else
        {
	*error = decode_server_error (text);
	}
      cJSON_Delete (root);
      }
    else
      {
      *error = make_parse_error_message();
      }

    free (text);
    }

  return ret;
  }


/*---------------------------------------------------------------------------
dropbox_download_file
---------------------------------------------------------------------------*/
BOOL dropbox_download_file (const char *token, const char *source, 
    const char *target, char **error)
  {
  BOOL ret = FALSE;

  int resp = 0;
  curl_issue_download_file (token, source, target, &resp, error);
  log_debug ("Curl response is %d", resp);

  if (resp == 200)
    {
    // Do nothing -- it's OK
    }
  else
    {
    // TODO -- parse the returned file as a potential JSON response
    asprintf (error, "Can't download %s: %s", source, dropbox_reason (resp));
    unlink (target);
    }

  return ret; 
  }


/*---------------------------------------------------------------------------
dropbox_get_file_info
---------------------------------------------------------------------------*/
BOOL dropbox_get_file_info (const char *token, const char *path, 
    DBStat *stat, char **error)
  {
  BOOL ret = FALSE;
  memset (stat, 0, sizeof (DBStat));

  if (strcmp (path, "/"))
    {
    // Dropbox does not support a metadata call on /, for some reason
    // So we must provide our own
    char *text = curl_issue_get_file_info (token, path, error);

    if (text)
      {
      if (strstr (text, "path/not_found"))
	{
        stat->type = DBSTAT_NONE;	
	}
      else 
	{
	//printf ("text=%s\n", text);

	cJSON *root = cJSON_Parse (text); 
	if (root)
	  {
	  cJSON *j_tag  = cJSON_GetObjectItem (root, ".tag");
	  if (j_tag)
	    {
	    if (strcmp (j_tag->valuestring, "folder") == 0)
	      stat->type = DBSTAT_FOLDER;
	    else 
	      stat->type = DBSTAT_FILE;

	    cJSON *j_server_modified  = cJSON_GetObjectItem 
               (root, "server_modified");
	    if (j_server_modified)
	      {
              stat->server_modified = 
                 parse_db_timestamp (j_server_modified->valuestring); 
              }

	    cJSON *j_client_modified  = cJSON_GetObjectItem 
               (root, "client_modified");
	    if (j_client_modified)
	      {
              stat->client_modified = 
                 parse_db_timestamp (j_client_modified->valuestring); 
              }

	    cJSON *j_hash  = cJSON_GetObjectItem 
               (root, "content_hash");
	    if (j_hash)
	      {
              memcpy (stat->hash, j_hash->valuestring, DBHASH_LENGTH);
              }
	    ret = TRUE;
	    }
	  else
	    {
	    *error = decode_server_error (text);
	    }
	  cJSON_Delete (root);
	  }
	else
	  {
	  *error = make_parse_error_message();
	  }
	}

      free (text);
      ret = TRUE;
      }
    else
      {
      // Do nothing, and hope error has been set
      }
    }
  else
    {
    stat->type = DBSTAT_FOLDER;
    ret = TRUE;
    }

  return ret; 
  }


/*---------------------------------------------------------------------------
_dropbox_get_fole_list
---------------------------------------------------------------------------*/
BOOL _dropbox_get_file_list (const char *token, const char *path, 
    BOOL recursive, BOOL include_dirs, const char *cursor, 
    List *list, char **error)
  {
  BOOL ret = FALSE;

  if (strcmp (path, "/") == 0) path = "";
  char *text = curl_issue_list_files (token, path, recursive, cursor, error);
  if (text)
    {
    cJSON *root = cJSON_Parse (text); 
    if (root)
      {
      cJSON *entries = cJSON_GetObjectItem (root, "entries");
      if (entries) 
	{
	int i, n = cJSON_GetArraySize (entries);
	for (i = 0; i < n; i++)
	  {
	  cJSON *item = cJSON_GetArrayItem (entries, i);
	  cJSON *j_tag = cJSON_GetObjectItem (item, ".tag");
	  if (j_tag && strcmp (j_tag->valuestring, "file") == 0)
            {
            DBStat *stat = malloc (sizeof (DBStat));
            memset (stat, 0, sizeof (DBStat));
            stat->type = DBSTAT_FILE;
	    cJSON *item = cJSON_GetArrayItem (entries, i);
	    cJSON *j_path = cJSON_GetObjectItem (item, "path_display");
	    if (j_path)
	      stat->path  = strdup (j_path->valuestring); 
	    cJSON *j_name = cJSON_GetObjectItem (item, "name");
	    if (j_name)
	      stat->name  = strdup (j_name->valuestring); 
	    cJSON *j_size = cJSON_GetObjectItem (item, "size");
	    if (j_size)
	      stat->length = j_size->valueint;      
	    cJSON *j_server_modified  = cJSON_GetObjectItem 
               (item, "server_modified");
	    if (j_server_modified)
	      {
              stat->server_modified = 
                 parse_db_timestamp (j_server_modified->valuestring); 
              }
	    cJSON *j_client_modified  = cJSON_GetObjectItem 
               (item, "client_modified");
	    if (j_client_modified)
	      {
              stat->client_modified = 
                 parse_db_timestamp (j_client_modified->valuestring); 
              }
            list_append (list, stat);
            }
          else if (j_tag && strcmp (j_tag->valuestring, "folder") == 0 && include_dirs)
            {
            DBStat *stat = malloc (sizeof (DBStat));
            memset (stat, 0, sizeof (DBStat));
            stat->type = DBSTAT_FOLDER;
	    cJSON *item = cJSON_GetArrayItem (entries, i);
	    cJSON *j_path = cJSON_GetObjectItem (item, "path_display");
	    if (j_path)
	      stat->path  = strdup (j_path->valuestring); 
	    cJSON *j_name = cJSON_GetObjectItem (item, "name");
	    if (j_name)
	      stat->name  = strdup (j_name->valuestring); 
            list_append (list, stat);
            }
          }
        cJSON *has_more = cJSON_GetObjectItem (root, "has_more");
        if (has_more) 
	  {
          if (has_more->valueint)
            {
            cJSON *j_cursor = cJSON_GetObjectItem (root, "cursor");
           _dropbox_get_file_list (token, "", recursive, include_dirs, 
               j_cursor->valuestring, 
               list, error);
            }
          }
        ret = TRUE;
        }
      else
        {
        char *msg = decode_server_error (text);
        if (error) *error = msg;
        }
      }
    else
      {
      if (error) *error = make_parse_error_message();
      }

    cJSON_Delete (root);
    free (text);
    }
  return ret;
  }


/*---------------------------------------------------------------------------
dropbox_get_fole_list
---------------------------------------------------------------------------*/
BOOL dropbox_get_file_list (const char *token, const char *path, 
    BOOL recursive, BOOL include_dirs, List *list, char **error)
  {
  return _dropbox_get_file_list (token, path, recursive, include_dirs, NULL, 
    list, error);
  }


/*---------------------------------------------------------------------------
dropbox_get_token
---------------------------------------------------------------------------*/
char *dropbox_get_token (const char *code, char **error)
  {
  char *ret = NULL;
  char *text = curl_issue_token (code, error);
  if (!*error)
    {
    //printf ("text=%s\n", text);
    cJSON *root = cJSON_Parse (text); 
    if (root)
      {
      cJSON *j_token  = cJSON_GetObjectItem (root, "access_token");
      if (j_token)
        {
        ret = strdup (j_token->valuestring);
        }
      else
        {
        *error = decode_server_error (text);
        }
      }
    else
      {
      *error = make_parse_error_message();
      }

    cJSON_Delete (root);
    }
  //printf ("hello XXXXX %s\n", ret);
  return ret;
  }


/*---------------------------------------------------------------------------
dropbox_hash
---------------------------------------------------------------------------*/
BOOL dropbox_hash (const char *filename, char output_hash[65], char **error)
  {
  BOOL ret = FALSE;
  int f = open (filename, O_RDONLY);
  if (f >= 0)
    {
    unsigned char *s = malloc (HASH_BUFSZ);
    unsigned char *bighash = malloc(0);
    int n;
    int bytes = 0;
    while ((n = read (f, s, HASH_BUFSZ)) > 0)
      {
      unsigned char hash[32];
      sha256_hash_block (s, n, hash);
      bighash = realloc (bighash, 32 + bytes);
      memcpy (bighash + bytes, hash, 32);
      bytes += 32;
      }
    unsigned char final_hash[32];
    memset (final_hash, 0, 32);
    sha256_hash_block ((unsigned char *)bighash, bytes, final_hash);
    int i;
    memset (output_hash, 0, 65);
    for (i = 0; i < 32; i++)
      sprintf (output_hash + (i*2), "%02x", final_hash[i]);
    output_hash[64] = 0;
    free (s);
    free (bighash);
    close (f);
    }
  else
    {
    *error = strdup ("Can't open file for reading");
    }
  return ret;
  }


/*---------------------------------------------------------------------------
dropbox_move
---------------------------------------------------------------------------*/
BOOL dropbox_move (const char *token, const char *old_path, const char *new_path,
    char **error)
  {
  BOOL ret = FALSE;

  char *text = curl_issue_move (token, old_path, new_path, error); 
  if (text)
    {
    cJSON *root = cJSON_Parse (text); 
    if (root)
      {
      cJSON *j_tag  = cJSON_GetObjectItem (root, ".tag");
      if (j_tag)
	{
        ret = TRUE;
        // That's good enough to assume success
	}
      else
        {
	*error = decode_server_error (text);
	}
      cJSON_Delete (root);
      }
    else
      {
      *error = make_parse_error_message();
      }

    free (text);
    }

  return ret;
  }





/*---------------------------------------------------------------------------
dropbox_reason
Convert HTTP code to failure reason -- these codes are not partcularly
well aligned with usual HTTP practice
---------------------------------------------------------------------------*/
static const char *dropbox_reason (int code)
  {
  switch (code)
    {
    case 200: return "OK";
    case 400: return "Authentication token expired or no permissions";
    case 409: return "Not found, or no permissions";
    }
    
  return "Unknown error";
  }
 

/*---------------------------------------------------------------------------
dropbox_upload_file
---------------------------------------------------------------------------*/
BOOL dropbox_upload_file (const char *token, const char *source, 
    const char *target, char **error)
  {
  BOOL ret = FALSE;

  int resp = 0;
  curl_issue_upload_file (token, source, target, &resp, error);
  log_debug ("Curl response is %d", resp);

  if (resp == 200)
    {
    // Do nothing -- it's OK
    }
  else
    {
    // TODO -- parse the returned file as a potential JSON response
    asprintf (error, "Can't upload %s: %s", source, dropbox_reason (resp));
    unlink (target);
    }

  return ret; 
  }


/*---------------------------------------------------------------------------
parse_db_timestammp
---------------------------------------------------------------------------*/
static time_t parse_db_timestamp (const char *s)
  {
  char *old_tz = getenv ("TZ");

  // Ugh. This sucks. There is no UTC/GMT equivalent of mktime() in
  //  C :/
  setenv ("TZ", "UTC0", 1);
  tzset();

  struct tm tm;
  memset (&tm, 0, sizeof (struct tm));
  strptime (s, "%FT%TZ", &tm);  

  time_t t = mktime (&tm);   

  if (old_tz)
    {
    setenv ("TZ", old_tz, 1);
    tzset();
    }
  else
    {
    unsetenv ("TZ");
    }

  return t;
  }


/*---------------------------------------------------------------------------
make_parse_error_message
//TODO
---------------------------------------------------------------------------*/
static char *make_parse_error_message (void)
  {
  char *ret = NULL;
  asprintf (&ret, "JSON parse error near here: %s", cJSON_GetErrorPtr());
  return ret;
  }


