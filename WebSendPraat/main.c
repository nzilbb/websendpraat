//
//  main.c
//  WebSendPraat
//
//  Created by Robert Fromont on 16/05/18.
//  Copyright © 2018 New Zealand Institute of Language, Brain and Behaviour. All rights reserved.
//

#include "web.h"
#include "json.h"
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

// use Paul Boersma's implementation...
#include "sendpraat.h"

void printUsage() {
    printf ("Web-enabled sendpraat can be used like standard sendpraat,\n");
    printf (" but supports useing http/https URLs instead of local file paths,\n");
    printf (" and can also process JSON messages, and function as a Chrome extension Native Messaging Host.\n");
    printf ("Syntax:\n");
#if win
    printf ("   sendpraat <program> <message>\n");
#else
    printf ("   sendpraat [<timeOut>] <program> <message>\n");
#endif
    printf ("\n");
    printf ("Arguments:\n");
    printf ("   <program>: the name of a running program that uses the Praat shell,\n");
    printf ("              or a sendpraatjson://... specifier for JSON request preocessing,\n");
    printf ("              or a chrome-extension://... specifier to start as a Native Messaaging Host.\n");
    printf ("   <message>: a sequence of Praat shell lines (commands and directives).\n");
#if ! win
    printf ("   <timeOut>: the number of seconds that sendpraat will wait for an answer\n");
    printf ("              before writing an error message. A <timeOut> of 0 means that\n");
    printf ("              the message will be sent asynchronously, i.e., that sendpraat\n");
    printf ("              will return immediately without issuing any error message.\n");
#endif
    printf ("\n");
    printf ("sendpraatjson:// specifiers include the full JSON for a request, e.g.\n");
    printf ("sendpraat 'sendpraatjson://{\"message\":\"sendpraat\",\"sendpraat\": [\"praat\",\"Read from file... http://example.com/example.wav\",\"Edit\"]}'\n");
    printf ("\n");
    printf ("Usage:\n");
    printf ("   Each line is a separate argument.\n");
    printf ("   Lines that contain spaces should be put inside double quotes.\n");
    printf ("\n");
    printf ("Examples:\n");
    printf ("\n");
#if win
    printf ("   sendpraat praat Quit\n");
#else
    printf ("   sendpraat 0 praat Quit\n");
#endif
    printf ("      Causes the program \"praat\" to quit (gracefully).\n");
    printf ("      This works because \"Quit\" is a fixed command in Praat's Control menu.\n");
#if ! win
    printf ("      Sendpraat will return immediately.\n");
#endif
    printf ("\n");
#if win
    printf ("   sendpraat praat \"Play reverse\"\n");
#else
    printf ("   sendpraat 1000 praat \"Play reverse\"\n");
#endif
    printf ("      Causes the program \"praat\", which can play sounds,\n");
    printf ("      to play the selected Sound objects backwards.\n");
    printf ("      This works because \"Play reverse\" is an action command\n");
    printf ("      that becomes available in Praat's dynamic menu when Sounds are selected.\n");
#if ! win
    printf ("      Sendpraat will allow \"praat\" at most 1000 seconds to perform this.\n");
#endif
    printf ("\n");
#if win
    printf ("   sendpraat praat \"execute C:\\MyDocuments\\MyScript.praat\"\n");
#else
    printf ("   sendpraat praat \"execute ~/MyResearch/MyProject/MyScript.praat\"\n");
#endif
    printf ("      Causes the program \"praat\" to execute a script.\n");
#if ! win
    printf ("      Sendpraat will allow \"praat\" at most 10 seconds (the default time out).\n");
#endif
    printf ("\n");
    printf ("   sendpraat als \"for i from 1 to 5\" \"Draw circle... 0.5 0.5 0.1*i\" \"endfor\"\n");
    printf ("      Causes the program \"als\" to draw five concentric circles\n");
    printf ("      into its Picture window.\n");
}

