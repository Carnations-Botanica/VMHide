//
//  kern_start.cpp
//  VMHide
//
//  Created by RoyalGraphX on 10/12/24.
//

#include "kern_start.hpp"

static VMH vmhInstance;

VMH *VMH::callbackVMH;

// static integer to keep track of initial and post reroute presence.
static int hvVmmPresent = 0;
size_t size = sizeof(hvVmmPresent);

// default to enabled, why else would someone use this?
VMH::VmhState vmhStateEnum = VMH::VMH_ENABLED;

// static variable to store the original handler
static sysctl_handler_t originalHvVmmHandler = nullptr;

// Struct to hold both process name and potential PID
struct FilteredProcess {
    const char *name;
    pid_t pid;
};

// VMHide's custom sysctl VMM present function (with PID support)
int vmh_sysctl_vmm_present(struct sysctl_oid *oidp, void *arg1, int arg2, struct sysctl_req *req) {
    
    // retrieve the current process information
    proc_t currentProcess = current_proc();
    pid_t procPid = proc_pid(currentProcess);
    char procName[256];

    // get the process name using proc_name
    proc_name(procPid, procName, sizeof(procName));

    // log the process name and PID for debugging, and discovering new processes to filter
    DBGLOG(MODULE_CSYS, "vmh_sysctl_vmm_present called by process: %s (PID: %d)", procName, procPid);

    // array of processes to filter by name or PID
    const FilteredProcess filteredProcs[] = {
        {"SoftwareUpdateNo", -1}, // -1 indicates no specific PID
        {"fairplaydeviceid", -1},
        {"networkservicepr", -1},
        {"identityservices", -1},
        {"localspeechrecog", -1},
        {"softwareupdated", -1},
        {"AppleIDSettings", -1},
        {"mediaanalysisd", -1},
        {"transparencyd", -1},
        {"ControlCenter", -1},
        {"com.apple.sbd", -1},
        {"translationd", -1},
        {"itunescloudd", -1},
        {"amsaccountsd", -1},
        {"duetexpertd", -1},
        {"groupkitd", -1},
        {"accountsd", -1},
        {"Terminal", -1},
        {"ndoagent", -1},
        {"remindd", -1},
        {"triald", -1},
        {"sysctl", -1},
        {"apsd", -1},
        {"cdpd", -1},
        {"akd", -1},
        {" ", 0}  // PID 0, or basically, the Kernel Task itself
    };

    // check if the current process matches any of the filtered names or PIDs
    bool isFiltered = false;
    for (const auto &filteredProc : filteredProcs) {
        if ((filteredProc.pid == -1 && strcmp(procName, filteredProc.name) == 0) ||
            (filteredProc.pid != -1 && procPid == filteredProc.pid)) {
            isFiltered = true;
            break;
        }
    }

    if (isFiltered) {
        // if process is in the filtered list, hide the VMM status
        DBGLOG(MODULE_CSYS, "Process '%s' (PID: %d) matched filter. Hiding VMM present.", procName, procPid);
        
        int hv_vmm_present_off = 0;
        return SYSCTL_OUT(req, &hv_vmm_present_off, sizeof(hv_vmm_present_off));
    } else if (originalHvVmmHandler) {
        // if not filtered, call the original handler
        DBGLOG(MODULE_CSYS, "Calling original 'hv_vmm_present' handler.");
        return originalHvVmmHandler(oidp, arg1, arg2, req);
    } else {
        // finally, if original handler doesn't exist
        DBGLOG(MODULE_CSYS, "No original handler found. Returning default 0. This should not have happened!");
        return 0;
    }
    
}

