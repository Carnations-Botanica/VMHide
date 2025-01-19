//
//  log2disk.cpp
//  Implementation File for the Log2Disk ProjectExtension
//
//  Created by RoyalGraphX on 11/20/24.
//

#include "log2disk.hpp"

// Log2Disk Instance Variables
Log2Disk *Log2Disk::callbackL2D;
static thread_t k2u_thread;
static thread_t flt_thread;
static mach_vm_address_t orig_cs_validate_page {};
static Log2Disk::SpaceEnum spaceEnumState = Log2Disk::KERNELSPACE;
static Log2Disk::WindowServerInfo wsInfo = {Log2Disk::WindowServerState::NOT_FOUND, -1};
bool stopK2UThread = false;
bool stopFLTThread = false;
bool Log2Disk::globalL2dEnable = false;
char Log2Disk::globalL2dParentProject[16] = {0};
char Log2Disk::globalL2dLogFilePath[128] = {0};
char Log2Disk::globalL2dLogLevel[64] = {0};
char Log2Disk::dateTimeBuffer[128] = {0};

// Create blank Log Buffer
Log2Disk::RingBuffer Log2Disk::logBuffer;

// Function to retrieve and return the system uptime as a string
const char* logSystemUptime() {
    
    // Static string to hold uptime
    static char uptimeStr[64];
    uint64_t uptimeNano = mach_absolute_time();
    uint64_t uptimeSeconds = uptimeNano / 1000000000;
    uint64_t uptimeFractional = (uptimeNano % 1000000000) / 10000000; // Get two decimal places
    snprintf(uptimeStr, sizeof(uptimeStr), "%llu.%02llu", uptimeSeconds, uptimeFractional);

    // Return the string
    return uptimeStr;
    
}