// NativeMessagingHost code owes a lot to:
//https://github.com/ForrestFeng/ChromeExtension/blob/master/NativeMessaging/C%2B%2BNativeMessagingHostSample/main.cpp

// Define union to read the message size easily
typedef union {
    unsigned long u32;
    unsigned char u8[4];
} U32_U8;

// send a JSON response back to the browser plugin
void sendResponseNativeMessagingHost(char* jsonResponse) {
    if (jsonResponse != NULL) {
        //fprintf (stderr, "Response: %s\n", jsonResponse);
        U32_U8 lenBuf;
        lenBuf.u32 = strlen(jsonResponse);
        fwrite(lenBuf.u8, 1, 4, stdout);
        fwrite(jsonResponse, 1, lenBuf.u32, stdout);
        fflush(stdout);
    } // there was a response
}
long lastSoFar = 0;
// download progress callback for Native Messaging Host
void downloadProgressNativeMessagingHost(long soFar, long total) {
    int report = FALSE;
    if ((soFar == 0 || soFar == total) && lastSoFar != soFar) report = TRUE;
    if ((((soFar - lastSoFar) * 100) / total) > 5) report = TRUE;
    if (report) {
        cJSON* reply = cJSON_CreateObject();
        cJSON_AddStringToObject(reply, "message", "progress");
        if (soFar < total) {
            cJSON_AddStringToObject(reply, "string", "Downloading...");
        } else {
            cJSON_AddStringToObject(reply, "string", "Downloaded.");
        }
        cJSON_AddNumberToObject(reply, "maximum", total);
        cJSON_AddNumberToObject(reply, "value", soFar);
        if (lastClientRef) cJSON_AddStringToObject(reply, "clientRef", lastClientRef);
        char* message = cJSON_Print(reply);
        sendResponseNativeMessagingHost(message);
        free(message);
        lastSoFar = soFar;
    }
}
// download progress callback for command line sendpraatjson:// invocation
void downloadProgressJSON(long soFar, long total) {
    int report = FALSE;
    if ((soFar == 0 || soFar == total) && lastSoFar != soFar) report = TRUE;
    if ((((soFar - lastSoFar) * 100) / total) > 5) report = TRUE;
    if (report) {
        cJSON* reply = cJSON_CreateObject();
        cJSON_AddStringToObject(reply, "message", "progress");
        cJSON_AddNumberToObject(reply, "maximum", total);
        cJSON_AddNumberToObject(reply, "value", soFar);
        if (lastClientRef) cJSON_AddStringToObject(reply, "clientRef", lastClientRef);
        char* message = cJSON_Print(reply);
        printf("%s\n", message);
        free(message);
        lastSoFar = soFar;
    }
}
// download progress callback for standard command line invocation
void downloadProgress(long soFar, long total) {
    int report = FALSE;
    if ((soFar == 0 || soFar == total) && lastSoFar != soFar) report = TRUE;
    if ((((soFar - lastSoFar) * 100) / total) > 5) report = TRUE;
    if (report) {
        printf(".");
        lastSoFar = soFar;
    }
}

// native messaging host - reads messages from stdin and writes responses to stdout
// https://developer.chrome.com/extensions/nativeMessaging
void nativeMessagingHost() {
    // The format of raw message received from the stdin
    // IIIISSSSS...SS
    // IIII is a 4bytes integer in binary format. It is the lenth of the following json message.
    // SSSSS...SS is the json message immidiatlly follows the 4 bytes header.
    // Each message is serialized using JSON, UTF-8 encoded and is preceded with 32-bit message length in native byte order.
    // You can send message back to chromium plugin by writing the same formated IIIISSSSS...SS to stdout.

    size_t iSize = 0;
    U32_U8 lenBuf;
    lenBuf.u32 = 0;
    while (TRUE) {
        fprintf (stderr, "Waiting for message...\n");
        iSize = fread(lenBuf.u8, 1, 4, stdin);
        if (iSize == 4) {
            int iLen = (int)lenBuf.u32;
            // now read the message
            if (iLen > 0) {
                char* jsonMsg = (char*)malloc(8 * iLen);
                iSize = fread(jsonMsg, 1, iLen, stdin);

                // process message
                lastSoFar = 0;
                char* jsonResponse = jsonMessage(jsonMsg, downloadProgressNativeMessagingHost);
                sendResponseNativeMessagingHost(jsonResponse);
                free(jsonResponse);
                free(jsonMsg);
            } // there was a message
            
            //uncomment it to debug the messaging
            /*FILE* log = fopen("D:\\native.txt", "w");
             fwrite((void*)lenBuf.u8, 1, 4, log);
             fwrite(utf8Buffer, 1, iLen, log);
             fclose(log);*/
            
        } else { // 4 bytes not read
            fprintf (stderr, "Cleaning up...\n");
            cleanupDownloads();
            fprintf (stderr, "Done.\n");
            exit(1); // take this as a sign to quit
        }
    } // next message
}