// function to reroute kern.hv_vmm_present function to our own custom one
bool reRouteHvVmm(KernelPatcher &patcher, mach_vm_address_t sysCtlChildrenAddress) {
    
    // ensure that sysctlChildrenAddress exists before continuing
    if (!sysCtlChildrenAddress) {
        SYSLOG(MODULE_RRHV, "Failed to resolve _sysctl__children passed in to function.");
        return false;
    }
    
    // Case the address to sysctl_oid_list*
    sysctl_oid_list *sysctlChildren = reinterpret_cast<sysctl_oid_list *>(sysCtlChildrenAddress);
    
    // traverse the sysctl tree to locate 'kern'
    sysctl_oid *kernNode = nullptr;
    SLIST_FOREACH(kernNode, sysctlChildren, oid_link) {
        if (strcmp(kernNode->oid_name, "kern") == 0) {
            DBGLOG(MODULE_RRHV, "Found 'kern' node.");
            break;
        }
    }
    
    // check if kern node was found
    if (!kernNode) {
        SYSLOG(MODULE_RRHV, "Failed to locate 'kern' node in sysctl tree.");
        return false;
    }
    
    // traverse 'kern' to find 'hv_vmm_present'
    sysctl_oid_list *kernChildren = reinterpret_cast<sysctl_oid_list *>(kernNode->oid_arg1);
    sysctl_oid *vmmNode = nullptr;
    SLIST_FOREACH(vmmNode, kernChildren, oid_link) {
        if (strcmp(vmmNode->oid_name, "hv_vmm_present") == 0) {
            DBGLOG(MODULE_RRHV, "Found 'hv_vmm_present' node.");
            break;
        }
    }

    // check if the vmm present entry was found
    if (!vmmNode) {
        SYSLOG(MODULE_RRHV, "Failed to locate 'hv_vmm_present' sysctl entry.");
        return false;
    }

    // save the original handler in the global variable
    originalHvVmmHandler = vmmNode->oid_handler;
    DBGLOG(MODULE_RRHV, "Successfully saved original 'hv_vmm_present' sysctl handler.");
    
    // ensure kernel r/w access
    PANIC_COND(MachInfo::setKernelWriting(true, patcher.kernelWriteLock) != KERN_SUCCESS, MODULE_SHORT, "Failed to enable God mode. (Kernel R/W)");
    
    // reroute the handler to our custom function
    vmmNode->oid_handler = vmh_sysctl_vmm_present;
    MachInfo::setKernelWriting(false, patcher.kernelWriteLock);
    DBGLOG(MODULE_RRHV, "Successfully rerouted 'hv_vmm_present' sysctl handler.");
    return true;
    
}

// gets sysctl__children memory address and returns it
mach_vm_address_t sysctlChildrenAddr(KernelPatcher &patcher) {
    
    // resolve the _sysctl__children symbol with the given patcher
    mach_vm_address_t sysctlChildrenAddress = patcher.solveSymbol(KernelPatcher::KernelID, "_sysctl__children");
    
    // check if the address was successfully resolved, else return 0
    if (sysctlChildrenAddress) {
        DBGLOG(MODULE_SYSCTL, "Resolved _sysctl__children at address: 0x%llx", sysctlChildrenAddress);

        // cast the address to sysctl_oid_list*
        sysctl_oid_list *sysctlChildren = reinterpret_cast<sysctl_oid_list *>(sysctlChildrenAddress);

        // log the address for debugging
        DBGLOG(MODULE_SYSCTL, "Sysctl children list at address: 0x%llx", reinterpret_cast<mach_vm_address_t>(sysctlChildren));

        // iterate over the sysctl_oid_list
        sysctl_oid *oid;
        SLIST_FOREACH(oid, sysctlChildren, oid_link) {
            // log each OID's name and number
            DBGLOG(MODULE_SYSCTL, "OID Name: %s, OID Number: %d", oid->oid_name, oid->oid_number);
        }

        return sysctlChildrenAddress;
    } else {
        KernelPatcher::Error err = patcher.getError();
        SYSLOG(MODULE_SYSCTL, "Failed to resolve _sysctl__children. (Lilu returned: %d)", err);
        patcher.clearError();
        return 0;
    }
    
}

// Function to solve the _sysctl__children symbol address
static void solveSysCtlChildrenAddr(void *user __unused, KernelPatcher &Patcher) {
    
    // Log area
    DBGLOG(MODULE_SYSCTL, "solveSysCtlChildrenAddr called after KernelPatcher loaded successfully.");
    
    // Get the address of _sysctl__children here
    mach_vm_address_t sysCtlChildrenAddress = sysctlChildrenAddr(Patcher);
    
    // Chain into reRouteHvVmm with the address, and the Patcher instance
    if (!reRouteHvVmm(Patcher, sysCtlChildrenAddress)) {
        SYSLOG(MODULE_SYSCTL, "Failed to reroute hv_vmm_present.");
    } else {
        DBGLOG(MODULE_SYSCTL, "VMH is now active and filtering processes.");
    }
    
    // verify hvVmmPresent after rerouting; expected result is 0
    static int hvVmmPresent = 0; // reset int for the check
    DBGLOG(MODULE_SYSCTL, "Will now test if VMM Status is being spoofed to sysctl.");
    if (sysctlbyname("kern.hv_vmm_present", &hvVmmPresent, &size, nullptr, 0) == 0) {
        DBGLOG(MODULE_INFO, "Post-reroute VMM presence status (kern.hv_vmm_present): %d", hvVmmPresent);
        
        if (hvVmmPresent != 0) {
            DBGLOG(MODULE_ERROR, "Failed after reroute; hvVmmPresent value is higher than 0.");
            return;
        } else {
            DBGLOG(MODULE_SYSCTL, "Success! You are now appearing as baremetal.");
            DBGLOG(MODULE_CUTE, "Thanks for using VMHide!");
        }
    } else {
        SYSLOG(MODULE_ERROR, "Failed to read kern.hv_vmm_present for post-reroute verification.");
        vmhStateEnum = VMH::VMH_DISABLED;
    }
    
}

