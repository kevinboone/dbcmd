/*---------------------------------------------------------------------------
dbcmd
token.h
GPL v3.0
---------------------------------------------------------------------------*/

#pragma once

char *token_read_stored (void);
char *token_prompt (void);
void token_store (const char *token);
void token_delete ();
char *token_init (char **error);


