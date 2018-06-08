//
//  json.h
//  WebSendPraat
//
//  Created by Robert Fromont on 31/05/18.
//  Copyright © 2018 New Zealand Institute of Language, Brain and Behaviour. All rights reserved.
//

#ifndef json_h
#define json_h

#include <stdio.h>
#include "cjson/cJSON.h"

/* Processes a JSON message, and returns the JSON reply (which the caller is responsible for freeing) */
char* jsonMessage(char* json, void (*downloadProgress)(long,long));

/* The last clientRef, for passing back with progress notifications */
static char* lastClientRef;

#endif /* json_h */
