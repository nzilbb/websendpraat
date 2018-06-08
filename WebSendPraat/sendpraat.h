//
//  sendpraat.h
//  WebSendPraat
//
//  Declarations for Paul Boersma's functions
//  Created by Robert Fromont on 30/05/18.
//  Copyright © 2018 New Zealand Institute of Language, Brain and Behaviour. All rights reserved.
//

#ifndef sendpraat_h
#define sendpraat_h

#if defined (_WIN32)
    #include <windows.h>
    #include <stdio.h>
    #include <wchar.h>
    #ifdef __MINGW32__
        #define swprintf  _snwprintf
    #endif
    #define gtk 0
    #define win 1
    #define mac 0
#elif (defined (macintosh) || defined (__MACH__))
    #include <Carbon/Carbon.h>
    #include <wchar.h>
    #define gtk 0
    #define win 0
    #define mac 1
#elif defined (UNIX)
    #include <sys/types.h>
    #include <signal.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <unistd.h>
    #include <ctype.h>
    #include <wchar.h>
    #if defined (NO_GRAPHICS) || defined (NO_GUI)   /* for use inside Praat */
        #define gtk 0
    #else
        #include <gtk/gtk.h>
        #define gtk 1
    #endif
    #define win 0
    #define mac 0
#else
    #include <wchar.h>
    #define gtk 0
    #define win 0
    #define mac 0
#endif

#include <sys/types.h>
#include <unistd.h>

char *sendpraat (void *display, const char *programName, long timeOut, const char *text);
wchar_t *sendpraatW (void *display, const wchar_t *programName, long timeOut, const wchar_t *text);
void startPraat(void); /* defined in main.c */
#endif /* sendpraat_h */
