//
//  web.c
//  WebSendPraat
//
//  Created by Robert Fromont on 31/05/18.
//  Copyright © 2018 New Zealand Institute of Language, Brain and Behaviour. All rights reserved.
//

#include "web.h"

#include <curl/curl.h>
#include "c_hashmap/hashmap.h"

static map_t urlToLocal = NULL;
char statusErrorBuffer[1024];

/* header callback */
size_t header_callback(char *buffer,   size_t size,   size_t nitems,   void *userdata)
{
    char header[nitems * size + 1];
    strncpy(header, buffer, nitems * size);
    char* newline = strchr(header, '\n');
    if (newline) *newline = '\0';
    newline = strchr(header, '\r');
    if (newline) *newline = '\0';
    char* filenamespec = strstr(header, "filename=");
    if (filenamespec) {
        strcpy(userdata, filenamespec + 9);
    }
    return nitems * size;
}

/* download progress callback */
static int xferinfo(void *p,
                    curl_off_t dltotal, curl_off_t dlnow,
                    curl_off_t ultotal, curl_off_t ulnow)
{
    if (dltotal > 0) {
        if (p) {
            void (*downloadProgress)(long,long);
            downloadProgress = p;
            (*downloadProgress)(dlnow, dltotal);
        }
        //printf("progress: %ld%\n", ((100*dlnow) / dltotal));
    }
    return 0;
}

/*
 * Converts all http:// and https:// URLs in the given script line to local file paths,
 * by downloading the content to a local file.
 */
char* downloadHttpToLocal(char* line, char* authorization, void (*downloadProgress)(long,long), char** error) {
    // ensure we've initialized our URL/filename map
    if (!urlToLocal) {
        urlToLocal = hashmap_new();
    }
    
    char* authorizationHeader = malloc(1024);
    if (authorization) {
        /* create authorization header */
        sprintf(authorizationHeader, "Authorization: %s", authorization);
    }

    char* local = malloc (strlen(line) * 2); // TODO is 2x the string length enough?
    local[0] = '\0';
    char* token;
    const char delimiter[2] = " ";
    
    /* get the first token */
    token = strtok(line, delimiter);
    
    /* walk through other tokens */
    while( token != NULL ) {
        if (token != line) { // not the first token
            strcat(local, " "); // add delimiter
        }
        
        // is the token a URL?
        if (strstr(token, "http://") == token || strstr(token, "https://") == token) {
            //fprintf (stderr, "Getting %s\n", token);
            // download content to a local file
            CURL* curl;
            CURLcode res;
            char* tempfilename = tmpnam(NULL);
            FILE* urlfile;
            // final name of the file - default to the last part of the URL path (but this might change)
            char* localfilename = malloc(256);
            // put the final file in the same directory as the temp file
            strcpy(localfilename, tempfilename);
            char* lastslashintempfilename = strrchr(localfilename, '/');
            strcpy(lastslashintempfilename + 1, strrchr(token, '/') + 1);
            // ignore the query string, if any
            char* querystringstart = strchr(localfilename, '?');
            if (querystringstart) *querystringstart = '\0';

            curl = curl_easy_init();
            if(curl) {
                curl_easy_setopt(curl, CURLOPT_URL, token);
                /* tell libcurl to follow redirection */
                curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                
                /* open the file */
                urlfile = fopen(tempfilename, "wb");
                if(urlfile) {
                    struct curl_slist *headerlist = NULL;
                    if (authorization) {
                        /* add authorization header */
                        headerlist = curl_slist_append(headerlist, authorizationHeader);
                        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
                    }
                    
                    /* callbacks and options */
                    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
                    curl_easy_setopt(curl, CURLOPT_HEADERDATA, lastslashintempfilename + 1);
                    curl_easy_setopt(curl, CURLOPT_WRITEDATA, urlfile);
                    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferinfo);
                    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, downloadProgress);
                    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
                    
                    /* Perform the request, res will get the return code */
                    res = curl_easy_perform(curl);
                    
                    /* close the content file */
                    fclose(urlfile);
                    
                    /* Check for errors */
                    if(res != CURLE_OK) {
                        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
                        *error = curl_easy_strerror(res);
                        remove(tempfilename); // delete the temporary file
                    } else {
                        long response_code;
                        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
                        if (response_code != 200) {
                            char* explanation = "";
                            switch (response_code) {
                                case 401: explanation = " (unauthorized)"; break;
                                case 403: explanation = " (forbidden)"; break;
                                case 404: explanation = " (not found)"; break;
                                case 408: explanation = " (request timeout)"; break;
                                case 504: explanation = " (gateway timeout)"; break;
                            }
                            sprintf(statusErrorBuffer, "Response code: %ld%s", response_code, explanation);
                            *error = statusErrorBuffer;
                            fprintf (stderr, "ERROR: %s\n", *error);
                            remove(tempfilename); // delete the temporary file
                        } else {
                            // rename content file to something sensible
                            //fprintf(stderr, "%s -> %s\n", tempfilename, localfilename);
                            if (rename(tempfilename, localfilename) != 0) {
                                sprintf(statusErrorBuffer, "Could not rename %s to %s\n", tempfilename, localfilename);
                                fprintf(stderr, statusErrorBuffer);
                                *error = statusErrorBuffer;
                                remove(tempfilename); // delete the temporary file
                            } else {
                                // get canonical path of local file
                                char *full_path = realpath(localfilename, NULL);
                                strcpy(localfilename, full_path);
                                free(full_path);

                                // remember while file the URL was saved as
                                hashmap_put(urlToLocal, token, localfilename);
                                fprintf(stderr, "%s -> %s\n", token, localfilename);

                                // reference that in the line
                                token = localfilename;
                            }
                        } // response code ok
                    } // request ok
                } else {
                    fprintf(stderr, "Could not open output file: %s\n", tempfilename);
                    *error = "Could not open output file.";
                }
                /* always cleanup */
                curl_easy_cleanup(curl);
            } else { // curl_easy_init failed
                fprintf(stderr, "curl_easy_init() failed\n");
                *error = "Could not initialize curl.";
            }
        } // is URL
        // add the token to the local version of the line
        strcat(local, token);
        
        // next token
        token = strtok(NULL, delimiter);
    } // next token
    
    free(authorizationHeader);
    return local;
}