// ...but with a different main function
int main (int argc, char **argv) {
    int iarg, line, length = 0;
    long timeOut = 10;   /* Default. */
    char programName [64], *message, *result;
    if (argc == 1) {
        printUsage();
        exit (0);
    }
    
    /* if there's one argument that starts will "sendpraatjson://" */
    if (argc == 2 && strstr(argv[1], "sendpraatjson://") == argv[1]) {
        // process JSON message
        lastSoFar = 0;
        char* reply = jsonMessage(argv[1] + 16, &downloadProgressJSON);
        // print the reply directly to stdout
        printf("%s", reply);
        exit(0);
    }

    /* if the first argument starts will "chrome-extension://" */
    if ((argc >= 2 && strstr(argv[1], "chrome-extension://") == argv[1]) // Chrome
        || (argc >= 2 && strcmp(argv[1], "@jsendpraat") == 0)            // Firefox
        || (argc >= 3 && strcmp(argv[2], "@jsendpraat") == 0)) {         // Firefox
        nativeMessagingHost();
        exit(0);
    }

    /* Otherwise, continue as a normal command-line invocation... */
    iarg = 1;
    
#if ! win
    /*
     * Get time-out.
     */
    if (isdigit (argv [iarg] [0])) timeOut = atol (argv [iarg ++]);
#endif
    
    /*
     * Get program name.
     */
    if (iarg == argc) {
        fprintf (stderr, "sendpraat: missing program name. Type \"sendpraat\" to get help.\n");
        return 1;
    }
    strcpy (programName, argv [iarg ++]);
    
    /*
     * Create the message string.
     */
    for (line = iarg; line < argc; line ++) length += strlen (argv [line]) + 1;
    length --;
    message = malloc (length + 1);
    message [0] = '\0';
    for (line = iarg; line < argc; line ++) {
        char* downloadError = NULL;
        char* localLine = downloadHttpToLocal(argv [line], NULL, &downloadProgress, &downloadError);
        if (downloadError) {
            fprintf (stderr, "sendpraat: Download error: %s\n", downloadError);
            exit(600);
        }
        strcat (message, localLine);
        free(localLine); // free memory used to build the local version of the line
        if (line < argc - 1) strcat (message, "\n");
    }
    
    /*
     * Send message.
     */
    result = sendpraat (NULL, programName, timeOut, message);
    if (result != NULL) {
        // maybe praat's simply not running
        startPraat();
        // try again
        result = sendpraat (NULL, programName, timeOut, message);
        if (result != NULL) {
            fprintf (stderr, "sendpraat: %s\n", result); exit (1);
        }
    }
    
    exit (0);
    return 0;
}

/* start Praat... */
void startPraat(void) {
    char* praatPath;
#if mac
    praatPath = "/Applications/Praat.app/Contents/MacOS/Praat";
#endif
    if (fork() == 0) { // child process
        // start praat
        execv(praatPath, (char *[]){ praatPath, NULL });
        exit(0);
    } else { // parent process
        // pause to give Praat a chance to start...
        sleep(1);
    }
}