// Helper function to calculate the current date and time
void Log2Disk::calcDateTime(char *outputBuffer, size_t bufferSize) {
    
    if (!outputBuffer || bufferSize == 0) return;

    clock_sec_t secs;
    clock_usec_t microsecs;

    // Get the current calendar time in seconds and microseconds
    clock_get_calendar_microtime(&secs, &microsecs);

    // Constants for date calculations
    const int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const int days_in_month_leap[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    
    // Adjust for local timezone offset in seconds
    // Example: Offset of -6 hours for UTC-6
    // This is very hacky and not how we should do this moving forward
    const int timezone_offset_seconds = -6 * 3600;
    secs += timezone_offset_seconds;

    // Convert seconds to date components
    int year = 1970;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = secs % 60;
    int days_since_epoch = secs / 60 / 60 / 24;

    // Leap year calculation
    int leap_year = 0;
    while (days_since_epoch >= (leap_year ? 366 : 365)) {
        int days_in_year = leap_year ? 366 : 365;
        days_since_epoch -= days_in_year;
        leap_year = (year % 4 == 0) && (year % 100 != 0 || year % 400 == 0);
        year++;
    }

    // Calculate month and day
    const int *month_days = leap_year ? days_in_month_leap : days_in_month;
    while (days_since_epoch >= month_days[month]) {
        days_since_epoch -= month_days[month];
        month++;
    }
    day = days_since_epoch + 1; // Day of the month starts at 1

    // Convert remaining seconds to hour and minute
    hour = (secs / 3600) % 24;
    minute = (secs / 60) % 60;

    // Format the date as YYYY-MM-DD HH:MM:SS
    snprintf(outputBuffer, bufferSize, "%04d-%02d-%02d %02d:%02d:%02d", year, month + 1, day, hour, minute, second);
    
}

// Helper function to generate a log file path so it can be a global
// log file path has to be macOS safe, meaning: YYYY/MM/DD-ParentProject-HH-MM-SS.txt
void Log2Disk::generateLogFilePath(char *outputBuffer, size_t bufferSize) {
    
    const char *basePath = "/System/Volumes/Data/Users/";
    char tempBuffer[PATH_MAX] = {}; // Temp holding for safe date/time
    
    if (!outputBuffer || bufferSize == 0) return;

    // Assuming Log2Disk::dateTimeBuffer and Log2Disk::globalL2dParentProject are already set
    if (!dateTimeBuffer || !globalL2dParentProject) {
        snprintf(outputBuffer, bufferSize, "InvalidInputs.txt");
        return;
    }

    // Extract date and time from dateTimeBuffer
    char date[11] = {};  // YYYY-MM-DD
    char time[9] = {};   // HH:MM:SS
    if (sscanf(dateTimeBuffer, "%10s %8s", date, time) != 2) {
        snprintf(outputBuffer, bufferSize, "InvalidDateTimeFormat.txt");
        return;
    }

    // Replace ':' in time with '-'
    for (char *c = time; *c != '\0'; ++c) {
        if (*c == ':') *c = '-';
    }

    // Construct the log file name in tempBuffer
    snprintf(tempBuffer, sizeof(tempBuffer), "%s-%s-%s.txt", date, globalL2dParentProject, time);

    // Prepend the base path and store it in outputBuffer
    snprintf(outputBuffer, bufferSize, "%s%s", basePath, tempBuffer);

}

// Function to handle adding Log Messages to the Circular Buffer
void Log2Disk::addLogMessage(const char *message) {
    
    // Copy the message into the buffer at the head index
    strlcpy(Log2Disk::logBuffer.messages[Log2Disk::logBuffer.head], message, MAX_MESSAGE_LENGTH);

    // Update head and wrap around if necessary
    Log2Disk::logBuffer.head = (Log2Disk::logBuffer.head + 1) % MAX_LOG_MESSAGES;

    // If the buffer is full, move the tail to overwrite the oldest message
    if (Log2Disk::logBuffer.count == MAX_LOG_MESSAGES) {
        Log2Disk::logBuffer.tail = (Log2Disk::logBuffer.tail + 1) % MAX_LOG_MESSAGES;
    } else {
        Log2Disk::logBuffer.count++;
    }
    
}

// Function to get all log messages based on the log level
void Log2Disk::retrieveLogMessages() {
    int currentIndex = Log2Disk::logBuffer.tail;
    for (int i = 0; i < Log2Disk::logBuffer.count; ++i) {
        const char *currentMessage = Log2Disk::logBuffer.messages[currentIndex];
        
        // Extract the log level from the message
        char extractedLevel[64] = {0};
        if (sscanf(currentMessage, "{%63[^}]}", extractedLevel) == 1) {
            // Check if the global log level matches the extracted level
            if (Log2Disk::globalL2dLogLevel[0] == '\0' ||
                strcasecmp(Log2Disk::globalL2dLogLevel, "all") == 0 ||
                strcasecmp(extractedLevel, Log2Disk::globalL2dLogLevel) == 0) {
                DBGLOG(MODULE_L2D, "[%d]: %s", i, currentMessage);
            }
        } else {
            // If unable to extract a level, log the message (fallback case)
            DBGLOG(MODULE_L2D, "[%d]: %s", i, currentMessage);
        }

        currentIndex = (currentIndex + 1) % MAX_LOG_MESSAGES;
    }
}

// Log messages to the disk using the session log file
void Log2Disk::logToDisk(const char *level, const char *tag, const char *format, ...) {
    
    char logMessage[MAX_MESSAGE_LENGTH];
    
    // Format the log message
    va_list args;
    va_start(args, format);
    vsnprintf(logMessage, sizeof(logMessage), format, args);
    va_end(args);
    
    // Uptime Calcs
    const char* uptimeStr = logSystemUptime();
    
    // Prepend the level and tag to the message
    char formattedMessage[MAX_MESSAGE_LENGTH];
    snprintf(formattedMessage, sizeof(formattedMessage), "<%s> {%s} @ [%s] %s", uptimeStr, level, tag, logMessage);
    
    // Add the message to the circular buffer
    addLogMessage(formattedMessage);
    
    // Write the message to disk or console
    // DBGLOG(MODULE_L2D, "Log added to buffer: %s", formattedMessage);
    
}

// L2D's custom _cs_page_validate function
static void l2d_cs_validate_page(vnode_t vp, memory_object_t pager, memory_object_offset_t page_offset, const void *data, int *validated_p, int *tainted_p, int *nx_p) {

    char path[PATH_MAX];
    int pathlen = PATH_MAX;
    
    // Ensure the original func exists to call
    if (orig_cs_validate_page) {
        // Call the original function using FunctionCast
        FunctionCast(l2d_cs_validate_page, orig_cs_validate_page)(vp, pager, page_offset, data, validated_p, tainted_p, nx_p);
        
        // Check if WindowServer is already found
        if (wsInfo.state == Log2Disk::WindowServerState::FOUND) {
            // DBGLOG(MODULE_CSVP, "WindowServer is FOUND, but l2d_cs_validate_page was called.");
            return;
        }
    } else {
        DBGLOG(MODULE_CSVP, "Original cs_validate_page is not valid, skipping call.");
        return;
    }
    
    // Retrieve the vnode's file path
    // DBGLOG(MODULE_CSVP, "Testing VNode Path validity.");
    if (vn_getpath(vp, path, &pathlen) != 0) {
        DBGLOG(MODULE_CSVP, "Failed to get path for vnode.");
        return;
    }

    // Check if the path matches the WindowServer executable
    if (strcmp(path, "/System/Library/PrivateFrameworks/SkyLight.framework/Versions/A/Resources/WindowServer") == 0) {
        
        pid_t pid = proc_pid(current_proc());
        DBGLOG(MODULE_CSVP, "Detected WindowServer with PID: %d", pid);
        
        // Update the WindowServerInfo structure
        wsInfo.state = Log2Disk::WindowServerState::FOUND;
        wsInfo.pid = pid;
        DBGLOG(MODULE_CSVP, "Updated WindowServerInfo: state=%d, pid=%d", wsInfo.state, wsInfo.pid);

    }
    
}

// Thread function to call kern2user in a loop
static void kern2userThread(void *param, int) {
    
    Log2Disk *self = static_cast<Log2Disk *>(param);
    
    // Loop Routine
    while (!stopK2UThread) {
        self->kern2user();
        IOSleep(500);
    }

    // Shutdown Routine
    if (k2u_thread == nullptr) {
        DBGLOG("K2ULT", "kern2user thread is nullptr, proceeding with shutdown routine.");
        thread_terminate(k2u_thread);
    } else {
        DBGLOG("K2ULT", "kern2user thread %p is valid pointer, cannot shutdown this loop.", k2u_thread);
    }
    
}

// Function to detect kernelspace -> userspace transition
void Log2Disk::kern2user() {

    // Check the state of WindowServerInfo. [Userspace Check 1/1]
    if (wsInfo.state == WindowServerState::FOUND) {
        DBGLOG(MODULE_K2U, "WindowServerInfo state is FOUND. WindowServer PID: %d", wsInfo.pid);
        
        // Update spaceEnumState to USERSPACE
        if (spaceEnumState != USERSPACE) {
            spaceEnumState = USERSPACE;
            if (spaceEnumState == USERSPACE) {
                if (stopK2UThread == false) {
                    
                    DBGLOG(MODULE_K2UT, "kern2user thread %p stopping as requested.", k2u_thread);
                    stopK2UThread = true; // Signal the thread to stop
                    DBGLOG(MODULE_K2UT, "Successfully Signaled kern2user thread to stop.");
                    k2u_thread = nullptr; // Mark the thread as terminated
                    DBGLOG(MODULE_K2UT, "kern2user thread has updated value of %p", k2u_thread);
                    
                } else {
                    DBGLOG(MODULE_ERROR, "Failed to signal kern2user thread to stop.");
                }
                
                DBGLOG(MODULE_K2U, "Successfully updated spaceEnumState to USERSPACE.");
                
                Log2Disk::L2DUserSpaceInit(); // Continue the L2D Init routine.

            } else {
                DBGLOG(MODULE_ERROR, "Failed to update spaceEnumState to USERSPACE.");
                return;
            }
        }
        
        return;
    } else if (wsInfo.state == WindowServerState::NOT_FOUND) {
        DBGLOG(MODULE_K2U, "WindowServerInfo state is NOT_FOUND.");
        return;
    } else {
        DBGLOG(MODULE_K2U, "WindowServerInfo state is UNKNOWN.");
        return;
    }
    
}

// VFS initialization and starting grounds for creating the session log file
static void createLogFile() {
    
    // Func variables
    vnode_t vnode = NULL;
    vnode_t file_vnode = NULL;
    vfs_context_t context = vfs_context_current();
    errno_t error;
    
    // Call vfs_rootvnode and log the result
    vnode_t root_vnode = vfs_rootvnode();
    if (root_vnode) {
        DBGLOG(MODULE_CLF, "Successfully retrieved global root vnode: %p", root_vnode);
        vnode_put(root_vnode); // Always release the vnode when done
    } else {
        DBGLOG(MODULE_CLF, "Failed to retrieve global root vnode");
        return;
    }
    
    // Locate /System/Volumes/Data
    mount_t data_mp = NULL;
    vfs_iterate(0, [](mount_t mp, void *arg) -> int {
        struct vfsstatfs *fs_stat = vfs_statfs(mp);
        if (fs_stat && strcmp(fs_stat->f_mntonname, "/System/Volumes/Data") == 0) {
            DBGLOG(MODULE_CLF, "Found mount for /System/Volumes/Data");
            *(mount_t *)arg = mp; // Pass back mount_t through arg
            return 0; // Continue iteration
        }
        return 0; // Continue iteration
    }, &data_mp);
    
    if (!data_mp) {
        DBGLOG(MODULE_CLF, "Could not locate /System/Volumes/Data");
        return;
    }

    // Check for /System/Volumes/Data/Users
    error = vnode_lookup("/System/Volumes/Data/Users", 0, &vnode, context);
    if (error == 0) {
        DBGLOG(MODULE_CLF, "Successfully found /System/Volumes/Data/Users, vnode: %p", vnode);
        
        // Attempt to open the file with the create flag and write access
        error = vnode_open(Log2Disk::globalL2dLogFilePath, O_CREAT | FWRITE, 0644, 0, &file_vnode, context);
        if (error == 0) {
            
            DBGLOG(MODULE_CLF, "Successfully opened test file for writing, vnode: %p", file_vnode);
            
            // Write data to the file
            const char *init_data = "";
            size_t data_len = strlen(init_data);
            int resid; // Remaining bytes that couldn't be written
            error = vn_rdwr(UIO_WRITE, file_vnode, (caddr_t)init_data, data_len, 0, UIO_SYSSPACE, IO_NODELOCKED | IO_SYNC, vfs_context_ucred(context), &resid, NULL);
            
            if (error == 0) {
                DBGLOG(MODULE_CLF, "Successfully wrote to file: %s", Log2Disk::globalL2dLogFilePath);
            } else {
                DBGLOG(MODULE_CLF, "Failed to write to file: %s, error: %d", Log2Disk::globalL2dLogFilePath, error);
            }
            
            vnode_put(file_vnode); // Release the vnode
        } else {
            DBGLOG(MODULE_CLF, "Failed to open test file, error: %d", error);
        }

        vnode_put(vnode); // Release the vnode for /System/Volumes/Data/Users
    } else {
        DBGLOG(MODULE_CLF, "Failed to find /System/Volumes/Data/Users, error: %d", error);
    }
    
}

// Updates the log file with the newest log buffer data
void Log2Disk::flushLogBufferToLogFile() {
    
    // Func variables
    vnode_t file_vnode = NULL;
    vfs_context_t context = vfs_context_current();
    errno_t error;

    DBGLOG(MODULE_FLBTLF, "Reopening the log file to compare and append new data");

    // Open the file for reading and writing
    error = vnode_open(Log2Disk::globalL2dLogFilePath, FREAD | FWRITE, 0, 0, &file_vnode, context);
    if (error == 0) {
        DBGLOG(MODULE_FLBTLF, "Successfully reopened log file, vnode: %p", file_vnode);

        // Step 1: Read existing file content
        char fileBuffer[4096] = {0}; // Buffer for reading file data
        size_t bytesRead = 0;
        error = vn_rdwr(UIO_READ, file_vnode, fileBuffer, sizeof(fileBuffer) - 1, 0, UIO_SYSSPACE, IO_NODELOCKED | IO_SYNC, vfs_context_ucred(context), (int *)&bytesRead, NULL);

        if (error == 0) {
            fileBuffer[bytesRead] = '\0'; // Null-terminate the file content
            DBGLOG(MODULE_FLBTLF, "Successfully read file content, bytesRead: %zu", bytesRead);

            // Step 2: Iterate through the log buffer and compare with file content
            int currentIndex = Log2Disk::logBuffer.tail;
            char missingLogs[4096] = {0}; // Buffer to accumulate missing logs
            size_t missingLogsLen = 0;

            for (int i = 0; i < Log2Disk::logBuffer.count; ++i) {
                const char *currentMessage = Log2Disk::logBuffer.messages[currentIndex];

                // Check if this log message is already present in the file content
                if (strstr(fileBuffer, currentMessage) == NULL) {
                    // If not present, add this log message to missingLogs
                    size_t msgLen = strlen(currentMessage);
                    if (missingLogsLen + msgLen + 1 < sizeof(missingLogs)) {
                        strcat(missingLogs, currentMessage);
                        strcat(missingLogs, "\n"); // Add newline for clarity
                        missingLogsLen += msgLen + 1;
                    } else {
                        DBGLOG(MODULE_FLBTLF, "Missing logs buffer is full, truncating");
                        break;
                    }
                }

                currentIndex = (currentIndex + 1) % MAX_LOG_MESSAGES;
            }

            // Step 3: Append missing logs to file
            if (missingLogsLen > 0) {
                int resid; // Remaining bytes that couldn't be written
                error = vn_rdwr(UIO_WRITE, file_vnode, (caddr_t)missingLogs, missingLogsLen, 0, UIO_SYSSPACE, IO_APPEND | IO_SYNC, vfs_context_ucred(context), &resid, NULL);

                if (error == 0) {
                    DBGLOG(MODULE_FLBTLF, "Successfully appended missing logs to the file");
                } else {
                    DBGLOG(MODULE_FLBTLF, "Failed to append missing logs to the file, error: %d", error);
                }
            } else {
                DBGLOG(MODULE_FLBTLF, "No new logs to append, file content is up to date");
            }

            // Synchronize file to ensure changes are written to disk
            error = VNOP_FSYNC(file_vnode, MNT_WAIT, context);
            if (error != 0) {
                DBGLOG(MODULE_FLBTLF, "Failed to synchronize file: %s, error: %d", Log2Disk::globalL2dLogFilePath, error);
            }
        } else {
            DBGLOG(MODULE_FLBTLF, "Failed to read file content, error: %d", error);
        }

        vnode_put(file_vnode); // Release the vnode
    } else {
        DBGLOG(MODULE_FLBTLF, "Failed to reopen log file, error: %d", error);
    }
}

// Thread function to call kern2user in a loop
static void flushLogsThread(void *param, int) {
    
    Log2Disk *self = static_cast<Log2Disk *>(param);
    
    // Loop Routine
    while (!stopFLTThread) {
        self->flushLogBufferToLogFile();
        IOSleep(120000); // 2 minutes
    }

    // Shutdown Routine
    if (flt_thread == nullptr) {
        DBGLOG("FLTT", "flushLogBufferToLogFile thread is nullptr, proceeding with shutdown routine.");
        thread_terminate(flt_thread);
    } else {
        DBGLOG("FLTT", "flushLogBufferToLogFile thread %p is valid pointer, cannot shutdown this loop.", flt_thread);
    }
    
}

// Initial Load routine
void Log2Disk::init(const char* l2dParentProject, bool l2dEnable, const char* l2dLogLevel) {
    
    // Set the global parent project name
    strlcpy(globalL2dParentProject, l2dParentProject, sizeof(globalL2dParentProject));
    if (globalL2dParentProject[0] == '\0') {  // Check if the string is empty
        DBGLOG(MODULE_L2D, "Parent Project data does not exist! Check L2D Init header.");
        return; // Exit early if the parent extension name is not given or is invalid
    }
    DBGLOG(MODULE_L2D, "Global parent project successfully set to %s", globalL2dParentProject);
    
    // Set the global enable flag
    globalL2dEnable = l2dEnable;
    if (!globalL2dEnable) {
        DBGLOG(MODULE_L2D, "Log2Disk is disabled via l2dEnable.");
        return; // Exit early if the extension is not enabled
    }
    DBGLOG(MODULE_L2D, "Global enable flag successfully set to true.");

    // Set the global log level
    if (l2dLogLevel && l2dLogLevel[0] != '\0') { // Check if l2dLogLevel is non-null and non-empty
        strlcpy(globalL2dLogLevel, l2dLogLevel, sizeof(globalL2dLogLevel));
        DBGLOG(MODULE_L2D, "Global log level successfully set to: %s", globalL2dLogLevel);
    } else {
        globalL2dLogLevel[0] = '\0'; // Ensure it's empty if not provided
        DBGLOG(MODULE_WARN, "Global log level not provided, defaulting to all.");
    }

    // Start the routine
    const char* l2dVersionNumber = L2D_VERSION;
    DBGLOG(MODULE_L2D, "Hello world from Log2Disk PE!");
    DBGLOG(MODULE_INFO, "Current Build Version running: %s for %s", l2dVersionNumber, globalL2dParentProject);
    DBGLOG(MODULE_INFO, "Copyright © 2024, 2025 Carnations Botanica. All rights reserved.");
    
    // Internal Header BEGIN
    // DBGLOG(MODULE_WARN, "This build of Log2Disk is for CarnationsInternal usage only!");
    // Log2Disk::logToDisk("WARN", MODULE_L2D, "This build of Log2Disk is for CarnationsInternal usage only!");
    // Internal Header END
    
    // L2D log buffer test routine
    Log2Disk::logToDisk("INFO", MODULE_L2D, "L2D Message Buffer begin!");
    // Log2Disk::logToDisk("DEBUG", MODULE_L2D, "This is a DEBUG level log test!");
    // Log2Disk::logToDisk("INFO", MODULE_L2D, "This is a INFO level log test!");
    // Log2Disk::logToDisk("CUSTOM", MODULE_L2D, "This is a CUSTOM level log test!");
    
    // Initialize spaceEnumState and check if it's correctly set
    spaceEnumState = KERNELSPACE;
    if (spaceEnumState == KERNELSPACE) {
        DBGLOG(MODULE_L2D, "Successfully initialized spaceEnumState to KERNELSPACE.");
        
        // Start the kern2user loop in a new kernel thread
        if (kernel_thread_start(kern2userThread, this, &k2u_thread) == KERN_SUCCESS) {
            DBGLOG(MODULE_L2D, "Background thread for kern2user started on thread %p successfully.", k2u_thread);
        } else {
            DBGLOG(MODULE_ERROR, "Failed to create background thread for kern2user.");
            return;
        }

        // Attempt route request for cs_validate_page
        DBGLOG(MODULE_L2D, "Calling onPatcherLoadForce to RouteRequest _cs_validate_page -> l2d_cs_validate_page");
        if (getKernelVersion() >= KernelVersion::BigSur) {
            lilu.onPatcherLoadForce([](void *user, KernelPatcher &patcher) {
                KernelPatcher::RouteRequest csRoute[] = {
                    KernelPatcher::RouteRequest("_cs_validate_page", l2d_cs_validate_page, orig_cs_validate_page)
                };
                if (!patcher.routeMultipleLong(KernelPatcher::KernelID, csRoute, 1))
                    DBGLOG(MODULE_ERROR, "Failed to route _cs_validate_page -> l2d_cs_validate_page");
            });
        } else {
            DBGLOG(MODULE_WARN, "Kernel version is lower than Big Sur. Routing _cs_validate_page is not supported.");
            return; // will get filled out some other time, need to investigate later
        }

    } else {
        DBGLOG(MODULE_ERROR, "Failed to initialize spaceEnumState to KERNELSPACE.");
        return;
    }
    
}

// Continuation function that runs once we're in USERSPACE
void Log2Disk::L2DUserSpaceInit() {
    
    // DBGLOG a few addresses in memory to ensure proper init/deinit
    DBGLOG(MODULE_L2DUSI, "Address of orig_cs_validate_page: %p", &orig_cs_validate_page);
    DBGLOG(MODULE_L2DUSI, "Address of l2d_cs_validate_page: %p", reinterpret_cast<void *>(&l2d_cs_validate_page));
    
    // Allocate a buffer for the date and time string, then call it
    DBGLOG(MODULE_L2DUSI, "Fetching current date and time.");
    calcDateTime(Log2Disk::dateTimeBuffer, sizeof(Log2Disk::dateTimeBuffer));
    DBGLOG(MODULE_L2DUSI, "Current date and time: %s", Log2Disk::dateTimeBuffer);
    Log2Disk::logToDisk("INFO", MODULE_L2DUSI, "Current date and time: %s", Log2Disk::dateTimeBuffer);
    
    // Generate the log file path and store it in the buffer
    DBGLOG(MODULE_L2DUSI, "Generating log file path.");
    Log2Disk::generateLogFilePath(Log2Disk::globalL2dLogFilePath, sizeof(Log2Disk::globalL2dLogFilePath));
    DBGLOG(MODULE_L2DUSI, "Generated log file path: %s", Log2Disk::globalL2dLogFilePath);
    
    // Init the log file
    createLogFile();
    
    // Create a loop thread that runs every 2 minutes to sync log buffer to log file
    // Log2Disk::flushLogBufferToLogFile(); // Initial sync
    // Start the flushLogsThread loop in a new kernel thread
    if (kernel_thread_start(flushLogsThread, this, &flt_thread) == KERN_SUCCESS) {
        DBGLOG(MODULE_L2D, "Background thread for flushLogBufferToLogFile started on thread %p successfully.", flt_thread);
    } else {
        DBGLOG(MODULE_ERROR, "Failed to create background thread for flushLogBufferToLogFile.");
        return;
    }
    
    DBGLOG(MODULE_CUTE, "Thanks for using Log2Disk!");
    
}

// Usual Lilu deinit routine
void Log2Disk::deinit() {
    
    DBGLOG(MODULE_L2D, "This PE cannot be disabled like this!");
    DBGLOG(MODULE_L2D, "If you do not want L2D to load, remove the boot-arg from NVRAM.");
    
}
