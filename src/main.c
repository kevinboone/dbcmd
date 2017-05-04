/*---------------------------------------------------------------------------
dbcmd
main.c
GPL v3.0
---------------------------------------------------------------------------*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <termios.h>
#include "cJSON.h"
#include "curl.h"
#include "dropbox.h"
#include "token.h"
#include "commands.h"
#include "log.h"
#include "sha256.h"


/*==========================================================================
Command table
*==========================================================================*/
typedef int (*CmdFn) (const CmdContext *context, int argc, char **argv);
typedef void (*HelpFn) (const CmdContext *context, const char *name);
typedef struct _CmdTableEntry
  {
  const char *name;
  CmdFn fn;
  const char *usage;
  const char *desc;
  void *dummy;
  } CmdTableEntry;


int cmd_commands (const CmdContext *context, int argc, char **argv); // FWD
int cmd_help (const CmdContext *context, int argc, char **argv); // FWD
CmdTableEntry *find_command_fn (const char *name); //FWD


/*==========================================================================
cmd table
*==========================================================================*/

CmdTableEntry cmd_table[] =
  {
  {"commands", cmd_commands, "", "list commands", NULL},
  {"delete", cmd_delete, "{remote_path}", "delete file or folder", NULL},
  {"get", cmd_get, "{remote_path} [local_path]", "download file from server", NULL},
  {"hash",  cmd_hash, "{local_file}", "shows the Dropbox hash of a local file", NULL},
  {"help",  cmd_help, "[command]", "show information about a command", NULL},
  {"info",  cmd_info, "{remote_path}", "show file/path info", NULL},
  {"list",  cmd_list, "[remote_path]", "list files on the server", NULL},
  {"move",  cmd_move, "{old_path} {new_path}", "move file or folder on the server", NULL},
  {"new",  cmd_new, "{new_path}", "create a new folder on the server", NULL},
  {"put",  cmd_put, "{local_path} {remote_path}", "upload file to server", NULL},
  {NULL, NULL}
  };


/*==========================================================================
show_usage
*==========================================================================*/
void show_my_usage (const char *argv0)
  {
  printf ("Usage %s [command] [options] [arguments]\n", argv0);
  printf ("The following options take effect without a command:\n");
  printf ("  -?                   show this message\n");
  printf ("  -v, --version        show version information\n");
  printf ("The following options apply to all commands:\n");
  printf ("      --loglevel=N  set logging verbosity (0-3)\n");
  printf ("  -a, --auth           renew authentication with Dropbox\n");
  printf ("  -y, --yes            renew authentication with Dropbox\n");
  printf ("  -l, --long           display in long format\n");
  printf ("      --width=N        set display width, if it cannot be guessed\n");
  printf ("Other options are available to specific commands:\n");
  printf ("Run '%s help [command]' for information about a command\n", argv0);
  printf ("Run '%s commands' for a list of commands\n", argv0);
  }


/*==========================================================================
cmd_help
*==========================================================================*/
int cmd_help (const CmdContext *context, int argc, char **argv)
  {
  if (argc == 1)
    system ("man dbcmd"); 
  else
    {
    char *cmd;
    asprintf (&cmd, "man dbcmd-%s", argv[1]);
    system (cmd);
    free (cmd);
    }
  return 0;
  }


/*==========================================================================
cmd_commands
*==========================================================================*/
int cmd_commands (const CmdContext *context, int argc, char **argv)
  {
  int i = 0;
  int name_len = 0; 
  int usage_len = 0;
  int desc_len = 0; 
  CmdTableEntry *cmd = &(cmd_table[i]); 
  do
    {
    const char *name = cmd->name;
    const char *usage = cmd->usage;
    const char *desc = cmd->desc;
    int l = strlen (name);
    if (l > name_len) name_len = l;
    l = strlen (usage);
    if (l > usage_len) usage_len = l;
    l = strlen (desc);
    if (l > desc_len) desc_len = l;
    i++;
    cmd = &(cmd_table[i]); 
    } while (cmd->name != NULL);

  i = 0;
  cmd = &(cmd_table[i]); 
  do
    {
    int j;
    printf ("%s", cmd->name);
    for (j = 0; j < name_len - strlen (cmd->name) + 2; j++) printf (" ");
    printf ("%s", cmd->usage);
    for (j = 0; j < usage_len - strlen (cmd->usage) + 3; j++) printf (" ");
    printf ("%s", cmd->desc);
    printf ("\n");
    i++;
    cmd = &(cmd_table[i]); 
    } while (cmd->name != NULL);

  return 0;
  }


/*==========================================================================
find_command_fn
*==========================================================================*/
CmdTableEntry *find_command_fn (const char *name)
  {
  int i = 0;
  CmdTableEntry *cmd = &(cmd_table[i]); 
  do
    {
    if (strcmp (name, cmd->name) == 0)
      return cmd;
    cmd = &(cmd_table[i]); 
    i++;
    } while (cmd->name != NULL);
  return NULL;
  }