/*
 * Finds all http:// or https:// URLs in the given script line and,
 * if they have already been downloaded using convertHttpToLocal() to local file paths,
 * rewrites them as local file names.
 */
char* rewriteHttpToLocal(char* line) {
    char* local = malloc (strlen(line) * 2); // TODO is 2x the string length enough?
    local[0] = '\0';
    char* token;
    const char delimiter[2] = " ";
    
    /* get the first token */
    token = strtok(line, delimiter);
    
    /* walk through other tokens */
    while( token != NULL ) {
        if (token != line) { // not the first token
            strcat(local, " "); // add delimiter
        }
        
        // is the token a URL?
        if (strstr(token, "http://") == token || strstr(token, "https://") == token) {
            // file the local file
            if (urlToLocal) {
                char* localfilename;
                int error = hashmap_get(urlToLocal, token, (void**)(&localfilename));
                if (error == MAP_OK) {
                    //fprintf(stderr, "found %s -> %s\n", token, localfilename);
                    // reference that in the line
                    token = localfilename;
                } // found local filename
            } // map has been initialised
        } // is URL
        
        // add the token to the local version of the line
        strcat(local, token);
        
        // next token
        token = strtok(NULL, delimiter);
    } // next token
    
    return local;
}

const int JSON_BUFFER_SIZE = 4096;
static size_t write_json(char *in, size_t size, size_t nmemb, char *out)
{
    size_t toRead = size * nmemb;
    int alreadyRead = strlen(out);
    if (toRead > JSON_BUFFER_SIZE-alreadyRead-1) toRead = JSON_BUFFER_SIZE-alreadyRead-1;
    strncpy(out+alreadyRead, in, toRead);
    *(out+alreadyRead+toRead) = '\0'; // ensure it's null terminated
    return toRead;
}

/*
 * Upload the given file to the given URL.
 * Returns NULL on success, or an error message on failure.
 */
char* uploadFile(char* url, char* fileParameter, char* fileName, const cJSON* otherParameters, char* authorization, cJSON** response) {
    char* error = NULL;
    CURL* curl;
    CURLcode res;
    struct curl_httppost *formpost = NULL;
    struct curl_httppost *lastptr = NULL;
    struct curl_slist *headerlist = NULL;
    
    /* Ask for JSON */
    headerlist = curl_slist_append(headerlist, "Accept: application/json");
    
    char* authorizationHeader = malloc(1024);
    if (authorization) {
        /* add authorization header */
        sprintf(authorizationHeader, "Authorization: %s", authorization);
        headerlist = curl_slist_append(headerlist, authorizationHeader);
    }
    
    if (otherParameters) {
        /* Fill in the other parameters */
        const cJSON *parameter = NULL;
        cJSON_ArrayForEach(parameter, otherParameters)
        {
            if (cJSON_IsString(parameter)) {
                curl_formadd(&formpost,
                             &lastptr,
                             CURLFORM_COPYNAME, parameter->string,
                             CURLFORM_COPYCONTENTS, parameter->valuestring,
                             CURLFORM_END);
            } // string value
        } // next parameter
    } // there are other parameters
    
    /* Fill in the file upload field */
    char* name = strrchr(fileName, '/') + 1;
    if (!name) name = fileName;
    curl_formadd(&formpost,
                 &lastptr,
                 CURLFORM_COPYNAME, fileParameter,
                 CURLFORM_FILE, fileName,
                 CURLFORM_FILENAME, name,
                 CURLFORM_CONTENTTYPE, "application/octet-stream",
                 CURLFORM_END);
    curl = curl_easy_init();
    if(curl) {

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
        // capture and return response
        char jsonString[JSON_BUFFER_SIZE];
        jsonString[0] = '\0';
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_json);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, jsonString);

        /* Perform the request, res will get the return code */
        res = curl_easy_perform(curl);
        /* Check for errors */
        if(res != CURLE_OK) {
            error = curl_easy_strerror(res);
            fprintf(stderr, "upload error %s\n", error);
        } else {
            // make response available to caller
            (*response) = cJSON_Parse(jsonString);
            if ((*response) == NULL) {
                char* jsonError = cJSON_GetErrorPtr();
                error = malloc(strlen(jsonError) + 20);
                sprintf(error, "JSON error before: %s\n", jsonError);
                fprintf(stderr, "%s\n", error);
            }
        }
        
        /* always cleanup */
        curl_easy_cleanup(curl);
        
        /* then cleanup the formpost chain */
        curl_formfree(formpost);
        /* free slist */
        curl_slist_free_all(headerlist);
    } else { // curl_easy_init failed
        error = "curl_easy_init() failed.";
    }
    //fprintf(stderr, "returning from uploadFile: %s\n", error);
    free(authorizationHeader);
    return error;
}

int deleteFile(any_t item, any_t data) {
    if (remove(data) != 0) fprintf(stderr, "Could not delete: %s\n", data);
    return MAP_OK;
}

/*
 * Cleans up, by deleting downloaded files.
 */
void cleanupDownloads(void) {
    hashmap_iterate(urlToLocal, deleteFile, NULL);
    hashmap_free(urlToLocal);
}
