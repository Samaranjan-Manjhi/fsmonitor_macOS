#include <CoreServices/CoreServices.h>
#include <stdio.h>
#include <libgen.h>  // For basename()
#include <fcntl.h>
#include <unistd.h>
#include <sys/param.h> // For MAXPATHLEN
#include <stdlib.h>
#include <time.h>

int mv_flag = 0;
FILE *log_file = NULL;

// Function to get the current timestamp
void getCurrentTime(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *local_time = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", local_time);
}

// Function to print and log event flags
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
        char *fileName = basename(paths[i]);  // Extracts just the file or directory name
        const char *action = NULL;

        if (eventFlags[i] & kFSEventStreamEventFlagItemCreated) {
            action = "Created";
        } else if (eventFlags[i] & kFSEventStreamEventFlagItemRemoved) {
            action = "Deleted";
        } else if (eventFlags[i] & kFSEventStreamEventFlagItemModified) {
            action = "Modified";
        } else if (eventFlags[i] & kFSEventStreamEventFlagItemRenamed) {
            if (mv_flag == 0) {
                action = "Renamed (Source)";
                mv_flag = 1;
            } else {
                action = "Renamed (Destination)";
                mv_flag = 0;
            }
        }

        if (action) {
            logEvent(action, paths[i], fileName);
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