/*---------------------------------------------------------------------------
main
---------------------------------------------------------------------------*/
int main (int argc, char **argv)
  {
  static BOOL show_version = FALSE;
  static BOOL show_usage = FALSE;
  static BOOL reauth = FALSE;
  static BOOL force = FALSE;
  static BOOL recursive = FALSE;
  static BOOL long_ = FALSE;
  static BOOL yes = FALSE;
  static int screen_width = 80; //TODO
  static int loglevel = INFO;

  // Sort the arguments so that switches come first
  // A consequence of this rather ugly process is that
  //   we can't have arguments of the form "-x y", only "--x=y"
  char **sorted_argv = malloc (argc * sizeof (char*));

  int i;
  int ii = 0;

  sorted_argv[0] = argv[0];
  ii++;

  for (i = 1; i < argc; i++)
    {
    if (argv[i][0] == '-')
      {
      sorted_argv[ii] = argv[i];
      ii++;
      }
    }

  for (i = 1; i < argc; i++)
    {
    if (argv[i][0] != '-')
      {
      sorted_argv[ii] = argv[i];
      ii++;
      }
    }

  /*for (i = 0; i < argc; i++)
    {
    printf ("%d %s\n", i, sorted_argv[i]);
    }*/

  static struct option long_options[] = 
   {
     {"version", no_argument, &show_version, 'v'},
     {"help", no_argument, &show_usage, '?'},
     {"auth", no_argument, &reauth, 'a'},
     {"recursive", no_argument, &recursive, 'r'},
     {"force", no_argument, &force, 'f'},
     {"long", no_argument, &long_, 'l'},
     {"loglevel", required_argument, NULL, 0},
     {"width", required_argument, NULL, 'w'},
     {"yes", no_argument, &yes, 'y'},
     {0, 0, 0, 0}
   };

  if (isatty (STDOUT_FILENO))
    {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0)
      {
      screen_width = ws.ws_col;
      }
    }

  int opt;
  while (1)
   {
   int option_index = 0;
   opt = getopt_long (argc, sorted_argv, "?valfrw:y",
     long_options, &option_index);

   if (opt == -1) break;

   switch (opt)
     {
     case 0:
        if (strcmp (long_options[option_index].name, "version") == 0)
          show_version = TRUE;
        else if (strcmp (long_options[option_index].name, "help") == 0)
          show_usage = TRUE;
        else if (strcmp (long_options[option_index].name, "auth") == 0)
          reauth = TRUE;
        else if (strcmp (long_options[option_index].name, "recursive") == 0)
          recursive = TRUE;
        else if (strcmp (long_options[option_index].name, "force") == 0)
          force = TRUE;
        else if (strcmp (long_options[option_index].name, "long") == 0)
          long_ = TRUE;
        else if (strcmp (long_options[option_index].name, "yes") == 0)
          yes = TRUE;
        else if (strcmp (long_options[option_index].name, "loglevel") == 0)
          loglevel = atoi (optarg);
        else if (strcmp (long_options[option_index].name, "width") == 0)
          screen_width = atoi (optarg);
        else
          exit (-1);
        break;

     case 'l': long_ = TRUE; break;
     case 'a': reauth = TRUE; break;
     case 'r': recursive = TRUE; break;
     case 'f': force = TRUE; break;
     case 'y': yes = TRUE; break;
     case 'v': show_version = TRUE; break;
     case 'w': screen_width = atoi (optarg); break;
     case '?': show_usage = TRUE; break;
     default:  exit(-1);
     }
   }

  if (show_usage)
    {
    show_my_usage (argv[0]);
    exit (0);
    }
 
  if (show_version)
    {
    printf ("%s " VERSION "\n", argv[0]);
    printf ("Copyright (c)2017 Kevin Boone\n");
    printf ("Distributed under the terms of the GPL, v3.0\n");
    exit (0);
    }


  log_set_level (loglevel);

  int ret = 0;
 
  if (optind == argc)
    {
    fprintf (stderr, "%s: No command specified\n", argv[0]);
    fprintf (stderr, "'%s --help' for brief usage\n", argv[0]);
    fprintf (stderr, "'%s commands' for a list of commands\n", argv[0]);
    ret = -1;
    }
  else
    {
    if (reauth) 
      token_delete();

    int new_argc = argc - optind;
    char **new_argv = malloc (new_argc * sizeof (char *));
    int i;
    for (i = 0; i < new_argc; i++)
      new_argv[i] = strdup (sorted_argv[optind + i]);

    const char *cmd = new_argv[0];

    CmdTableEntry *cmd_entry = find_command_fn (cmd);

    if (cmd_entry)
      {
      CmdContext context;
      context.recursive = recursive;
      context.long_ = long_;
      context.screen_width = screen_width;
      context.yes = yes;
      ret = cmd_entry->fn (&context, new_argc, new_argv); 
      }
    else
      {
      fprintf (stderr, "%s: Unknown command '%s'\n", argv[0], cmd);
      fprintf (stderr, "'%s --help' for usage\n", argv[0]);
      ret = -1;
      }

    for (i = 0; i < new_argc; i++)
      free (new_argv[i]);
    free (new_argv);
    }

  free (sorted_argv);
  return ret;
  }


