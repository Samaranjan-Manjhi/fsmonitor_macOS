#include <CoreServices/CoreServices.h>
#include <iostream>
#include <fstream>
#include <libgen.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/param.h>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <vector>

#define MAX_LOG_FILE_SIZE (5 * 1024 * 1024) // 5 MB
#define LOG_FILE_PATH "/tmp/fsevents_log.txt"
//#define LOG_FILE_PATH "/var/log/fsevents_monitor.log"

std::ofstream log_file;
std::string last_rename_source = ""; // Store the last rename source path
std::string last_logged_path = "";   // Store the last logged path to prevent duplicates
int s_flag = 0;
int open_flag = 0;
std::string open_filename = "";
std::string source_cp_prev="";
std::string source_cp_current="";
int count=0;
// List of filenames to exclude from logging
const std::vector<std::string> excluded_files = {
    "LiveFiles.ticotsord", "mwconnect.log", "libmwshare.log", "escanmon.log",
    "clients.plist", "fsevents_log.txt", "launchd.log", "softwarelist_data.txt",
    "softwares.list", "softwares.list.tmp", "mwsystem_profiler", "mwsystem_profiler.tmp",
    "auditd.pid", "window_9.data", "window_7.data", "live.2.indexHead", ".store.db",
    "pcs.db", "pcs.db-shm", "Info.plist", ".viminfo", "NSIRD_locationd_eInWAL"
};

// Function to check if a filename is in the exclusion list
bool isExcludedFile(const std::string &filename) {
    for (const auto &excluded : excluded_files) {
        if (filename == excluded) {
            return true; // Match found in exclusion list
        }
    }
    if (filename.find(".swp") != std::string::npos || filename.find(".swx") != std::string::npos) {
        return true;
    }
    return false;
}

// Function to get the current timestamp
void getCurrentTime(std::string &buffer) {
    time_t now = time(nullptr);
    struct tm *local_time = localtime(&now);
    char time_buffer[20];
    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", local_time);
    buffer = time_buffer;
}

// Function to open the log file, creating it if necessary
bool isLogFileAccessible() {
    struct stat buffer;
    return (stat(LOG_FILE_PATH, &buffer) == 0);  // Return true if file exists
}

// Function to open the log file, creating it if necessary
void openLogFile() {
    log_file.open(LOG_FILE_PATH, std::ios::app);
    if (!log_file.is_open()) {
        std::cerr << "Failed to open log file" << std::endl;
        exit(1);
    }
   //std::cout<<"file created successfully------------------Mahesh\n";
    // Write header to the log file if empty
    log_file.seekp(0, std::ios::end);
    if (log_file.tellp() == 0) {
        // Uncomment this line to add headers
        // log_file << "Timestamp, Action, Path, Filename\n";
    }
}

// Function to truncate the log file
void truncateLogFile() {
    if (log_file.is_open()) {
        log_file.close();
        log_file.open(LOG_FILE_PATH, std::ios::out | std::ios::trunc); // Reopen the file in write mode to truncate
        if (!log_file.is_open()) {
            std::cerr << "Failed to truncate log file" << std::endl;
            exit(1);
        }
        std::cout << "Log file truncated due to size limit." << std::endl;
    }
}

// Function to check the log file size and truncate if necessary
void checkLogFileSize() {
    struct stat st;
    if (stat(LOG_FILE_PATH, &st) == 0) {
        if (st.st_size > MAX_LOG_FILE_SIZE) {
            truncateLogFile();
        }
    }
}

