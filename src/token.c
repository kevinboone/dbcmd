/*---------------------------------------------------------------------------
dbcmd
token.c
GPL v3.0
---------------------------------------------------------------------------*/

#define _GNU_SOURCE
#include <stdio.h> 
#include <malloc.h> 
#include <stdlib.h> 
#include <errno.h> 
#include <string.h> 
#include <unistd.h> 
#include "auth.h"
#include "token.h"
#include "dropbox.h"

#define FILENAME ".dbcmd_token"

/*---------------------------------------------------------------------------
token_get_filename
---------------------------------------------------------------------------*/
char *token_get_filename (void)
  {
  char *ret = NULL;
  asprintf (&ret, "%s/" FILENAME, getenv("HOME"));
  return ret;
  }


/*---------------------------------------------------------------------------
token_delete
---------------------------------------------------------------------------*/
void token_delete (void)
  {
  char *filename = token_get_filename();
  unlink (filename);
  free (filename);
  }


/*---------------------------------------------------------------------------
token_prompt
---------------------------------------------------------------------------*/
char *token_prompt (void)
  {
  char *ret = NULL;
  printf ("Please enter this URL into your browser, and allow " APP_NAME "\n"
          "to get access to your Dropbox files: \n\n"
          "https://www.dropbox.com/oauth2/authorize?response_type=code&client_id=" APP_KEY "\n\n"
          "Then enter the authorization code here: ");

  fflush (stdout);
  char s[300];
  fgets (s, sizeof (s) - 1, stdin);
  if (s [strlen(s) - 1] == 10) s[strlen(s) - 1] = 0;

  char *error = NULL;

  ret = dropbox_get_token (s, &error);
  if (error)
    {
    fprintf (stderr, "Can't create access token: %s\n", error);
    free (error);
    }

  return ret; 
  }


/*---------------------------------------------------------------------------
token_read_stored
---------------------------------------------------------------------------*/
char *token_read_stored (void)
  {
  char *ret = NULL;
  char *filename = token_get_filename();

  FILE *f = fopen (filename, "r");
  if (f)
    {
    char s[300];
    memset (s, 0, sizeof (s));
    fscanf (f, "%s", s);
    fclose (f);
    ret = strdup (s);
    }
  else
    {
    // Not an error in itself
    }

  free (filename);
  return ret;
  }


/*---------------------------------------------------------------------------
token_store
---------------------------------------------------------------------------*/
void token_store (const char *token)
  {
  char *filename = token_get_filename();

  FILE *f = fopen (filename, "w");
  if (f)
    {
    // TODO set file permissions
    fprintf (f, "%s\n", token);
    fclose (f);
    }
  else
    fprintf (stderr, "Warning: can't open file to store token\n");

  free (filename);
  }


/*---------------------------------------------------------------------------
token_init
---------------------------------------------------------------------------*/
char *token_init (char **error)
  {
  char *token = token_read_stored ();
  if (!token)
   {
   token = token_prompt(); // Will display its own errors
   if (token)
     token_store (token);
    }  
  return token;
  }


