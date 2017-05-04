/*---------------------------------------------------------------------------
dbcmd
curl.h
GPL v3.0
---------------------------------------------------------------------------*/

#pragma once

#include "bool.h"

char *curl_issue_create_folder (const char *token, const char *new_path, 
    char **error);
char *curl_issue_delete_path (const char *token, const char *path, 
    char **error);
void curl_issue_download_file (const char *token, const char *source, 
    const char *target, int *http_code, char **error);
char *curl_issue_get_file_info (const char *token, const char *path, 
    char **error);
char *curl_issue_list_files (const char *token, const char *path, 
    BOOL recursive, const char *cursor, char **error);
char *curl_issue_move (const char *token, const char *old_path, 
    const char *new_path, char **error);
char *curl_issue_token (const char *code, char **error);
void curl_issue_upload_file (const char *token, const char *source, 
    const char *target, int *http_code, char **error);



