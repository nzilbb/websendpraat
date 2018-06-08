//
//  web.h
//  WebSendPraat
//
//  Created by Robert Fromont on 31/05/18.
//  Copyright © 2018 New Zealand Institute of Language, Brain and Behaviour. All rights reserved.
//

#ifndef web_h
#define web_h

#include <stdio.h>
#include "sendpraat.h"
#include "cjson/cJSON.h"

/*
 * Converts all http:// and https:// URLs in the given script line to local file paths,
 * by downloading the content to a local file.
 * The caller is responsible for freeing the returned string.
 */
char* downloadHttpToLocal(char* line, char* authorization, void (*downloadProgress)(long,long), char** error);

/*
 * Finds all http:// or https:// URLs in the given script line and,
 * if they have already been downloaded using convertHttpToLocal() to local file paths,
 * rewrites them as local file names.
 * The caller is responsible for freeing the returned string.
 */
char* rewriteHttpToLocal(char* line);

/*
 * Upload the given file to the given URL.
 * Returns NULL on success, or an error message on failure.
 */
char* uploadFile(char* url, char* fileParameter, char* fileName, const cJSON* otherParameters, char* authorization, cJSON** response);

/*
 * Cleans up, by deleting downloaded files.
 */
void cleanupDownloads(void);

#endif /* web_h */
