//
//  kern_start.cpp
//  VMHide
//
//  Created by RoyalGraphX on 10/12/24.
//

#include "kern_start.hpp"

// enum to represent VMHide states
enum VmhState {
    VMH_DISABLED,
    VMH_ENABLED,
    VMH_PASSTHROUGH
};

// define a struct to hold both process name and optional PID
struct FilteredProcess {
    const char *name;
    pid_t pid;
};

// static variable to store the original handler
static sysctl_handler_t originalHvVmmHandler = nullptr;

// static KernelPatcher instance
static KernelPatcher patcher;

// function to parse the sysctl__children memory address
mach_vm_address_t parseSysctlChildren() {
    // resolve the _sysctl__children symbol
    mach_vm_address_t sysctlChildrenAddress = patcher.solveSymbol(KernelPatcher::KernelID, "_sysctl__children");

    // check if the address was successfully resolved
    if (sysctlChildrenAddress) {
        DBGLOG("VMHide", "Resolved _sysctl__children at address: 0x%llx", sysctlChildrenAddress);

        // cast the address to sysctl_oid_list*
        sysctl_oid_list *sysctlChildren = reinterpret_cast<sysctl_oid_list *>(sysctlChildrenAddress);

        // log the address for debugging
        DBGLOG("VMHide", "Sysctl children list at address: 0x%llx", reinterpret_cast<mach_vm_address_t>(sysctlChildren));

        // iterate over the sysctl_oid_list
        sysctl_oid *oid;
        SLIST_FOREACH(oid, sysctlChildren, oid_link) {
            // log each OID's name and number
            DBGLOG("VMHide", "OID Name: %s, OID Number: %d", oid->oid_name, oid->oid_number);
        }

        return sysctlChildrenAddress;
    } else {
        SYSLOG("VMHide", "Failed to resolve _sysctl__children.");
        return 0;
    }
}

// function to reroute kern.hv_vmm_present function to our own custom one
bool reRouteHvVmm(mach_vm_address_t sysctlChildrenAddress) {
    
    // ensure that sysctlChildrenAddress exists before continuing
    if (!sysctlChildrenAddress) {
        SYSLOG("VMHide", "Failed to resolve _sysctl__children.");
        return false;
    }

    // case the address to sysctl_oid_list*
    sysctl_oid_list *sysctlChildren = reinterpret_cast<sysctl_oid_list *>(sysctlChildrenAddress);
    
    // traverse the sysctl tree to locate 'kern'
    sysctl_oid *kernNode = nullptr;
    SLIST_FOREACH(kernNode, sysctlChildren, oid_link) {
        if (strcmp(kernNode->oid_name, "kern") == 0) {
            DBGLOG("VMHide", "Found 'kern' node.");
            break;
        }
    }

    // check if kern node was found
    if (!kernNode) {
        SYSLOG("VMHide", "Failed to locate 'kern' node in sysctl tree.");
        return false;
    }

    // traverse 'kern' to find 'hv_vmm_present'
    sysctl_oid_list *kernChildren = reinterpret_cast<sysctl_oid_list *>(kernNode->oid_arg1);
    sysctl_oid *vmmNode = nullptr;
    SLIST_FOREACH(vmmNode, kernChildren, oid_link) {
        if (strcmp(vmmNode->oid_name, "hv_vmm_present") == 0) {
            DBGLOG("VMHide", "Found 'hv_vmm_present' node.");
            break;
        }
    }

    // check if the vmm present entry was found
    if (!vmmNode) {
        SYSLOG("VMHide", "Failed to locate 'hv_vmm_present' sysctl entry.");
        return false;
    }

    // save the original handler in the global variable
    originalHvVmmHandler = vmmNode->oid_handler;
    
    // reroute the handler to our custom function
    vmmNode->oid_handler = vmh_sysctl_vmm_present;

    DBGLOG("VMHide", "Successfully rerouted 'hv_vmm_present' sysctl handler.");
    return true;
}

// VMHide's custom sysctl VMM present function (with PID support)
int vmh_sysctl_vmm_present(struct sysctl_oid *oidp, void *arg1, int arg2, struct sysctl_req *req) {
    // retrieve the current process information
    proc_t currentProcess = current_proc();
    pid_t procPid = proc_pid(currentProcess);
    char procName[256];

    // get the process name using proc_name
    proc_name(procPid, procName, sizeof(procName));

    // log the process name and PID for debugging, and discovering new processes to filter
    DBGLOG("VMHide", "vmh_sysctl_vmm_present called by process: %s (PID: %d)", procName, procPid);

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
        DBGLOG("VMHide", "Process '%s' (PID: %d) matched filter. Hiding VMM present.", procName, procPid);
        
        int hv_vmm_present_off = 0;
        return SYSCTL_OUT(req, &hv_vmm_present_off, sizeof(hv_vmm_present_off));
    } else if (originalHvVmmHandler) {
        // if not filtered, call the original handler
        DBGLOG("VMHide", "Calling original 'hv_vmm_present' handler.");
        return originalHvVmmHandler(oidp, arg1, arg2, req);
    } else {
        // finally, if original handler doesn't exist
        DBGLOG("VMHide", "No original handler found. Returning default 0. This should not have happened!");
        return 0;
    }
}

