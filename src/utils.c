/*---------------------------------------------------------------------------
dbcmd
utils.c
GPL v3.0
---------------------------------------------------------------------------*/

#define _GNU_SOURCE
#include <stdio.h> 
#include <malloc.h> 
#include <stdlib.h> 
#include <errno.h> 
#include <string.h> 
#include <libgen.h> 
#include <sys/types.h> 
#include <sys/stat.h> 
#include <dirent.h> 
#include <fnmatch.h> 
#include "utils.h"
#include "log.h"
#include "list.h"


/*---------------------------------------------------------------------------
_getline
The glibc getline is not available in Android Bionic
---------------------------------------------------------------------------*/
int _getline (char **line, size_t *len, FILE *f)  
  {
  if (feof (f)) return -1; 

  int n = 0;
  char *buff = malloc (1);
  buff[0] = 0;

  int c;
  while ((c = fgetc (f)) != EOF)
    {
    int l = strlen (buff);
    buff = realloc (buff, l + 2);
    buff[l] = c;
    buff[l+1] = 0;
    if (c == '\n')
      {
      *line = buff;
      return n;
      } 
    }
  
  *line = buff;
  return n;
  }


/*---------------------------------------------------------------------------
utils_contains_wildcards
---------------------------------------------------------------------------*/
BOOL utils_contains_wildcards (char *filename)
  {
  if (strchr (filename, '*')) return TRUE;
  if (strchr (filename, '?')) return TRUE;
  return FALSE;
  }


/*---------------------------------------------------------------------------
utils_expand_path
---------------------------------------------------------------------------*/
void utils_expand_path (const char *dir, const char *pattern, List *paths, 
     BOOL recursive)
  {
  DIR *d = opendir (dir);
  if (d)
    {
    struct dirent *de;
    while ((de = readdir (d)))
      {
      if (strcmp (de->d_name, "..") == 0) continue;
      if (strcmp (de->d_name, ".") == 0) continue;
      char *full_name = malloc (strlen(dir) + strlen(de->d_name) + 5);
      strcpy (full_name, dir);
      if (full_name[strlen(full_name) - 1] != '/')
        strcat (full_name, "/");
      strcat (full_name, de->d_name);

      if (utils_is_directory (full_name))
        {
        if (recursive)
          utils_expand_path (full_name, pattern, paths, recursive);
        free (full_name);
        }
      else
        {
        if (fnmatch (pattern, de->d_name, 0) == 0)
          list_append (paths, full_name);
        else
        free (full_name);
        }
      }
    closedir (d);
    }
  else
    {
    log_warning ("Can't open directory '%s': %s", dir, strerror (errno));
    }
  }


/*---------------------------------------------------------------------------
utils_is_directory
---------------------------------------------------------------------------*/
BOOL utils_is_directory (char *path)
  {
   struct stat sb;
   if (stat (path, &sb) != 0)
       return FALSE;
  return S_ISDIR (sb.st_mode);
  }


/*---------------------------------------------------------------------------
utils_make_directory
Make the directory for the specified file, including parents, if possible
There is no error return -- we'll find out whether the operation succeeded
when we try to use the directory
---------------------------------------------------------------------------*/
void utils_make_directory (const char *_filename)
  {
  char *filename = strdup (_filename);
  char *dir = dirname (filename);
  log_debug ("Ensuring directory %s exists", dir);
  char *cmd = NULL;
  asprintf (&cmd, "mkdir -p \"%s\"", dir);
  system (cmd);
  free (cmd);
  free (filename);
  }


/*---------------------------------------------------------------------------
utils_read_stream_to_end
This will always return something, although it might be an empty string
---------------------------------------------------------------------------*/
char *utils_read_stream_to_end (FILE *f)
  {
  char *ret = strdup("");
  char *line = NULL;
  size_t len = 0;
  while (_getline (&line, &len, f) != -1) 
    {
    ret = realloc (ret, strlen (ret) + strlen (line) + 2);
    strcat (ret, line);
    //printf("Retrieved line of length %u :%s\n", len, line);
    if (line) free (line);
    }
  return ret;
  }


