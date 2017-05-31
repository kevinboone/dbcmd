/*==========================================================================
dbcmd
dropbox.c
Copyright (c)2017 Kevin Boone, GPLv3.0
*==========================================================================*/

#define _GNU_SOURCE
#include <stdio.h>
#include <curl/curl.h>
#include <malloc.h>
#include <memory.h>
#include <time.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "token.h"
#include "cJSON.h"
#include "dropbox.h"
#include "dropbox_stat.h"
#include "list.h"
#include "log.h"
#include "auth.h"
#include "sha256.h"

#define EASY_INIT_FAIL "Cannot initialize curl"

// Size of file chuck to do SHA256 on, when compariing hashes.
// This is not arbitrary -- the DB algorithm depends on using a fixed
// size hash
#define HASH_BUFSZ (4 * 1024 * 1024)


/*---------------------------------------------------------------------------
Forward
---------------------------------------------------------------------------*/
void _dropbox_list_files (const char *token, const char *path, 
    List *list, BOOL include_dirs, BOOL recursive, const char *cursor, 
    char **error);
static size_t dropbox_write_callback (void *contents, size_t size, 
    size_t nmemb, void *userp);
static size_t dropbox_store_callback (void *contents, size_t size, 
    size_t nmemb, void *userp);
static time_t dropbox_parse_timestamp (const char *s);


/*---------------------------------------------------------------------------
Private structs
---------------------------------------------------------------------------*/
struct DBWriteStruct 
  {
  char *memory;
  size_t size;
  };

struct DBStoreStruct 
  {
  int f; // A file handle
  };


/*---------------------------------------------------------------------------
dropbox_humanize_error
---------------------------------------------------------------------------*/
void dropbox_humanize_error (const char *db_error, char **error)
  {
  // There are probably plenty more messages that might be encountered...
  if (strstr (db_error, "not_found"))
    asprintf (error, "Path or file not found");
  else if (strstr (db_error, "path/not_folder"))
     asprintf (error, "Path supplied was not a folder");
  else if (strstr (db_error, "conflict/file"))
     asprintf (error, "Filename already exists");
  else if (strstr (db_error, "conflict/folder"))
     asprintf (error, "Folder already exists");
  else if (strstr (db_error, "malformed_path"))
     asprintf (error, "Server path incorrectly formed");
  else if (strstr (db_error, "insufficient_space"))
     asprintf (error, "Insufficient space on server");
  else
    asprintf (error, "%s", db_error);
  }


/*---------------------------------------------------------------------------
This should be called in situations where a curl perform call returns
zero, but there might be an error message buried in the server
response. 
---------------------------------------------------------------------------*/
void dropbox_check_response_for_error (const char *response, char **error)
  {
  log_debug ("dropbox_check_response_for_error \"%s\"", response);
  cJSON *root = cJSON_Parse (response); 
  if (root)
    {
    cJSON *j  = cJSON_GetObjectItem (root, "error_summary");
    if (j)
      {
      dropbox_humanize_error (j->valuestring, error);
      }
    else
      {
      // Do nowt -- non-error JSON response
      }
    cJSON_Delete (root);
    }
  else
    {
    asprintf (error, "%s", response);
    }

  OUT
  }


/*---------------------------------------------------------------------------
 TODO -- restructure this so callers just use 
   dropbox_check_response_for_error
---------------------------------------------------------------------------*/
static char *dropbox_decode_server_error (const char *text)
  {
  IN
  log_debug ("decode_server_error \"%s\"", text);
  char *ret = NULL;
  dropbox_check_response_for_error (text, &ret);
  OUT
  return ret;
  }


