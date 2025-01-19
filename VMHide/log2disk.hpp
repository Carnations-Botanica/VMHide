//
//  log2disk.hpp
//  Header File for the Log2Disk ProjectExtension
//
//  Created by RoyalGraphX on 11/20/24.
//

#ifndef LOG2DISK_HPP
#define LOG2DISK_HPP

#include <string.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <kern/task.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <kern/locks.h>
#include <kern/queue.h>
#include <sys/sysctl.h>
#include <kern/clock.h>
#include <kern/debug.h>
#include <mach/clock.h>
#include <kern/thread.h>
#include <IOKit/IOLib.h>
#include <i386/_types.h>
#include <sys/vnode_if.h>
#include <mach/mach_time.h>
#include <mach/mach_host.h>
#include <libkern/libkern.h>
#include <mach/mach_types.h>
#include <libkern/OSAtomic.h>
#include <sys/kernel_types.h>
#include <Headers/kern_api.hpp>
#include <mach/i386/vm_types.h>
#include <Headers/kern_util.hpp>
#include <Headers/kern_mach.hpp>
#include <Headers/kern_util.hpp>
#include <Headers/kern_user.hpp>
#include <sys/_types/_timespec.h>
#include <Headers/kern_patcher.hpp>
#include <Headers/plugin_start.hpp>

// Logging defs
#define MODULE_L2D "L2D"
#define MODULE_CDT "CDT"
#define MODULE_K2U "K2U"
#define MODULE_K2UT "K2UT"
#define MODULE_CSVP "CSVP"
#define MODULE_CLF "CLF"
#define MODULE_FLBTLF "FLBTLF"
#define MODULE_L2DUSI "L2DUSI"
#define MODULE_GLFP "GLFP"
#define MODULE_AVFS "AVFS"
#define MODULE_ERROR "ERR"
#define MODULE_WARN "WARN"
#define MODULE_INFO "INFO"
#define MODULE_CUTE "\u2665"
#define MODULE_LONG_L2D "Log2Disk"

// Logging configuration
#define MAX_LOG_MESSAGES 100 // Maximum number of log messages the buffer can hold
#define MAX_MESSAGE_LENGTH 512 // Maximum length of each log message

class Log2Disk {
public:
    
    // Static member variables to L2D session data
    static bool globalL2dEnable;
    static char globalL2dParentProject[16];
    static char globalL2dLogLevel[64];
    static char dateTimeBuffer[128];
    static char globalL2dLogFilePath[128];

    // Overload operator() for convenience, this fixes the l2d macro
    void operator()(const char* level, const char* module, const char* message, ...) {
        va_list args;
        va_start(args, message);
        logToDisk(level, module, message, args);
        va_end(args);
    }
    
    // Ring buffer for storing log messages
    struct RingBuffer {
        char messages[MAX_LOG_MESSAGES][MAX_MESSAGE_LENGTH]; // Array of messages
        int head = 0;  // Points to the next message to overwrite
        int tail = 0;  // Points to the oldest message
        int count = 0; // Number of messages currently in the buffer
    };

    static RingBuffer logBuffer; // Instance of the RingBuffer to use

    // Enum to represent execution space states
    enum SpaceEnum {
        KERNELSPACE,
        USERSPACE
    };
    
    // Enum for WindowServer state
    enum WindowServerState {
        NOT_FOUND,
        FOUND
    };

    // Struct to represent WindowServer info
    struct WindowServerInfo {
        WindowServerState state = NOT_FOUND;
        pid_t pid = -1; // PID of the WindowServer process, default -1 (not found)
    };
    
    // Functions to handle adding to buffer
    void addLogMessage(const char *message);

    // Functions to handle getting the buffer data
    void retrieveLogMessages();
    
    // Helper function to calculate the current date and time in the format: 1970-01-00-000000
    void calcDateTime(char *outputBuffer, size_t bufferSize);
    
    // Function to generate a logfile path for this session
    void generateLogFilePath(char *outputBuffer, size_t bufferSize);

    // Ensure the logfile exists (create if necessary)
    void ensureLogExists(const char *path);

    // Log messages to the disk using the session log file
    void logToDisk(const char *level, const char *tag, const char *format, ...);
    
    // Routine that checks for kernelspace -> userspace transition
    void kern2user();
    
    // Updates the log file with the newest log buffer data
    void flushLogBufferToLogFile();

    // Initial Load routine
    void init(const char* l2dParentProject, bool l2dEnable, const char* l2dLogLevel);

    // Cont. func once L2D hits USERSPACE
    void L2DUserSpaceInit();
    
    // Usual Lilu destructor
    void deinit();
    
private:
    
    // Self instance for callbacks
    static Log2Disk *callbackL2D;
    
};

#define L2D(level, tag, format, ...) L2D.logToDisk(level, tag, format, ##__VA_ARGS__)

#endif // LOG2DISK_HPP

#ifndef L2D_VERSION /* L2D_VERSION Macro */
#define L2D_VERSION "Unknown"
#endif /* L2D_VERSION Macro */
