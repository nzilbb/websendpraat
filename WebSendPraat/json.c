//
//  json.c
//  WebSendPraat
//
//  Created by Robert Fromont on 31/05/18.
//  Copyright © 2018 New Zealand Institute of Language, Brain and Behaviour. All rights reserved.
//

#include "json.h"

#include <string.h>
#include "web.h"
#include "sendpraat.h"


/* Processes a JSON message, and returns the JSON reply */
char* jsonMessage(char* jsonString, void (*downloadProgress)(long,long)) { // TODO leaky
    //fprintf (stderr, "Message: %s\n", jsonString);
    cJSON* reply = cJSON_CreateObject();
    cJSON *json = cJSON_Parse(jsonString);
    if (json == NULL) {
        cJSON_AddStringToObject(reply, "message", "sendpraat");
        cJSON_AddNumberToObject(reply, "code", 900);
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            char message[1024];
            sprintf(message, "Error before: %s\n", error_ptr);
            cJSON_AddStringToObject(reply, "error", message); // TODO does using sprintf like this leak memory?
        } else {
            cJSON_AddStringToObject(reply, "error", "Could not parse JSON.");
        }
    }
    const cJSON* clientRef = cJSON_GetObjectItemCaseSensitive(json, "clientRef");
    char* authorization = NULL;
    const cJSON* authorizationElement = cJSON_GetObjectItemCaseSensitive(json, "authorization");
    if (authorizationElement && cJSON_IsString(authorizationElement) && authorizationElement->valuestring) {
        authorization = authorizationElement->valuestring;
    }

    const cJSON* message = cJSON_GetObjectItemCaseSensitive(json, "message");
    if (message == NULL || !cJSON_IsString(message) || (message->valuestring == NULL)) {
        cJSON_AddStringToObject(reply, "message", "sendpraat");
        cJSON_AddNumberToObject(reply, "code", 800);
        cJSON_AddStringToObject(reply, "error", "No message specified.");
    } else {
        cJSON_AddStringToObject(reply, "message", message->valuestring);

        if (strcmp(message->valuestring, "version") == 0) {
            cJSON_AddNumberToObject(reply, "code", 0);
            cJSON_AddStringToObject(reply, "version", "20180606.1132");
            
        } else if (strcmp(message->valuestring, "sendpraat") == 0) {
            const cJSON* arguments = cJSON_GetObjectItemCaseSensitive(json, "sendpraat");
            if (!cJSON_IsArray(arguments)) {
                cJSON_AddNumberToObject(reply, "code", 501);
                cJSON_AddStringToObject(reply, "error", "sendpraat is not an array.");
            } else {
                char message[2048];
                message[0] = '\0';
                char* programName = NULL;
                int argCount = cJSON_GetArraySize(arguments);
                char* downloadError = NULL;
                for (int i = 0; i < argCount; i++) {
                    const cJSON* argument = cJSON_GetArrayItem(arguments, i);
                    if (cJSON_IsString(argument) && argument->valuestring != NULL) {
                        if (!programName) { // first argument is program name
                            programName = argument->valuestring;
                        } else { // subsequent arguments are script lines
                            if (strlen(message) > 0) strcat(message, "\n");
                            char* line = downloadHttpToLocal(argument->valuestring, authorization, downloadProgress, &downloadError);
                            strcat(message, line);
                            free(line);
                        }
                    } // item is a string
                } // next argument
                if (downloadError) {
                    cJSON_AddStringToObject(reply, "error", downloadError);
                    cJSON_AddNumberToObject(reply, "code", 600);
                } else {
                    char* result = sendpraat (NULL, "Praat", 10, message);
                    if (result != NULL) {
                        // maybe praat's simply not running
                        startPraat();
                        // try again
                        result = sendpraat (NULL, "Praat", 10, message);
                    }
                    if (result != NULL) {
                        cJSON_AddStringToObject(reply, "error", result);
                        cJSON_AddNumberToObject(reply, "code", 1);
                    } else {
                        cJSON_AddNumberToObject(reply, "code", 0);
                    }
                }
            }
            
        } else if (strcmp(message->valuestring, "upload") == 0) {
            const cJSON* uploadUrl = cJSON_GetObjectItemCaseSensitive(json, "uploadUrl");
            if (uploadUrl == NULL || !cJSON_IsString(uploadUrl) || (uploadUrl->valuestring == NULL)) {
                cJSON_AddNumberToObject(reply, "code", 801);
                cJSON_AddStringToObject(reply, "error", "uploadUrl not supplied.");
            } else {
                const cJSON* fileUrl = cJSON_GetObjectItemCaseSensitive(json, "fileUrl");
                if (fileUrl == NULL || !cJSON_IsString(fileUrl) || (fileUrl->valuestring == NULL)) {
                    cJSON_AddNumberToObject(reply, "code", 802);
                    cJSON_AddStringToObject(reply, "error", "fileUrl not supplied.");
                } else {
                    const cJSON* fileParameter = cJSON_GetObjectItemCaseSensitive(json, "fileParameter");
                    if (fileParameter == NULL || !cJSON_IsString(fileParameter) || (fileParameter->valuestring == NULL)) {
                        cJSON_AddNumberToObject(reply, "code", 803);
                        cJSON_AddStringToObject(reply, "error", "fileParameter not supplied.");
                    } else {
                        const cJSON* otherParameters = cJSON_GetObjectItemCaseSensitive(json, "otherParameters");
                        const cJSON* arguments = cJSON_GetObjectItemCaseSensitive(json, "sendpraat");
                        if (!cJSON_IsArray(arguments)) {
                            cJSON_AddNumberToObject(reply, "code", 501);
                            cJSON_AddStringToObject(reply, "error", "sendpraat is not an array.");
                        } else {
                            char message[2048];
                            message[0] = '\0';
                            char* programName = NULL;
                            int argCount = cJSON_GetArraySize(arguments);
                            for (int i = 0; i < argCount; i++) {
                                const cJSON* argument = cJSON_GetArrayItem(arguments, i);
                                if (cJSON_IsString(argument) && argument->valuestring != NULL) {
                                    if (!programName) { // first argument is program name
                                        programName = argument->valuestring;
                                    } else { // subsequent arguments are script lines
                                        if (strlen(message) > 0) strcat(message, "\n");
                                        char* line = rewriteHttpToLocal(argument->valuestring);
                                        strcat(message, line);
                                        free(line);
                                    }
                                } // item is a string
                            } // next argument
                            char* result = sendpraat (NULL, "Praat", 10, message);
                            if (result != NULL) {
                                // maybe praat's simply not running
                                startPraat();
                                // try again
                                result = sendpraat (NULL, "Praat", 10, message);
                            }
                            if (result != NULL) {
                                cJSON_AddStringToObject(reply, "error", result);
                                cJSON_AddNumberToObject(reply, "code", 1);
                            } else { // sendpraat succeeded
                                // upload the file
                                char* filePath = rewriteHttpToLocal(fileUrl->valuestring);
                                cJSON* uploadResponse = NULL;
                                char* error = uploadFile(uploadUrl->valuestring, fileParameter->valuestring, filePath, otherParameters, authorization, &uploadResponse);
                                free(filePath);
                                //fprintf(stderr, "finished uploadfile %s\n", error);
                                if (error || uploadResponse == NULL) {
                                    if (error) {
                                        cJSON_AddStringToObject(reply, "error", error);
                                        free(error);
                                    }
                                    cJSON_AddNumberToObject(reply, "code", 700);
                                } else {
                                    // discard our pre-prepared JSON object
                                    free(reply);
                                    // and return the one returned by the server
                                    reply = uploadResponse;
                                    cJSON_AddStringToObject(reply, "message", "upload");
                                    cJSON_AddNumberToObject(reply, "code", 0);
                                    //fprintf(stderr, "UPLOAD: %s", cJSON_Print(reply));
                                }
                            }
                        } // arguments ok
                    } // fileParameter ok
                } // fileUrl ok
            } // uploadUrl ok
        } else { // unknown message
            cJSON_AddNumberToObject(reply, "code", 700);
            cJSON_AddStringToObject(reply, "error", "Unknown message.");
        }
    }
    
    if (clientRef != NULL && clientRef->valuestring) {
        cJSON_AddStringToObject(reply, "clientRef", clientRef->valuestring);
        lastClientRef = clientRef->valuestring;
    } else {
        lastClientRef = NULL;
    }
    return cJSON_Print(reply);
}