/*---------------------------------------------------------------------------
dropbox_get_file_info
---------------------------------------------------------------------------*/
void dropbox_get_file_info (const char *token, const char *path, 
          DBStat *stat, char **error)
  {
  IN
  log_debug ("dropbox_get_file_info token=%s, path=%s",
    token, path);

  if (strcmp (path, "") == 0)
    {
    // Dropbox does not support file info on the top-level directory 
    dropbox_stat_set_path (stat, "/");
    dropbox_stat_set_name (stat, "/");
    dropbox_stat_set_type (stat, DBSTAT_FOLDER);
    }
  else
    {
    CURL* curl = curl_easy_init();
    if (curl)
      {
      struct DBWriteStruct response;
      response.memory = malloc(1);  
      response.size = 0;    
   
      struct curl_slist *headers = NULL;

      curl_easy_setopt (curl, CURLOPT_POST, 1);

      char *auth_header, *data;
      asprintf (&auth_header, "Authorization: Bearer %s", token);
      headers = curl_slist_append (headers, auth_header);
      headers = curl_slist_append (headers, "Content-Type: application/json");

      curl_easy_setopt (curl, CURLOPT_URL, 
	  "https://api.dropboxapi.com/2/files/get_metadata");

      asprintf (&data, "{\"path\":\"%s\"}", path); 

      char curl_error [CURL_ERROR_SIZE];
      curl_easy_setopt (curl, CURLOPT_ERRORBUFFER, curl_error);
      curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, dropbox_write_callback);
      curl_easy_setopt (curl, CURLOPT_WRITEDATA, &response);
      curl_easy_setopt (curl, CURLOPT_POSTFIELDS, data);
      curl_easy_setopt (curl, CURLOPT_HTTPHEADER, headers);

      CURLcode curl_code = curl_easy_perform (curl);
      if (curl_code == 0)
	{
	char *resp = response.memory;
        if (strstr (resp, "path/not_found"))
	  {
          dropbox_stat_set_type (stat, DBSTAT_NONE);
	  }
        else 
	  {
	  cJSON *root = cJSON_Parse (resp); 
	  if (root)
	    {
	    cJSON *j_tag  = cJSON_GetObjectItem (root, ".tag");
	    if (j_tag)
	      {
	      if (strcmp (j_tag->valuestring, "folder") == 0)
                dropbox_stat_set_type (stat, DBSTAT_FOLDER);
	      else 
                dropbox_stat_set_type (stat, DBSTAT_FILE);

	    cJSON *j_path = cJSON_GetObjectItem (root, "path_display");
	    if (j_path)
	      dropbox_stat_set_path (stat, j_path->valuestring); 
	    cJSON *j_name = cJSON_GetObjectItem (root, "name");
	    if (j_name)
	      dropbox_stat_set_name (stat, j_name->valuestring); 

	      cJSON *j_size = cJSON_GetObjectItem (root, "size");
	      if (j_size)
		dropbox_stat_set_length (stat, j_size->valueint);      

	      cJSON *j_server_modified  = cJSON_GetObjectItem 
                 (root, "server_modified");
	      if (j_server_modified)
	        {
                dropbox_stat_set_server_modified 
                  (stat, dropbox_parse_timestamp 
                    (j_server_modified->valuestring)); 
                }

	      cJSON *j_client_modified  = cJSON_GetObjectItem 
                 (root, "client_modified");
	      if (j_client_modified)
	        {
                dropbox_stat_set_client_modified 
                  (stat, dropbox_parse_timestamp 
                    (j_client_modified->valuestring)); 
                }

	      cJSON *j_hash  = cJSON_GetObjectItem 
                 (root, "content_hash");
	      if (j_hash)
	        {
                dropbox_stat_set_hash (stat, j_hash->valuestring);
                }
	      }
	    else
	      {
	      *error = dropbox_decode_server_error (resp);
	      }
	    cJSON_Delete (root);
	    }
	  else
	    {
	    *error = strdup (resp);
	    }
          }
	}
      else
	{
	*error = strdup (curl_error); 
	}

      free (response.memory);
      curl_slist_free_all (headers); 
      free (auth_header);
      free (data);
      curl_easy_cleanup (curl);
      }
    else
      {
      *error = strdup (EASY_INIT_FAIL); 
      // TODO
      }
    }
  OUT
  }



