# websendpraat

Praat (http://praat.org) is a popular phonetics tool developed by Paul Boersma and David Weenink at the University of Amsterdam. Praat can receive commands from other programs using a mechanism called "sendpraat".

*websendpraat*, developed by the NZILBB at the University of Canterbury (http://www.nzilbb.canterbury.ac.nz), adds some enhancements to the standard implementation of sendpraat:
* URLs (http or https) can be used instead of local file paths
* commands can be invoked using JSON
* the program can function as a browser extension Native Messaging Host (for Chrome and Firefox extensions)

Currently, there is only an implementation for OS X. Future plans include:
* Implementations for Linux and Windows.
* Browser extensions for Chrome, Firefox, and eventually Safari.

This has been developed primarily for use with LaBB-CAT, a brower-based linguistics tool (https://labbcat.canterbury.ac.nz), but can be used to open embedded audio in Praat from any web page.

--------------------------

A compiled version of websendpraat for OS X is available [here](bin/WebSendPraatMacx86_64)

It works just like sendpraat on the command line, e.g.

`WebSendPraatMacx86_64 Praat Read from file... /some/file.wav`

...except you can also supply URLs instead of file paths:

`WebSendPraatMacx86_64 Praat Read from file... http://example.org/some/file.wav`

Commands can also be expressed with JSON using a "sendpraatjson://" prefix:

`WebSendPraatMacx86_64 sendpraatjson://{message:'sendpraat', sendpraat: ['Praat', 'Read from file... http://example.org/some/file.wav', 'Edit']}`

websendpraat works as a Chrome Native Messaging Host if the first command line argument is not "Praat". It then accepts messages on stdin using Chrome's Native Messaging protocol (https://developer.chrome.com/extensions/nativeMessaging#native-messaging-host-protocol). The format for a message is:
```
    {
       "message" : "sendpraat",
       "sendpraat" : [
         "praat",
         "Quit"
       ]
    }
```
In addition to sendpraat commands, files that have been downloaded can be re-uploaded, so TextGrids can be downloaded, edited by the user, and then re-uploaded.  The format for upload messages is:
```
    {
        "message" : "upload", 
        "sendpraat" : [
           "praat",
           "select TextGrid " + nameInPraat, // name of a textgrid object in Praat
           "Write to text file... " + fileUrl // original URL of the downloaded file
        ], 
        "uploadUrl" : uploadUrl, // URL to upload to
        "fileParameter" : fileParameter, // name of file HTTP parameter
        "fileUrl" : fileUrl, // original URL of the downloaded file
        "otherParameters" : otherParameters // extra HTTP request parameters
    }
```
