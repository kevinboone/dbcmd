/*---------------------------------------------------------------------------
dbcmd
commands.h
GPL v3.0
---------------------------------------------------------------------------*/

#pragma once

#include "bool.h"

typedef struct _CmdContext
  { 
  BOOL recursive;
  BOOL long_;
  BOOL force;
  int screen_width;
  BOOL yes;
  BOOL dry_run;
  } CmdContext;


int cmd_delete (const CmdContext *context, int argc, char **argv);
int cmd_move (const CmdContext *context, int argc, char **argv);
int cmd_hash (const CmdContext *context, int argc, char **argv);
int cmd_info (const CmdContext *context, int argc, char **argv);
int cmd_list (const CmdContext *context, int argc, char **argv);
int cmd_put (const CmdContext *context, int argc, char **argv);
int cmd_get (const CmdContext *context, int argc, char **argv);
int cmd_newfolder (const CmdContext *context, int argc, char **argv);
int cmd_usage (const CmdContext *context, int argc, char **argv);