/*---------------------------------------------------------------------------
dropbox_get_token
---------------------------------------------------------------------------*/
char *dropbox_get_token (const char *code, char **error)
  {
  IN

  char *ret = NULL;

  log_debug ("dropbox_get_token: make token from code \"%s\"", code);

  CURL* curl = curl_easy_init();
  if (curl)
    {
    struct DBWriteStruct response;
    response.memory = malloc(1);  
    response.size = 0;    
 
    struct curl_slist *headers = NULL;

    curl_easy_setopt (curl, CURLOPT_POST, 1);

    char *curl_url = NULL;
    char *curl_creds = NULL;
    asprintf (&curl_url, 
      "https://api.dropboxapi.com/oauth2/token?grant_type=authorization_code&code=%s", 
      code); 
    asprintf (&curl_creds, 
      "%s:%s", APP_KEY, APP_SECRET); 
    curl_easy_setopt (curl, CURLOPT_URL, curl_url);
    curl_easy_setopt (curl, CURLOPT_USERPWD, curl_creds);

    log_debug ("curl URL is %s", curl_url);
    log_debug ("curl creds are %s", curl_creds);

    char curl_error [CURL_ERROR_SIZE];
    curl_easy_setopt (curl, CURLOPT_ERRORBUFFER, curl_error);
    curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, dropbox_write_callback);
    curl_easy_setopt (curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt (curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt (curl, CURLOPT_POSTFIELDS, "");

    log_debug ("About to issue request");
    CURLcode curl_code = curl_easy_perform (curl);
    log_debug ("Request complete -- error code %d", curl_code);
    if (curl_code == 0)
      {
      const char *resp = response.memory;
      cJSON *root = cJSON_Parse (resp); 
      if (root)
        {
        cJSON *j_token  = cJSON_GetObjectItem (root, "access_token");
        if (j_token)
          {
          ret = strdup (j_token->valuestring);
          }
        else
          {
          *error = dropbox_decode_server_error (resp);
          }
         cJSON_Delete (root);
        }
      else
        {
        *error = dropbox_decode_server_error (resp);
        }
      }
    else
      {
      *error = strdup (curl_error); 
      }

    free (curl_url);
    free (curl_creds);
    free (response.memory);
    curl_slist_free_all (headers); 
    curl_easy_cleanup (curl);
    }
  else
    {
    *error = strdup (EASY_INIT_FAIL); 
    }

  OUT
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
void dropbox_move (const char *token, const char *old_path, 
           const char *new_path, char **error)
  {
  IN
  log_debug ("dropbox_move old=%s new=%s", old_path, new_path);
  CURL* curl = curl_easy_init();
  if (curl)
    {
    struct DBWriteStruct response;
    response.memory = malloc(1);  
    response.size = 0;    
   
    struct curl_slist *headers = NULL;

    curl_easy_setopt (curl, CURLOPT_POST, 1);

    char *auth_header, *data;
    asprintf (&auth_header, "Authorization: Bearer %s", token);
    headers = curl_slist_append (headers, auth_header);
    headers = curl_slist_append (headers, 
	"Content-Type: application/json");

    curl_easy_setopt (curl, CURLOPT_URL, 
	"https://api.dropboxapi.com/2/files/move");

    asprintf (&data, 
	"{\"from_path\":\"%s\",\"to_path\":\"%s\"}", old_path, new_path);
    headers = curl_slist_append (headers, data);

    char curl_error [CURL_ERROR_SIZE];
    curl_easy_setopt (curl, CURLOPT_ERRORBUFFER, curl_error);
    curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, dropbox_write_callback);
    curl_easy_setopt (curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt (curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt (curl, CURLOPT_POSTFIELDS, data);

    CURLcode curl_code = curl_easy_perform (curl);
    if (curl_code == 0)
      {
      dropbox_check_response_for_error (response.memory, error);
      }
    else
      {
      *error = strdup (curl_error); 
      }

    free (response.memory);
    curl_slist_free_all (headers); 
    free (auth_header);
    free (data);
    curl_easy_cleanup (curl);
    }
  else
    {
    *error = strdup (EASY_INIT_FAIL); 
    }
  OUT
  }

/*---------------------------------------------------------------------------
dropbox_parse_timestammp
---------------------------------------------------------------------------*/
static time_t dropbox_parse_timestamp (const char *s)
  {
  IN
  log_debug ("Parsing timestamp %s", s);
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

  log_debug ("Parsed time_t is %ld", t);
  OUT
  return t;
  }


/*---------------------------------------------------------------------------
dropbox_parse_file_list
---------------------------------------------------------------------------*/
static void dropbox_parse_file_list (const char *token, const char *path, 
    const char *response, BOOL include_dirs, BOOL recursive, List *list, 
    char **error)
  {
  IN
  cJSON *root = cJSON_Parse (response); 
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
          DBStat *stat = dropbox_stat_create();
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
	       dropbox_parse_timestamp (j_server_modified->valuestring); 
	    }
	  cJSON *j_client_modified  = cJSON_GetObjectItem 
	     (item, "client_modified");
	  if (j_client_modified)
	    {
	    stat->client_modified = 
	       dropbox_parse_timestamp (j_client_modified->valuestring); 
	    }
	  list_append (list, stat);
	  }
	else if (j_tag && strcmp (j_tag->valuestring, "folder") == 0 && 
            include_dirs)
	  {
          DBStat *stat = dropbox_stat_create();
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
          const char *cursor = j_cursor->valuestring;
          _dropbox_list_files (token, path, list, include_dirs, 
             recursive, cursor, error); 
	  }
	}
      }
    else
      {
      char *msg = dropbox_decode_server_error (response);
      if (error) *error = msg;
      }
    }
  else
    {
    if (error) *error = strdup (response); 
    }

  cJSON_Delete (root);
  OUT
  }


