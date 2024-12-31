#include <CoreServices/CoreServices.h>
#include <stdio.h>
#include <libgen.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/param.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

int s_flag=0, d_flag=0;
FILE *log_file = NULL;
char last_rename_source[MAXPATHLEN] = {0}; // Store the last rename source path
char last_logged_path[MAXPATHLEN] = {0};   // Store the last logged path to prevent duplicates

// List of filenames to exclude from logging
const char *excluded_files[] = {
    "LiveFiles.ticotsord",
    "mwconnect.log",
    "libmwshare.log",
    "escanmon.log",
    "clients.plist",
    "fsevents_log.txt",
    "launchd.log",
    "softwarelist_data.txt",
    "softwares.list",
    "softwares.list.tmp",
    "mwsystem_profiler",
    ".viminfo",
    "NSIRD_locationd_eInWAL",
    NULL // Null-terminated for iteration
};

// Function to check if a filename is in the exclusion list
int isExcludedFile(const char *filename) {
    for (const char **excluded = excluded_files; *excluded != NULL; excluded++) {
        if (strcmp(filename, *excluded) == 0) {
            return 1; // Match found in exclusion list
        }
    }
    return 0; // No match found
}

// Function to get the current timestamp
void getCurrentTime(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *local_time = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", local_time);
}

// Function to log events
void logEvent(const char *action, const char *path, const char *filename) {
    char time_buffer[20];
    getCurrentTime(time_buffer, sizeof(time_buffer));

    // Write log entry
    if (log_file) {
        fprintf(log_file, "%s, %s, %s, %s\n", time_buffer, action, path, filename);
        fflush(log_file); // Ensure the data is written to the file immediately
    }
}

// Callback for FSEvents
void eventCallback(
    ConstFSEventStreamRef streamRef,
    void *clientCallBackInfo,
    size_t numEvents,
    void *eventPaths,
    const FSEventStreamEventFlags eventFlags[],
    const FSEventStreamEventId eventIds[]) {

    char **paths = (char **)eventPaths;
    for (size_t i = 0; i < numEvents; i++) {
        char *fileName = basename(paths[i]);

        // Skip logging if the file is in the exclusion list
        if (isExcludedFile(fileName)) {
            continue;
        }

        const char *action = NULL;

        if (eventFlags[i] & kFSEventStreamEventFlagItemCreated) {
            action = "Created";
            s_flag=0;
        } else if (eventFlags[i] & kFSEventStreamEventFlagItemRemoved) {
            action = "Deleted";
            s_flag=0;
        } else if (eventFlags[i] & kFSEventStreamEventFlagItemModified) {
            action = "Modified";
            s_flag=0;
        } else if (eventFlags[i] & kFSEventStreamEventFlagItemRenamed) {
            // Handle renames with source-destination pairing
            if (strlen(last_rename_source) == 0) {
                // Record the source path
                snprintf(last_rename_source, MAXPATHLEN, "%s", paths[i]);
                logEvent("Moved (Source)", paths[i], fileName);
                s_flag=1;
            } else if (strcmp(last_rename_source, paths[i]) != 0 && s_flag==1) {
                // Log the destination path only if it differs from the source
                logEvent("Moved (Destination)", paths[i], fileName);
                memset(last_rename_source, 0, sizeof(last_rename_source)); // Clear the source
                s_flag=0;
            }
            continue; // Skip further processing for rename events
        }

        // Prevent duplicate logs
        if (action && strcmp(last_logged_path, paths[i]) != 0) {
            logEvent(action, paths[i], fileName);
            snprintf(last_logged_path, MAXPATHLEN, "%s", paths[i]);
        }
    }
}

int main(int argc, const char *argv[]) {
    // Open the log file
    log_file = fopen("/tmp/fsevents_log.txt", "a");
    if (!log_file) {
        perror("Failed to open log file");
        exit(1);
    }

    // Write header to the log file if empty
    fseek(log_file, 0, SEEK_END);
    if (ftell(log_file) == 0) {
        fprintf(log_file, "Timestamp, Action, Path, Filename\n");
    }

    // Default to watching the root directory if no directory is provided
    const char *path = (argc < 2) ? "/" : argv[1];

    CFStringRef pathToWatch = CFStringCreateWithCString(NULL, path, kCFStringEncodingUTF8);
    CFArrayRef pathsToWatch = CFArrayCreate(NULL, (const void **)&pathToWatch, 1, NULL);

    FSEventStreamContext context = {0, NULL, NULL, NULL, NULL};
    FSEventStreamRef stream = FSEventStreamCreate(
        NULL,
        &eventCallback,
        &context,
        pathsToWatch,
        kFSEventStreamEventIdSinceNow,
        1.0, // Latency in seconds
        kFSEventStreamCreateFlagFileEvents // Report individual file events
    );

    FSEventStreamScheduleWithRunLoop(stream, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    FSEventStreamStart(stream);

    printf("Monitoring directory: %s\n", path);
    CFRunLoopRun();

    FSEventStreamStop(stream);
    FSEventStreamInvalidate(stream);
    FSEventStreamRelease(stream);
    CFRelease(pathsToWatch);
    CFRelease(pathToWatch);

    // Close the log file
    fclose(log_file);

    return 0;
}