// Main VMH Routine function
void VMH::init() {
    
    // Start off the routine
    callbackVMH = this;
    char vmhState[64] = {0};
    DBGLOG(MODULE_INIT, "Hello World from VMHide!");
    DBGLOG(MODULE_INFO, "This build of VMHide is for carnationsinternal usage!");
    
    // CHECK 1/2
    // let's make sure we parse a boot arg if one is present
    if (PE_parse_boot_argn("vmhState", vmhState, sizeof(vmhState))) {
        DBGLOG("VMHide", "vmhState argument found with value: %s", vmhState);
        
        if (strcmp(vmhState, "disabled") == 0) {
            vmhStateEnum = VMH::VMH_DISABLED;
            DBGLOG(MODULE_WARN, "vmhState is disabled. Halting futher VMHide.kext execution.");
        } else if (strcmp(vmhState, "enabled") == 0) {
            vmhStateEnum = VMH::VMH_ENABLED;
            DBGLOG(MODULE_INIT, "vmhState is enabled, will reroute regardless of VMM presence.");
        } else if (strcmp(vmhState, "strict") == 0) {
            vmhStateEnum = VMH::VMH_STRICT;
            DBGLOG(MODULE_WARN, "vmhState is strict. Hiding VMM status from all processes.");
        } else {
            vmhStateEnum = VMH::VMH_DISABLED;
            DBGLOG(MODULE_ERROR, "Unknown vmhState value found. Halting futher VMHide.kext execution.");
        }
    } else {
        // we didn't find one, let's attempt to hide the status then, why else would someone use this kext?
        DBGLOG(MODULE_INFO, "No explicit vmhState boot-arg found.");
        DBGLOG(MODULE_INFO, "Will now attempt to hide VM status if Hypervisor is found!");
    }

    // Init vmhState check
    if (vmhStateEnum != VMH::VMH_ENABLED) {
        DBGLOG(MODULE_ERROR, "Cannot continue. Failed Init Check [1/2]");
        return;
    }
    
    // CHECK 2/2
    // check current status of kern.hv_vmm_present to ensure usage on a hypervisor and not baremetal
    if (sysctlbyname("kern.hv_vmm_present", &hvVmmPresent, &size, nullptr, 0) == 0) {
        DBGLOG(MODULE_INFO, "Current VMM presence status (kern.hv_vmm_present): %d", hvVmmPresent);
    } else {
        SYSLOG(MODULE_ERROR, "Failed to read kern.hv_vmm_present.");
        vmhStateEnum = VMH::VMH_DISABLED;
    }
    
    // Continue only if hvVmmPresent is 1, ensuring a VM is in use
    if (hvVmmPresent > 0) {
        DBGLOG(MODULE_INIT, "VM or Hypervisor usage detected, proceeding to hide VM presence.");
    } else {
        vmhStateEnum = VMH::VMH_DISABLED;
        DBGLOG(MODULE_WARN, "No VM or Hypervisor usage detected, nothing to hide.");
    }
    
    // Init vmhState check
    if (vmhStateEnum != VMH::VMH_ENABLED) {
        DBGLOG(MODULE_ERROR, "Cannot continue. Failed Init Check [2/2]");
        return;
    }
    
    // Register a request to solve _sysctl__children on KernelPatcher load, and reroute to our custom function
    DBGLOG(MODULE_INIT, "Calling onPatcherLoadForce to resolve SysCtlChildrenAddr");
    lilu.onPatcherLoadForce(&solveSysCtlChildrenAddr);
    
}

// We use vmhState to determine VMH behaviour
void VMH::deinit() {
    
    DBGLOG(MODULE_ERROR, "This kernel extension cannot be disabled this way!");
    SYSLOG(MODULE_ERROR, "This kernel extension cannot be disabled this way!");
    
}


const char *bootargOff[] {
    "-vmhoff"
};

const char *bootargDebug[] {
    "-vmhdbg"
};

const char *bootargBeta[] {
    "-vmhbeta"
};

PluginConfiguration ADDPR(config) {
    xStringify(PRODUCT_NAME),
    parseModuleVersion(xStringify(MODULE_VERSION)),
    LiluAPI::AllowNormal |
    LiluAPI::AllowSafeMode |
    LiluAPI::AllowInstallerRecovery,
    bootargOff,
    arrsize(bootargOff),
    bootargDebug,
    arrsize(bootargDebug),
    bootargBeta,
    arrsize(bootargBeta),
    KernelVersion::Sonoma,
    KernelVersion::Sequoia,
    []() {
        
        // Start the main VMH routine
        vmhInstance.init();
        
    }
};