/*---------------------------------------------------------------------------
_dropbox_list_files
---------------------------------------------------------------------------*/
void _dropbox_list_files (const char *token, const char *path, 
    List *list, BOOL include_dirs, BOOL recursive, const char *cursor, 
    char **error)
  {
  IN
  log_debug ("token=%s, path=%s, include_dirs=%d, recursive=%d, cursor=%s",
    token, path, include_dirs, recursive, cursor);
  CURL* curl = curl_easy_init();
  if (curl)
    {
    struct DBWriteStruct response;
    response.memory = malloc(1);  
    response.size = 0;    
 
    struct curl_slist *headers = NULL;

    curl_easy_setopt (curl, CURLOPT_POST, 1);

    char *auth_header, *data;
    asprintf (&auth_header, "Authorization: Bearer %s", token);
    headers = curl_slist_append (headers, auth_header);
    headers = curl_slist_append (headers, "Content-Type: application/json");

    if (cursor)
      {
      curl_easy_setopt (curl, CURLOPT_URL, 
        "https://api.dropboxapi.com/2/files/list_folder/continue");

      asprintf (&data, "{\"cursor\":\"%s\"}", cursor); 
      }
    else
      {
      curl_easy_setopt (curl, CURLOPT_URL, 
        "https://api.dropboxapi.com/2/files/list_folder");

      asprintf (&data, "{\"path\":\"%s\",\"recursive\":%s}", path,
       recursive ? "true": "false");
      }

    char curl_error [CURL_ERROR_SIZE];
    curl_easy_setopt (curl, CURLOPT_ERRORBUFFER, curl_error);
    curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, dropbox_write_callback);
    curl_easy_setopt (curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt (curl, CURLOPT_POSTFIELDS, data);
    curl_easy_setopt (curl, CURLOPT_HTTPHEADER, headers);

    CURLcode curl_code = curl_easy_perform (curl);
    if (curl_code == 0)
      {
      dropbox_parse_file_list (token, path, response.memory, include_dirs, 
        recursive, list, error);
      }
    else
      {
      *error = strdup (curl_error); 
      }

    free (response.memory);
    curl_slist_free_all (headers); 
    free (auth_header);
    free (data);
    curl_easy_cleanup (curl);
    }
  else
    {
    *error = strdup (EASY_INIT_FAIL); 
    // TODO
    }

  OUT
  }


/*---------------------------------------------------------------------------
dropbox_list_files
---------------------------------------------------------------------------*/
void dropbox_list_files (const char *token, const char *path, 
    List *list, BOOL include_dirs, BOOL recursive, char **error)
  {
  IN
  _dropbox_list_files (token, path, list, include_dirs, recursive, 
     NULL, error);
  OUT
  }


