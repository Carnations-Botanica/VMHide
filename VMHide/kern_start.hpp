//
//  kern_start.hpp
//  VMHide
//
//  Created by RoyalGraphX on 10/15/24.
//

#ifndef kern_start_h
#define kern_start_h

#include <Headers/plugin_start.hpp>
#include <Headers/kern_patcher.hpp>
#include <Headers/kern_api.hpp>
#include <Headers/kern_util.hpp>
#include <Headers/kern_mach.hpp>
#include <mach/i386/vm_types.h>
#include <libkern/libkern.h>
#include <sys/sysctl.h>

/**
* Macros for sysctl types, taken from RestrictEvents
* https://github.com/acidanthera/RestrictEvents/blob/master/RestrictEvents/SoftwareUpdate.hpp
*/
#define CTLTYPE             0xf     // Mask for the type
#define CTLTYPE_NODE        1       // name is a node
#define CTLTYPE_INT         2       // name describes an integer
#define CTLTYPE_STRING      3       // name describes a string
#define CTLTYPE_QUAD        4       // name describes a 64-bit number
#define CTLTYPE_OPAQUE      5       // name describes a structure
#define CTLTYPE_STRUCT      CTLTYPE_OPAQUE  // name describes a structure

#define SYSCTL_OUT(r, p, l) (r->oldfunc)(r, p, l)

// Logging Defs
#define MODULE_SYSCTL "SYSC"
#define MODULE_PPU "PPU"
#define MODULE_RRHV "RRHV"
#define MODULE_CSYS "CSYS"
#define MODULE_INIT "MAIN"
#define MODULE_SHORT "VMH"
#define MODULE_LONG "VMHide"
#define MODULE_CUTE "\u2665"

// VMH Class
class VMH {
public:
    
    /**
     * Maximum number of processes we can track, Maximum length of a process name
     */
    #define MAX_PROCESSES 256
    #define MAX_PROC_NAME_LEN 256
    
    /**
     * Standard Init and deInit functions
     */
    void init();
    void deinit();

    /**
     * Enum to represent VMHide states
     */
    enum VmhState {
        VMH_INVERTED,
        VMH_UNDERCOVER,
        VMH_INTERNAL,
        VMH_DISABLED,
        VMH_ENABLED,
        VMH_DEFAULT,
        VMH_STRICT,
    };
    
private:
    
    /**
     *  Private self instance for callbacks
     */
    static VMH *callbackVMH;
    
};

#endif /* kern_start_hpp */

#ifndef VMH_VERSION /* VMH_VERSION Macro */
#define VMH_VERSION "Unknown"

#endif /* VMH_VERSION Macro */
