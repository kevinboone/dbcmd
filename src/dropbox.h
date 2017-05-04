/*---------------------------------------------------------------------------
dbcmd
dropbox.h
GPL v3.0
---------------------------------------------------------------------------*/

#pragma once

#include "bool.h"
#include "list.h"

#define DBHASH_LENGTH 65

typedef struct _DBStat
  {
  char *path;
  char *name;
  enum {DBSTAT_NONE, DBSTAT_FILE, DBSTAT_FOLDER} type;
  int64_t length;
  time_t client_modified;
  time_t server_modified;
  char hash[DBHASH_LENGTH];
  } DBStat;

typedef struct _DBCompRes
  {
  BOOL local_file_exists;
  BOOL remote_file_exists;
  time_t local_time;
  time_t remote_time;
  char local_hash[DBHASH_LENGTH];
  char remote_hash[DBHASH_LENGTH];
  } DBCompRes;

BOOL dropbox_create_folder (const char *token, const char *new_path,
    char **error);
BOOL dropbox_compare_files (const char *token, const char *local_file, 
    const char *remote_file, DBCompRes *res, char **error);
BOOL dropbox_delete (const char *token, const char *path, 
    char **error);
BOOL dropbox_get_file_info (const char *token, const char *path, 
    DBStat *stat, char **error);
BOOL dropbox_get_file_list (const char *token, const char *path, BOOL recursive, 
    BOOL include_dirs, List *list,  char **error);
char *dropbox_get_token (const char *code, char **error);
BOOL dropbox_hash (const char *filename, char hash[65], char **error);
BOOL dropbox_move (const char *token, const char *old_path, const char *new_path,
    char **error);
BOOL dropbox_download_file (const char *token, const char *source, 
    const char *target, char **error);
BOOL dropbox_upload_file (const char *token, const char *source, 
    const char *target, char **error);

void dbstat_free (void *self);


