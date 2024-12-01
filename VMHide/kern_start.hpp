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
#define MODULE_SHORT "VMH"
#define MODULE_ERROR "ERR"
#define MODULE_INIT "INIT"
#define MODULE_L2D "L2D"

// Reworked VMH Class from the earlier prototypes
class VMH {
public:
    /**
     * Standard Init and deInit functions
     */
    void init();
    void deinit();
    
    /**
     *  Function to parse the sysctl children memory address
     */
    static void solveSysCtlChildrenAddr(void *user __unused, KernelPatcher &Patcher);
    
    /**
     *  Function to reroute kern hv vmm present function to our own custom one in VMH
     */
    bool reRouteHvVmm(mach_vm_address_t sysctlChildrenAddress);
    
    /**
     *  VMHide's custom sysctl VMM present function (with PID support)
     */
    int vmh_sysctl_vmm_present(struct sysctl_oid *oidp, void *arg1, int arg2, struct sysctl_req *req);
    
    /**
     * Enum to represent VMHide states
     */
    enum VmhState {
        VMH_DISABLED,
        VMH_ENABLED,
        VMH_PASSTHROUGH
    };
    
private:
    /**
     *  Private self instance for callbacks
     */
    static VMH *callbackVMH;
    
    /**
     *  Enable KernelPatcher for resolving symbols
     *
     *  @param patcher KernelPatcher instance
     */
    void processKernel(KernelPatcher &patcher);
    
    /**
     *  Static variable to store the original kern hv vmm present handler
     */
    static sysctl_handler_t originalHvVmmHandler;
    
    /**
     *  Struct to hold both process name and potential PID
     */
    struct FilteredProcess {
        const char *name;
        pid_t pid;
    };
};

#endif /* kern_start_hpp */