/*---------------------------------------------------------------------------
dropbox_newfolder
---------------------------------------------------------------------------*/
void dropbox_newfolder (const char *token, const char *new_path,
           char **error)
  {
  IN
  log_debug ("dropbox_newfolder path=%s", new_path); 
  CURL* curl = curl_easy_init();
  if (curl)
    {
    struct DBWriteStruct response;
    response.memory = malloc(1);  
    response.size = 0;    
   
    struct curl_slist *headers = NULL;

    curl_easy_setopt (curl, CURLOPT_POST, 1);

    char *auth_header, *data;
    asprintf (&auth_header, "Authorization: Bearer %s", token);
    headers = curl_slist_append (headers, auth_header);
    headers = curl_slist_append (headers, 
	"Content-Type: application/json");

    curl_easy_setopt (curl, CURLOPT_URL, 
	"https://api.dropboxapi.com/2/files/create_folder");

    asprintf (&data, 
	"{\"path\":\"%s\"}", new_path);
    headers = curl_slist_append (headers, data);

    char curl_error [CURL_ERROR_SIZE];
    curl_easy_setopt (curl, CURLOPT_ERRORBUFFER, curl_error);
    curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, dropbox_write_callback);
    curl_easy_setopt (curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt (curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt (curl, CURLOPT_POSTFIELDS, data);

    CURLcode curl_code = curl_easy_perform (curl);
    if (curl_code == 0)
      {
      dropbox_check_response_for_error (response.memory, error);
      }
    else
      {
      *error = strdup (curl_error); 
      }

    free (response.memory);
    curl_slist_free_all (headers); 
    free (auth_header);
    free (data);
    curl_easy_cleanup (curl);
    }
  else
    {
    *error = strdup (EASY_INIT_FAIL); 
    }
  OUT
  }


/*---------------------------------------------------------------------------
dropbox_deleete
---------------------------------------------------------------------------*/
void dropbox_delete (const char *token, const char *path, 
    char **error)
  {
  IN
  log_debug ("dropbox_delete path=%s", path); 
  CURL* curl = curl_easy_init();
  if (curl)
    {
    struct DBWriteStruct response;
    response.memory = malloc(1);  
    response.size = 0;    
   
    struct curl_slist *headers = NULL;

    curl_easy_setopt (curl, CURLOPT_POST, 1);

    char *auth_header, *data;
    asprintf (&auth_header, "Authorization: Bearer %s", token);
    headers = curl_slist_append (headers, auth_header);
    headers = curl_slist_append (headers, 
	"Content-Type: application/json");

    curl_easy_setopt (curl, CURLOPT_URL, 
	"https://api.dropboxapi.com/2/files/delete");

    asprintf (&data, 
	"{\"path\":\"%s\"}", path);
    headers = curl_slist_append (headers, data);

    char curl_error [CURL_ERROR_SIZE];
    curl_easy_setopt (curl, CURLOPT_ERRORBUFFER, curl_error);
    curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, dropbox_write_callback);
    curl_easy_setopt (curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt (curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt (curl, CURLOPT_POSTFIELDS, data);

    CURLcode curl_code = curl_easy_perform (curl);
    if (curl_code == 0)
      {
      dropbox_check_response_for_error (response.memory, error);
      }
    else
      {
      *error = strdup (curl_error); 
      }

    free (response.memory);
    curl_slist_free_all (headers); 
    free (auth_header);
    free (data);
    curl_easy_cleanup (curl);
    }
  else
    {
    *error = strdup (EASY_INIT_FAIL); 
    }
  OUT
  }


/*---------------------------------------------------------------------------
dropbox_download
---------------------------------------------------------------------------*/
void dropbox_download (const char *token, const char *source, 
    const char *target, char **error)
  {
  IN
  log_debug ("dropbox_download token=%s, source=%s, "
    "target=%s",
    token, source, target); 

  int f = open (target, O_CREAT | O_TRUNC | O_WRONLY, 0666);
  if (f >= 0) // 0 seems to be possible on Android, at least
    {
    struct DBStoreStruct ss;
    ss.f = f;
    CURL* curl = curl_easy_init();
    if (curl)
      {
      struct curl_slist *headers = NULL;

      curl_easy_setopt (curl, CURLOPT_POST, 1);

      char *auth_header, *data;
      asprintf (&auth_header, "Authorization: Bearer %s", token);
      headers = curl_slist_append (headers, auth_header);
      // Dropbox insists that the content-type is empty
      headers = curl_slist_append (headers, 
	"Content-Type: ");

      curl_easy_setopt (curl, CURLOPT_URL, 
	"https://content.dropboxapi.com/2/files/download");

      asprintf (&data, 
	"Dropbox-API-Arg: {\"path\":\"%s\"}", source);
      headers = curl_slist_append (headers, data);

      char curl_error [CURL_ERROR_SIZE];
      curl_easy_setopt (curl, CURLOPT_ERRORBUFFER, curl_error);
      curl_easy_setopt (curl, CURLOPT_HTTPHEADER, headers);
      curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, dropbox_store_callback);
      curl_easy_setopt (curl, CURLOPT_WRITEDATA, (void *) &ss);
      // It seems we need to post _something_, or curl just gets stuck
      curl_easy_setopt (curl, CURLOPT_POSTFIELDS, "");

      CURLcode curl_code = curl_easy_perform (curl);
      if (curl_code == 0)
	{
        // We just have to assume that everything went OK. If it didn't,
        //  there might be an error message in the response itself; but
        //  how to distinguish this from a valid file, when we don't
        //  know in advance what the file should contain?
	}
      else
	{
	*error = strdup (curl_error); 
	}

      curl_slist_free_all (headers); 
      free (auth_header);
      free (data);
      curl_easy_cleanup (curl);
      }
    else
      {
      *error = strdup (EASY_INIT_FAIL); 
      }

    close (f);
    }
  else
    {
    asprintf (error, "Can't write %s: %s", source, strerror (errno));
    }

  OUT
  }


/*---------------------------------------------------------------------------
dropbox_upload_start
---------------------------------------------------------------------------*/
void dropbox_upload_start (const char *token, char**session, char **error)
  {
  log_debug ("Upload start");

  CURL* curl = curl_easy_init();
  if (curl)
    {
    struct DBWriteStruct response;
    response.memory = malloc(1);  
    response.size = 0;    
     
    struct curl_slist *headers = NULL;

    curl_easy_setopt (curl, CURLOPT_POST, 1);

    char *auth_header, *data;
    asprintf (&auth_header, "Authorization: Bearer %s", token);
    headers = curl_slist_append (headers, auth_header);
    headers = curl_slist_append (headers, 
	  "Content-Type: application/octet-stream");

    curl_easy_setopt (curl, CURLOPT_URL, 
	  "https://content.dropboxapi.com/2/files/upload_session/start");

    asprintf (&data, 
	  "Dropbox-API-Arg: {\"close\":false}");
    headers = curl_slist_append (headers, data);

    char curl_error [CURL_ERROR_SIZE];
    curl_easy_setopt (curl, CURLOPT_ERRORBUFFER, curl_error);
    curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, dropbox_write_callback);
    curl_easy_setopt (curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt (curl, CURLOPT_READDATA, (void *)NULL);
    curl_easy_setopt (curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt (curl, CURLOPT_INFILESIZE, 0);
    curl_easy_setopt (curl, CURLOPT_POSTFIELDSIZE, 0);

    CURLcode curl_code = curl_easy_perform (curl);
    if (curl_code == 0)
      {
      char *text = response.memory;
      cJSON *root = cJSON_Parse (text); 
      if (root)
	{
	cJSON *j_sid  = cJSON_GetObjectItem (root, "session_id");
	if (j_sid)
	  {
          *session = strdup (j_sid->valuestring);
          }
        cJSON_Delete (root);
        }
      if (*session == NULL)
        dropbox_check_response_for_error (text, error);
      }
     else
      {
      *error = strdup (curl_error); 
      }

     free (response.memory);
     curl_slist_free_all (headers); 
     free (auth_header);
     free (data);
     curl_easy_cleanup (curl);
     }
  else
     {
     *error = strdup (EASY_INIT_FAIL); 
     }

  }


/*---------------------------------------------------------------------------
dropbox_upload_done
---------------------------------------------------------------------------*/
void dropbox_upload_done (const char *token, const char *session, 
    size_t offset, const char *path,  char **error)
  {
  log_debug ("Upload done, session = %s, offset=%ld\n", session, offset);

  CURL* curl = curl_easy_init();
  if (curl)
    {
    struct DBWriteStruct response;
    response.memory = malloc(1);  
    response.size = 0;    
     
    struct curl_slist *headers = NULL;

    curl_easy_setopt (curl, CURLOPT_POST, 1);

    char *auth_header, *data;
    asprintf (&auth_header, "Authorization: Bearer %s", token);
    headers = curl_slist_append (headers, auth_header);
    headers = curl_slist_append (headers, 
	  "Content-Type: application/octet-stream");

    curl_easy_setopt (curl, CURLOPT_URL, 
	  "https://content.dropboxapi.com/2/files/upload_session/finish");

    asprintf (&data, 
	  "Dropbox-API-Arg: {\"cursor\":{\"session_id\":\"%s\",\"offset\":%ld},\"commit\":{\"path\":\"%s\",\"mode\":\"overwrite\"}}",
          session, offset, path);
    headers = curl_slist_append (headers, data);

    char curl_error [CURL_ERROR_SIZE];
    curl_easy_setopt (curl, CURLOPT_ERRORBUFFER, curl_error);
    curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, dropbox_write_callback);
    curl_easy_setopt (curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt (curl, CURLOPT_READDATA, (void *)NULL);
    curl_easy_setopt (curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt (curl, CURLOPT_INFILESIZE, 0);
    curl_easy_setopt (curl, CURLOPT_POSTFIELDSIZE, 0);

    CURLcode curl_code = curl_easy_perform (curl);
    if (curl_code == 0)
      {
      // TODO -- confirm response
      char *text = response.memory;
      cJSON *root = cJSON_Parse (text); 
      if (root)
	{
	cJSON *j_name  = cJSON_GetObjectItem (root, "name");
	if (j_name)
	  {
          // If we get _anything_ as a "name" in the response, assume
          //  OK
          }
         else
          {
          dropbox_check_response_for_error (text, error);
          }
        cJSON_Delete (root);
        }
      }
     else
      {
      *error = strdup (curl_error); 
      }

     free (response.memory);
     curl_slist_free_all (headers); 
     free (auth_header);
     free (data);
     curl_easy_cleanup (curl);
     }
  else
     {
     *error = strdup (EASY_INIT_FAIL); 
     }

  }


/*---------------------------------------------------------------------------
dropbox_upload_block
---------------------------------------------------------------------------*/
void dropbox_upload_block (const char *token, void *data, int length, 
      const char *session, size_t offset, char **error) 
  {
  log_debug ("Upload block, session = %s, offset=%ld\n", session, offset);

  CURL* curl = curl_easy_init();
  if (curl)
    {
    struct DBWriteStruct response;
    response.memory = malloc(1);  
    response.size = 0;    
     
    struct curl_slist *headers = NULL;

    curl_easy_setopt (curl, CURLOPT_POST, 1);

    char *auth_header, *argdata;
    asprintf (&auth_header, "Authorization: Bearer %s", token);
    headers = curl_slist_append (headers, auth_header);
    headers = curl_slist_append (headers, 
	  "Content-Type: application/octet-stream");

    curl_easy_setopt (curl, CURLOPT_URL, 
	  "https://content.dropboxapi.com/2/files/upload_session/append_v2");

    asprintf (&argdata, 
	  "Dropbox-API-Arg: {\"cursor\":{\"session_id\":\"%s\",\"offset\":%ld},\"close\":false}",
          session, offset);
    headers = curl_slist_append (headers, argdata);

    char curl_error [CURL_ERROR_SIZE];
    curl_easy_setopt (curl, CURLOPT_ERRORBUFFER, curl_error);
    curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, dropbox_write_callback);
    curl_easy_setopt (curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt (curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt (curl, CURLOPT_INFILESIZE, length);
    curl_easy_setopt (curl, CURLOPT_POSTFIELDSIZE, length);
    curl_easy_setopt (curl, CURLOPT_POSTFIELDS, (void *)data);

    CURLcode curl_code = curl_easy_perform (curl);
    if (curl_code == 0)
      {
      // TODO -- confirm response
      }
     else
      {
      *error = strdup (curl_error); 
      }

     free (response.memory);
     curl_slist_free_all (headers); 
     free (auth_header);
     free (argdata);
     curl_easy_cleanup (curl);
     }
  else
     {
     *error = strdup (EASY_INIT_FAIL); 
     }


  }


/*---------------------------------------------------------------------------
dropbox_upload
---------------------------------------------------------------------------*/
void dropbox_upload (const char *token, const char *source, 
    const char *target, int buffsize_mb, char **error)
  {
  IN
  log_debug ("dropbox_upload token=%s, source=%s, "
    "target=%s",
    token, source, target); 
  struct stat sb;
  stat (source, &sb);
  //int length = (int) sb.st_size;

  FILE *f = fopen (source, "r");
  int buffsize = 1024 * 1024 * buffsize_mb; 
  log_debug ("Upload blocksize is %d", buffsize);
  if (f)
    {
    void *buff = malloc (buffsize);
    size_t l;
    size_t offset = 0;
    char *session = NULL;

    dropbox_upload_start (token, &session, error);

    while ((l = fread (buff, 1, buffsize, f)) > 0 && !(*error))
      {
      dropbox_upload_block (token, buff, l, session, offset, error);
      offset += l;
      }

    dropbox_upload_done (token, session, offset, target, error);

    if (session) free (session);

#ifdef no_longer_used 
    CURL* curl = curl_easy_init();
    if (curl)
      {
      struct DBWriteStruct response;
      response.memory = malloc(1);  
      response.size = 0;    
   
      struct curl_slist *headers = NULL;

      curl_easy_setopt (curl, CURLOPT_POST, 1);

      char *auth_header, *data;
      asprintf (&auth_header, "Authorization: Bearer %s", token);
      headers = curl_slist_append (headers, auth_header);
      headers = curl_slist_append (headers, 
	"Content-Type: application/octet-stream");

      curl_easy_setopt (curl, CURLOPT_URL, 
	"https://content.dropboxapi.com/2/files/upload");

      asprintf (&data, 
	"Dropbox-API-Arg: {\"path\":\"%s\",\"mode\":\"overwrite\"}", target);
      headers = curl_slist_append (headers, data);

      char curl_error [CURL_ERROR_SIZE];
      curl_easy_setopt (curl, CURLOPT_ERRORBUFFER, curl_error);
      curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, dropbox_write_callback);
      curl_easy_setopt (curl, CURLOPT_WRITEDATA, &response);
      curl_easy_setopt (curl, CURLOPT_READDATA, (void *)f);
      curl_easy_setopt (curl, CURLOPT_HTTPHEADER, headers);
      curl_easy_setopt (curl, CURLOPT_INFILESIZE, length);
      curl_easy_setopt (curl, CURLOPT_POSTFIELDSIZE, length);

      CURLcode curl_code = curl_easy_perform (curl);
      if (curl_code == 0)
	{
        dropbox_check_response_for_error (response.memory, error);
	}
      else
	{
	*error = strdup (curl_error); 
	}

      free (response.memory);
      curl_slist_free_all (headers); 
      free (auth_header);
      free (data);
      curl_easy_cleanup (curl);
      }
    else
      {
      *error = strdup (EASY_INIT_FAIL); 
      }
#endif

    free (buff);
    fclose(f);
    }
  else
    {
    asprintf (error, "Can't read %s: %s", source, strerror (errno));
    }
  OUT
  }


/*---------------------------------------------------------------------------
dropbox_get_usage
---------------------------------------------------------------------------*/
void dropbox_get_usage (const char *token, int64_t *quota, 
           int64_t *usage, char **error)
  {
  *quota = 0;
  *usage = 0;
  CURL* curl = curl_easy_init();
  if (curl)
    {
    struct DBWriteStruct response;
    response.memory = malloc(1);  
    response.size = 0;    
   
    struct curl_slist *headers = NULL;

    curl_easy_setopt (curl, CURLOPT_POST, 1);

    char *auth_header;
    asprintf (&auth_header, "Authorization: Bearer %s", token);
    headers = curl_slist_append (headers, auth_header);
    // Must send an empty content-type here, else DB expects some data
    //   that actually matches the content type, and no data is expected
    //   with this API call
    headers = curl_slist_append (headers, "Content-Type: ");

    curl_easy_setopt (curl, CURLOPT_URL, 
	"https://api.dropboxapi.com/2/users/get_space_usage");

    char curl_error [CURL_ERROR_SIZE];
    curl_easy_setopt (curl, CURLOPT_ERRORBUFFER, curl_error);
    curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, dropbox_write_callback);
    curl_easy_setopt (curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt (curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt (curl, CURLOPT_POSTFIELDS, "");

    CURLcode curl_code = curl_easy_perform (curl);
    if (curl_code == 0)
      {
      const char *text = response.memory;
      cJSON *root = cJSON_Parse (text); 
      if (root)
	{
	cJSON *j_used  = cJSON_GetObjectItem (root, "used");
	if (j_used)
	  {
          *usage = (int64_t) j_used->valuedouble;

	  cJSON *j_allocation  = cJSON_GetObjectItem (root, "allocation");
	  if (j_allocation)
            {
	    cJSON *j_allocated  = cJSON_GetObjectItem (j_allocation, 
               "allocated");
            if (j_allocation)
              {
              *quota = (int64_t) j_allocated->valuedouble;
              }
            }
          }
         else
          {
          dropbox_check_response_for_error (text, error);
          }
        cJSON_Delete (root);
        }
      else
        dropbox_check_response_for_error (text, error);
      }
    else
      {
      *error = strdup (curl_error); 
      }

    free (response.memory);
    curl_slist_free_all (headers); 
    free (auth_header);
    curl_easy_cleanup (curl);
    }
  else
    {
    *error = strdup (EASY_INIT_FAIL); 
    }
  }



/*---------------------------------------------------------------------------
dropbox_write_callback
Callback for storing server response into an expandable memory block
---------------------------------------------------------------------------*/
static size_t dropbox_write_callback (void *contents, size_t size, 
    size_t nmemb, void *userp)
  {
  IN
  size_t realsize = size * nmemb;
  struct DBWriteStruct *mem = (struct DBWriteStruct *)userp;
  mem->memory = realloc (mem->memory, mem->size + realsize + 1);
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;
  OUT
  return realsize;
  }


/*---------------------------------------------------------------------------
dropbox_store_callback
Callback for storing server response into a disk file 
---------------------------------------------------------------------------*/
static size_t dropbox_store_callback (void *contents, size_t size, 
    size_t nmemb, void *userp)
  {
  IN
  size_t realsize = size * nmemb;
  struct DBStoreStruct *ss = (struct DBStoreStruct *)userp;
  write (ss->f, contents, realsize);
  OUT
  return realsize;
  }



