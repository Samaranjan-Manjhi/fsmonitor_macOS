#include <CoreServices/CoreServices.h>
#include <stdio.h>
#include <libgen.h>  // For basename()
#include <fcntl.h>
#include <unistd.h>
#include <sys/param.h> // For MAXPATHLEN
#include <stdlib.h>

int mv_flag = 0;

// Function to print event flags
void printEventFlags(FSEventStreamEventFlags flags) {
    if (flags & kFSEventStreamEventFlagItemCreated) {
        printf("Item Created | ");
    }
    if (flags & kFSEventStreamEventFlagItemRemoved) {
        printf("Item Removed | ");
    }
    if (flags & kFSEventStreamEventFlagItemRenamed) {
        printf("Item Renamed or Moved | ");
    }
    if (flags & kFSEventStreamEventFlagItemModified) {
        printf("Item Modified | ");
    }
    if (flags & kFSEventStreamEventFlagItemIsFile) {
        printf("Item is File | ");
    }
    if (flags & kFSEventStreamEventFlagItemIsDir) {
        printf("Item is Directory | ");
    }
    if (flags & kFSEventStreamEventFlagItemInodeMetaMod) {
        printf("Item Inode Metadata Modified | ");
    }
    if (flags & kFSEventStreamEventFlagItemFinderInfoMod) {
        printf("Item Finder Info Modified | ");
    }
    if (flags & kFSEventStreamEventFlagItemChangeOwner) {
        printf("Item Owner Changed | ");
    }
    if (flags & kFSEventStreamEventFlagItemXattrMod) {
        printf("Item Extended Attribute Modified | ");
    }
}

// Daemonize function to detach from the terminal and run as a background service
void daemonize() {
    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork failed");
        exit(1);
    }

    if (pid > 0) {
        // Parent process exits to let the child process become a daemon
        exit(0);
    }

    // Create a new session and detach from terminal
    if (setsid() < 0) {
        perror("setsid failed");
        exit(1);
    }

    // Change the current working directory to root
    if (chdir("/") < 0) {
        perror("chdir failed");
        exit(1);
    }

    // Close file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    // Redirect stdout and stderr to a log file
    int log_fd = open("/tmp/fsevents_monitor.log", O_RDWR | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
    if (log_fd < 0) {
        perror("Could not open log file");
        exit(1);
    }
    dup2(log_fd, STDOUT_FILENO);
    dup2(log_fd, STDERR_FILENO);
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
        printf("File Name: %s\n", fileName);
        printEventFlags(eventFlags[i]);
        printf("\n");
        
        if (eventFlags[i] & kFSEventStreamEventFlagItemCreated) {
            printf("  File/Folder Created: %s\n", fileName);
        }
        if (eventFlags[i] & kFSEventStreamEventFlagItemRemoved) {
            printf("  File/Folder Deleted: %s\n", fileName);
        }
        if (eventFlags[i] & kFSEventStreamEventFlagItemModified) {
            printf("  File/Folder Modified: %s\n", fileName);
        }
        if (eventFlags[i] & kFSEventStreamEventFlagItemRenamed) {
            printf("File/Folder Renamed or Moved: %s\n", fileName);
            if (mv_flag == 0) {
                printf("Source Path: %s\n", paths[i]);
                mv_flag = 1;
            } else {
                printf("Destination Path: %s\n", paths[i]);
                mv_flag = 0;
            }
        }
        printf("Change detected in: %s\n", paths[i]);
    }
}

int main(int argc, const char *argv[]) {
    // Daemonize the process
    daemonize();

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
    
    return 0;
}

