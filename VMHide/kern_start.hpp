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
#define MODULE_RRHV "RRHV"
#define MODULE_CSYS "CSYS"
#define MODULE_INIT "MAIN"
#define MODULE_SHORT "VMH"
#define MODULE_CUTE "<3"
#define MODULE_L2D "L2D"
#define MODULE_ERROR "ERR"
#define MODULE_WARN "WARN"
#define MODULE_INFO "INFO"

// VMH Class
class VMH {
public:
    
    /**
     * Standard Init and deInit functions
     */
    void init();
    void deinit();

    /**
     * Enum to represent VMHide states
     */
    enum VmhState {
        VMH_UNDERCOVER,
        VMH_INTERNAL,
        VMH_DISABLED,
        VMH_ENABLED,
        VMH_STRICT,
    };
    
    /**
     * Static integer for VMM Presence
    */
    static int hvVmmPresent;
    
    /**
     * Declare size type before using it
    */
    size_t size = sizeof(hvVmmPresent);
    
private:
    
    /**
     *  Private self instance for callbacks
     */
    static VMH *callbackVMH;
    
};

#endif /* kern_start_hpp */
