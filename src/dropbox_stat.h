/*---------------------------------------------------------------------------
dbcmd
dropbox_stat.h
GPL v3.0
---------------------------------------------------------------------------*/

#pragma once

#include "bool.h"
#include "list.h"

#define DBHASH_LENGTH 65

typedef enum {DBSTAT_NONE, DBSTAT_FILE, DBSTAT_FOLDER} DBType;

typedef struct _DBStat
  {
  char     *path;
  char     *name;
  DBType    type;
  int64_t   length;
  time_t    client_modified;
  time_t    server_modified;
  char      hash[DBHASH_LENGTH];
  } DBStat;

List        *dropbox_stat_create_list (void);
DBStat      *dropbox_stat_create (void);
const char  *dropbox_stat_get_path (const DBStat *self);
const char  *dropbox_stat_get_name (const DBStat *self);
const char  *dropbox_stat_get_hash (const DBStat *self);
int64_t      dropbox_stat_get_length (const DBStat *self);
DBType       dropbox_stat_get_type (const DBStat *self);
time_t       dropbox_stat_get_server_modified (const DBStat *self);
time_t       dropbox_stat_get_client_modified (const DBStat *self);
void         dropbox_stat_set_name (DBStat *self, const char *name);
void         dropbox_stat_set_path (DBStat *self, const char *path);
void         dropbox_stat_set_type (DBStat *self, DBType type);
void         dropbox_stat_set_client_modified (DBStat *self, time_t t);
void         dropbox_stat_set_hash (DBStat *self, const char *hash);
void         dropbox_stat_set_length (DBStat *self, int64_t length);
void         dropbox_stat_set_server_modified (DBStat *self, time_t t);
void         dropbox_stat_destroy (DBStat *self);
DBStat      *dropbox_stat_clone (const DBStat *self);