// main or something
void vmhInit() {
    
    // init vmhState as a local variable
    char vmhState[64] = {0};
    VmhState vmhStateEnum = VMH_ENABLED; // Default to VMH_ENABLED
    
    // init patcher and say hello
    patcher.init();
    DBGLOG("VMHide", "Hello World from VMHide!");
    
    // let's make sure we parse a boot arg if one is present
    if (PE_parse_boot_argn("vmhState", vmhState, sizeof(vmhState))) {
        DBGLOG("VMHide", "vmhState argument found with value: %s", vmhState);
        
        if (strcmp(vmhState, "disabled") == 0) {
            vmhStateEnum = VMH_DISABLED;
            DBGLOG("VMHide", "Will not attempt to hide VM status! Halting VMHide.kext actions.");
        } else if (strcmp(vmhState, "enabled") == 0) {
            vmhStateEnum = VMH_ENABLED;
            DBGLOG("VMHide", "Will now attempt to hide VM status!");
        } else if (strcmp(vmhState, "passthrough") == 0) {
            vmhStateEnum = VMH_PASSTHROUGH;
            DBGLOG("VMHide", "Disabling VMHide.kext actions.");
        } else {
            vmhStateEnum = VMH_DISABLED;
            DBGLOG("VMHide", "Unknown vmhState value. Halting VMHide.kext actions.");
        }
    } else {
        // we didn't find one, let's attempt to hide the status then, why else would someone use this kext?
        DBGLOG("VMHide", "No explicit vmhState boot arg found.");
        DBGLOG("VMHide", "Will now attempt to hide VM status!");
    }

    // only proceed if the state is VMH_ENABLED
    if (vmhStateEnum != VMH_ENABLED) {
        DBGLOG("VMHide", "Will not continue due to the current VMHide state. Failed VMH Check [1/5]");
        return;
    }

    // check current status of kern.hv_vmm_present
    int hvVmmPresent = 0; // int to track the state for kern.hv_vmm_present
    size_t size = sizeof(hvVmmPresent);
    if (sysctlbyname("kern.hv_vmm_present", &hvVmmPresent, &size, nullptr, 0) == 0) {
        DBGLOG("VMHide", "VMM presence status (kern.hv_vmm_present): %d", hvVmmPresent);
    } else {
        SYSLOG("VMHide", "Failed to read kern.hv_vmm_present.");
        vmhStateEnum = VMH_DISABLED;
    }
    
    // continue only if hvVmmPresent is 1
    if (hvVmmPresent > 0) {
        DBGLOG("VMHide", "VM detected, proceeding to hide VM presence.");
    } else {
        vmhStateEnum = VMH_DISABLED;
        DBGLOG("VMHide", "No VM detected, nothing to hide.");
    }
    
    // only proceed if the state is VMH_ENABLED
    if (vmhStateEnum != VMH_ENABLED) {
        DBGLOG("VMHide", "Will not continue due to the current VMHide state. Failed VMH Check [2/5]");
        return;
    }

    // Call parseSysctlChildren
    mach_vm_address_t sysctlAddress = parseSysctlChildren();
    if (sysctlAddress == 0) {
        vmhStateEnum = VMH_DISABLED; // something went wrong, ggwp.
        DBGLOG("VMHide", "sysctlAddress is null, disabling VMHide.");
    } else {
        DBGLOG("VMHide", "Successfully parsed sysctl children.");
    }
    
    // only proceed if the state is VMH_ENABLED
    if (vmhStateEnum != VMH_ENABLED) {
        DBGLOG("VMHide", "Will not continue due to the current VMHide state. Failed VMH Check [3/5]");
        return;
    }
    
    // Call reRouteHvVmm with the parsed sysctlAddress
    if (!reRouteHvVmm(sysctlAddress)) {
        SYSLOG("VMHide", "Failed to reroute hv_vmm_present.");
        vmhStateEnum = VMH_DISABLED;
    } else {
        DBGLOG("VMHide", "VMH is now active and filtering processes.");
    }
    
    // only proceed if the state is VMH_ENABLED
    if (vmhStateEnum != VMH_ENABLED) {
        DBGLOG("VMHide", "Will not continue due to the current VMHide state. Failed VMH Check [4/5]");
        return;
    }
    
    // verify hvVmmPresent after rerouting; expected result is 0
    hvVmmPresent = 0; // reset int for the check
    DBGLOG("VMHide", "Will now test if VMM Status is being spoofed...");
    if (sysctlbyname("kern.hv_vmm_present", &hvVmmPresent, &size, nullptr, 0) == 0) {
        DBGLOG("VMHide", "Post-reroute VMM presence status (kern.hv_vmm_present): %d", hvVmmPresent);
        if (hvVmmPresent != 0) {
            DBGLOG("VMHide", "VMH check failed after reroute; hvVmmPresent value is higher than 0.");
            vmhStateEnum = VMH_DISABLED;
            return;
        }
        DBGLOG("VMHide", "Success! You are now appearing as baremetal.");
        DBGLOG("VMHide", "Thanks for using VMHide!");
    } else {
        SYSLOG("VMHide", "Failed to read kern.hv_vmm_present for post-reroute verification.");
        vmhStateEnum = VMH_DISABLED;
    }
    
    // only proceed if the state is VMH_ENABLED
    if (vmhStateEnum != VMH_ENABLED) {
        DBGLOG("VMHide", "Will not continue due to the current VMHide state. Failed VMH Check [5/5]");
        return;
    }

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
    KernelVersion::Ventura,
    KernelVersion::Sequoia,
    []() {
        vmhInit();
    }
};
