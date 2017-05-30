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
#include "token.h"
#include "cJSON.h"
#include "dropbox.h"
#include "dropbox_stat.h"
#include "list.h"
#include "log.h"


/*---------------------------------------------------------------------------
dropbox_stat_create
---------------------------------------------------------------------------*/
DBStat *dropbox_stat_create (void)
  {
  DBStat *self = malloc (sizeof (DBStat));
  memset (self, 0, sizeof (DBStat));
  log_debug ("Created DBStat object %08X", (long) self);
  return self;
  }


/*---------------------------------------------------------------------------
dropbox_stat_destroy
---------------------------------------------------------------------------*/
static void _dropbox_stat_destroy (void *_self)
  {
  DBStat *self = _self;
  dropbox_stat_destroy (self);
  }


/*---------------------------------------------------------------------------
dropbox_stat_destroy
---------------------------------------------------------------------------*/
void dropbox_stat_destroy (DBStat *self)
  {
  log_debug ("Destroying DBStat object %08X", (long) self);
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
dropbox_stat_get_client_modified
---------------------------------------------------------------------------*/
time_t dropbox_stat_get_client_modified (const DBStat *self)
  {
  return self->client_modified;
  }


/*---------------------------------------------------------------------------
dropbox_stat_get_server_modified
---------------------------------------------------------------------------*/
time_t dropbox_stat_get_server_modified (const DBStat *self)
  {
  return self->server_modified;
  }


/*---------------------------------------------------------------------------
dropbox_stat_get_length
---------------------------------------------------------------------------*/
int64_t dropbox_stat_get_length (const DBStat *self)
  {
  return self->length;
  }


/*---------------------------------------------------------------------------
dropbox_stat_get_hash
---------------------------------------------------------------------------*/
const char *dropbox_stat_get_hash (const DBStat *self)
  {
  return self->hash;
  }


/*---------------------------------------------------------------------------
dropbox_stat_get_path
---------------------------------------------------------------------------*/
const char *dropbox_stat_get_path (const DBStat *self)
  {
  return self->path;
  }


/*---------------------------------------------------------------------------
dropbox_stat_get_name
---------------------------------------------------------------------------*/
const char *dropbox_stat_get_name (const DBStat *self)
  {
  return self->name;
  }


/*---------------------------------------------------------------------------
dropbox_stat_get_type
---------------------------------------------------------------------------*/
DBType dropbox_stat_get_type (const DBStat *self)
  {
  return self->type;
  }


/*==========================================================================
dropbox_stat_create_list 
*==========================================================================*/
List *dropbox_stat_create_list (void)
  {
  return list_create (_dropbox_stat_destroy);
  }


/*==========================================================================
dropbox_stat_set_name 
*==========================================================================*/
void dropbox_stat_set_name (DBStat *self, const char *name)
  {
  if (self->name) free (self->name);
  self->name = strdup (name);
  }


/*==========================================================================
dropbox_stat_set_path 
*==========================================================================*/
void dropbox_stat_set_path (DBStat *self, const char *path)
  {
  if (self->path) free (self->path);
  self->path = strdup (path);
  }


/*==========================================================================
dropbox_stat_set_type 
*==========================================================================*/
void dropbox_stat_set_type (DBStat *self, DBType type)
  {
  self->type = type;
  }


/*==========================================================================
dropbox_stat_set_client_modified 
*==========================================================================*/
void dropbox_stat_set_client_modified (DBStat *self, time_t t)
  {
  self->client_modified = t;
  }


/*==========================================================================
dropbox_stat_set_hash 
*==========================================================================*/
void dropbox_stat_set_hash (DBStat *self, const char *hash)
  {
  strncpy (self->hash, hash, DBHASH_LENGTH);
  }


/*==========================================================================
dropbox_stat_set_server_modified 
*==========================================================================*/
void dropbox_stat_set_server_modified (DBStat *self, time_t t)
  {
  self->server_modified = t;
  }


/*==========================================================================
dropbox_stat_set_length
*==========================================================================*/
void dropbox_stat_set_length (DBStat *self, int64_t length)
  {
  self->length = length;
  }