// Function to log events (with optional destination)
void logEvent(const std::string &action, const std::string &path, const std::string &filename, const std::string &dest = "") {
    std::string time_buffer;
    getCurrentTime(time_buffer);

    // Check if the log file is accessible
    if (!log_file.is_open() || !isLogFileAccessible()) {
        std::cout<<"file is not presented calling openLogFile function-------------------Mahesh\n";
        openLogFile(); // Reopen the log file if it was deleted or closed unexpectedly
    }

    // Check log file size and truncate if necessary
    checkLogFileSize();

    // Write log entry
    if (log_file.is_open()) {
        if (dest.empty()) {
            log_file << time_buffer << ", " << action << ", " << path << ", " << filename << "\n";
        } else {
            log_file << time_buffer << ", " << action << ", source: " << dest << ", dest: " << path << ", filename: " << filename << "\n";
        }
        log_file.flush(); // Ensure the data is written to the file immediately
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
    for (size_t i = 0; i < numEvents; ++i) {
        std::string fileName = basename(paths[i]);

        // Skip logging if the file is in the exclusion list
        if (fileName.find(".swx") != std::string::npos) {
            open_flag = 1;
            open_filename = fileName;
            std::cout << "\nIn .swx: open_flag:" << open_flag << " and open_filename:" << open_filename << std::endl;
        }

        if (isExcludedFile(fileName)) {
            continue;
        }

        std::string action = "";
        if (eventFlags[i] & kFSEventStreamEventFlagItemCreated) {
            action = "Created";
            s_flag = 0;
        } else if (eventFlags[i] & kFSEventStreamEventFlagItemRemoved) {
            action = "Deleted";
            s_flag = 0;
        } else if (eventFlags[i] & kFSEventStreamEventFlagItemModified) {
            action = "Modified";
            s_flag = 0;
        } else if (eventFlags[i] & kFSEventStreamEventFlagItemRenamed) {
            // Handle renames with source-destination pairing
            if (s_flag == 0) {
                // Record the source path
                last_rename_source = paths[i];
                logEvent("Moved (source)", paths[i], fileName);
                s_flag = 1;
            } else if (s_flag == 1) {
                // Log the destination path only if it differs from the source
                logEvent("Moved (Destination)", paths[i], fileName);
                last_rename_source.clear(); // Clear the source
                s_flag = 0;
            }
            continue; // Skip further processing for rename events
        }

        if ((eventFlags[i] & kFSEventStreamEventFlagItemCreated) && (eventFlags[i] & kFSEventStreamEventFlagItemModified)) {
            std::cout << "\nopen_flag:" << open_flag << " and open_filename:" << open_filename << std::endl;
            if (open_flag == 1 && open_filename.find(fileName) != std::string::npos) {
                std::cout << "\nFile created and modified .swx file:" << open_filename << std::endl;
                logEvent("Created and Modified", paths[i], fileName);
                open_flag = 0;
                open_filename.clear();
                continue;
            } else {
                logEvent("File Copied", paths[i], fileName);
                continue;
            }
        }

        if ((eventFlags[i] & kFSEventStreamEventFlagItemCreated) || (eventFlags[i] & kFSEventStreamEventFlagItemModified)) {
            if ((eventFlags[i] & kFSEventStreamEventFlagItemCreated) && (eventFlags[i] & kFSEventStreamEventFlagItemModified)) {
                if (open_flag == 1 && open_filename.find(fileName) != std::string::npos) {
                    logEvent("Created and Modified", paths[i], fileName);
                    open_flag = 0;
                    open_filename.clear();
                    continue;
                } else {
                    logEvent("File Copied", paths[i], fileName);
                    continue;
                }
            } else if (eventFlags[i] & kFSEventStreamEventFlagItemCreated) {
               // logEvent("Empty File created or copied", paths[i], fileName);
         //       std::cout<<"source of cp  prev is : "<<source_cp_prev<<"  source cp current  is : "<<source_cp_current<<"  and dest paths is: "<<paths[i]<<std::endl;
                   std::string src="";
                 if(strstr(source_cp_prev.c_str(),fileName.c_str()))
                  { 
                     src=source_cp_prev;  
                  }
                  if(!src.empty())
                  {
                 //     std::cout<<"source of copy  is:"<<src<<"  and destination is: "<<paths[i]<<std::endl;   
                      logEvent("File COPYIED", paths[i], fileName,src);
          
                    
                  }
                  else{
               
                   logEvent("Empty File created or copied", paths[i], fileName); 
                   }
               count=0;
                continue;
            } else {
                if (open_flag == 1 && open_filename.find(fileName) != std::string::npos) {
                    logEvent("File Modified", paths[i], fileName);
                    std::cout << "\nFile modified and .swx file is " << open_filename << std::endl;
                    open_flag = 0;
                    open_filename.clear();
                    continue;
                } else {
                    logEvent("File Copied and overwritten", paths[i], fileName);
                    continue;
                }
            }
        }

        // Prevent duplicate logs
        if(action.empty())
          {
             source_cp_prev = source_cp_current;
            source_cp_current=paths[i];
            
          //  std::cout<<"source_cp : "<<source_cp_current<<"\n";
            count++;
          }
        if (!action.empty() && last_logged_path != paths[i]) {
            logEvent(action, paths[i], fileName);
            last_logged_path = paths[i];
        }
    }
}

void daemonize() {
    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "Fork failed" << std::endl;
        exit(1);
    }
    if (pid > 0) {
        exit(0); // Parent process exits
    }

    // Create a new session and detach from the terminal
    if (setsid() < 0) {
        std::cerr << "Failed to create a new session" << std::endl;
        exit(1);
    }

    // Redirect standard file descriptors to /dev/null
    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
}

int main(int argc, const char *argv[]) {
//    daemonize(); // Daemonize the process

    openLogFile(); // Open the log file

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

    CFRunLoopRun();

    FSEventStreamStop(stream);
    FSEventStreamInvalidate(stream);
    FSEventStreamRelease(stream);
    CFRelease(pathsToWatch);
    CFRelease(pathToWatch);

    log_file.close(); // Close the log file
    return 0;
}
