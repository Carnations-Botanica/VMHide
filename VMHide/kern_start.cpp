//
//  kern_start.cpp
//  VMHide
//
//  Created by RoyalGraphX on 10/12/24.
//

#include "kern_start.hpp"
#include "log2disk.hpp"

static VMH vmhInstance;
static Log2Disk L2D;

VMH *VMH::callbackVMH;

// static integer to keep track of initial and post reroute presence.
static int hvVmmPresent = 0;
size_t size = sizeof(hvVmmPresent);

// Static array to store unique process names
static char uniqueProcesses[MAX_PROCESSES][MAX_PROC_NAME_LEN];
static int uniqueProcessCount = 0;  // Tracks the current number of stored process names

// default to enabled, why else would someone use this?
VMH::VmhState vmhStateEnum = VMH::VMH_DEFAULT;

// static variable to store the original handler
static sysctl_handler_t originalHvVmmHandler = nullptr;

// Struct to hold both process name and potential PID
struct FilteredProcess {
    const char *name;
    pid_t pid;
};

// array of processes to filter by name or PID
static const FilteredProcess filteredProcs[] = {
    {"SoftwareUpdateNo", -1}, // -1 indicates no specific PID
    {"fairplaydeviceid", -1},
    {"networkservicepr", -1},
    {"identityservices", -1},
    {"localspeechrecog", -1},
    {"softwareupdated", -1},
    {"AppleIDSettings", -1},
    {"mediaanalysisd", -1},
    {"avconferenced", -1},
    {"modelcatalogd", -1},
    {"transparencyd", -1},
    {"ControlCenter", -1},
    {"com.apple.sbd", -1},
    {"translationd", -1},
    {"itunescloudd", -1},
    {"amsaccountsd", -1},
    {"mobileassetd", -1},
    {"dataaccessd", -1},
    {"duetexpertd", -1},
    {"bluetoothd", -1}, // right now, this is causing issues on my test VM, causing kernel panics.
    {"locationd", -1},
    {"groupkitd", -1},
    {"accountsd", -1},
    {"aonsensed", -1},
    {"sharingd", -1},
    {"rapportd", -1},
    {"Terminal", -1},
    {"ndoagent", -1},
    {"remindd", -1},
    {"remoted", -1},
    {"triald", -1},
    {"sysctl", -1},
    {"launchd", 1}, // PID 1, usually is launchd, but if not, we can't hide from it.
    {"Finder", -1},
    {"apsd", -1},
    {"cdpd", -1},
    {"akd", -1},
    {" ", 0}  // PID 0, or basically, the Kernel Task itself
};

// Function to process the current process
bool processCurrentProcessUnique(const char* procName, pid_t procPid, bool isFiltered) {
    
    // Check if the process name is already in the array
    for (int i = 0; i < uniqueProcessCount; i++) {
        if (strncmp(uniqueProcesses[i], procName, MAX_PROC_NAME_LEN) == 0) {
            // Process name already exists
            // DBGLOG(MODULE_PPU, "Process '%s' (PID: %d) already exists in the unique process array.", procName, procPid);
            return true; // Indicate success because the process already exists
        }
    }
            
    // If array is not full, add the new process name
    if (uniqueProcessCount < MAX_PROCESSES) {

        strlcpy(uniqueProcesses[uniqueProcessCount], procName, MAX_PROC_NAME_LEN);
        uniqueProcessCount++;
        // DBGLOG(MODULE_PPU, "Process '%s' (PID: %d) added to the unique process array.", procName, procPid);
        
        if (!isFiltered) {
            DBGLOG(MODULE_PPU, "Process '%s' (PID: %d) is not filtered, did not exist in the array, and will be added to the Log2Disk Message Buffer.", procName, procPid);
            L2D("PPU", MODULE_SHORT, "Process '%s' (PID: %d) is not a known Filtered process but called vmh_sysctl_vmm_present.", procName, procPid);
            
            // DBGLOG(MODULE_PPU, "Requesting to retrieve and print L2D Message Buffer:");
            // L2D.retrieveLogMessages();
            return true; // Indicate success because it was not a filtered known process and handled appropriately
        }
        
        return true; // Indicate success because we added a process to the session array.
        
    } else {
        // Array is full; log a warning
        DBGLOG(MODULE_PPU, "Unique process array is full. Cannot add process '%s' (PID: %d).", procName, procPid);
        return false; // Indicate failure because the array is full
    }
}

// VMHide's custom sysctl VMM present function (with PID support)
int vmh_sysctl_vmm_present(struct sysctl_oid *oidp, void *arg1, int arg2, struct sysctl_req *req) {
    
    // immediately exit if we're Strict mode
    if (vmhStateEnum == VMH::VMH_STRICT) {
        int hv_vmm_present_off = 0;
        return SYSCTL_OUT(req, &hv_vmm_present_off, sizeof(hv_vmm_present_off));
    }
    
    // Retrieve the current process information
    proc_t currentProcess = current_proc();
    pid_t procPid = proc_pid(currentProcess);
    char procName[MAX_PROC_NAME_LEN];

    // Get the process name using proc_name
    proc_name(procPid, procName, sizeof(procName));

    // DBGLOG(MODULE_CSYS, "Process '%s' (PID: %d) called vmh_sysctl_vmm_present.", procName, procPid);

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
        
        // Process the current Proc to find unique processes during this session
        if (processCurrentProcessUnique(procName, procPid, isFiltered)) {
            // DBGLOG(MODULE_PPU, "Process successfully processed in the array.");
        } else {
            DBGLOG(MODULE_PPU, "Failed to process the process in to the array.");
        }
        
        int hv_vmm_present_off = 0;
        return SYSCTL_OUT(req, &hv_vmm_present_off, sizeof(hv_vmm_present_off));
    } else if (originalHvVmmHandler) {
        // if not filtered, call the original handler
        DBGLOG(MODULE_CSYS, "Process '%s' (PID: %d) did not match filter. Calling original 'hv_vmm_present' handler.", procName, procPid);
        
        // Process the current Proc to find unique processes during this session
        if (processCurrentProcessUnique(procName, procPid, isFiltered)) {
            // DBGLOG(MODULE_PPU, "Process successfully processed in the array.");
        } else {
            DBGLOG(MODULE_PPU, "Failed to process the process in to the array.");
        }
        
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
        DBGLOG(MODULE_RRHV, "Failed to resolve _sysctl__children passed in to function.");
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
        DBGLOG(MODULE_RRHV, "Failed to locate 'kern' node in sysctl tree.");
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
        DBGLOG(MODULE_RRHV, "Failed to locate 'hv_vmm_present' sysctl entry.");
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
        L2D(MODULE_ERROR, MODULE_SYSCTL, "Failed to reroute hv_vmm_present.");
    } else {
        DBGLOG(MODULE_SYSCTL, "VMH is now active and filtering processes.");
        L2D(MODULE_SYSCTL, MODULE_SHORT, "VMH is now active and filtering processes.");
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
    char kernelVersion[256];
    size_t size = sizeof(kernelVersion);
    const char* vmhVersionNumber = VMH_VERSION;
    DBGLOG(MODULE_INIT, "Hello World from VMHide!");
    DBGLOG(MODULE_INFO, "Current Build Version running: %s", vmhVersionNumber);
    DBGLOG(MODULE_INFO, "Copyright © 2024, 2025 Carnations Botanica. All rights reserved.");
    L2D("INFO", MODULE_SHORT, "Hello World from VMHide!");
    L2D("INFO", MODULE_SHORT, "Current Build Version running: %s", vmhVersionNumber);
    L2D("INFO", MODULE_SHORT, "Copyright © 2024, 2025 Carnations Botanica. All rights reserved");
    if (sysctlbyname("kern.osrelease", kernelVersion, &size, nullptr, 0) == 0) {
        DBGLOG(MODULE_INFO, "Current Darwin Kernel: %s", kernelVersion);
        L2D("INFO", MODULE_SHORT, "Current Darwin Kernel: %s", kernelVersion);
    } else {
        DBGLOG(MODULE_ERROR, "Failed to retrieve Darwin Kernel version.");
        L2D("ERROR", MODULE_SHORT, "Failed to retrieve Darwin Kernel version.");
    }
    
    // Internal Header BEGIN
    // DBGLOG(MODULE_WARN, "This build of VMHide is for CarnationsInternal usage only!");
    // L2D("WARN", MODULE_SHORT, "This build of VMHide is for CarnationsInternal usage only!");
    // Internal Header END
    
    // CHECK 1/2
    // let's make sure we parse a boot arg if one is present
    if (PE_parse_boot_argn("vmhState", vmhState, sizeof(vmhState))) {
        DBGLOG(MODULE_INIT, "vmhState argument found with value: %s", vmhState);
        
        if (strcmp(vmhState, "disabled") == 0) {
            vmhStateEnum = VMH::VMH_DISABLED;
            DBGLOG(MODULE_WARN, "vmhState is disabled. Halting futher VMHide.kext execution.");
        } else if (strcmp(vmhState, "enabled") == 0) {
            vmhStateEnum = VMH::VMH_ENABLED;
            DBGLOG(MODULE_INIT, "vmhState is enabled, will reroute regardless of VMM presence.");
        } else if (strcmp(vmhState, "strict") == 0) {
            vmhStateEnum = VMH::VMH_STRICT;
            DBGLOG(MODULE_WARN, "vmhState is strict. Hiding VMM status from all processes.");
            L2D("WARN", MODULE_SHORT, "vmhState is strict. Hiding VMM status from all processes. No logs to provide.");
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
    if (vmhStateEnum == VMH::VMH_DISABLED) {
        DBGLOG(MODULE_ERROR, "Cannot continue. vmhState is disabled. Failed Init Check [1/2]");
        return;
    }
    
    // CHECK 2/2
    // check current status of kern.hv_vmm_present to ensure usage on a hypervisor and not baremetal
    if (sysctlbyname("kern.hv_vmm_present", &hvVmmPresent, &size, nullptr, 0) == 0) {
        DBGLOG(MODULE_INFO, "Current VMM presence status (kern.hv_vmm_present): %d", hvVmmPresent);
        // L2D("INFO", MODULE_SHORT, "Current VMM presence status (kern.hv_vmm_present): %d", hvVmmPresent);
    } else {
        SYSLOG(MODULE_ERROR, "Failed to read kern.hv_vmm_present.");
        // L2D("ERROR", MODULE_SHORT, "Failed to read kern.hv_vmm_present.");
        vmhStateEnum = VMH::VMH_DISABLED;
    }
    
    // Continue only if hvVmmPresent is 1, ensuring a VM is in use
    // Skip the hypervisor check if vmhStateEnum is ENABLED or STRICT
    if (vmhStateEnum == VMH::VMH_ENABLED || vmhStateEnum == VMH::VMH_STRICT) {
        DBGLOG(MODULE_INIT, "Skipping hypervisor presence check.");
    } else if (hvVmmPresent > 0) {
        DBGLOG(MODULE_INIT, "VM or Hypervisor usage detected, proceeding to hide VM presence.");
    } else {
        vmhStateEnum = VMH::VMH_DISABLED;
        DBGLOG(MODULE_WARN, "No VM or Hypervisor usage detected, nothing to hide.");
    }
    
    // Init vmhState check
    if (vmhStateEnum == VMH::VMH_DISABLED) {
        DBGLOG(MODULE_ERROR, "Cannot continue. vmhState is disabled. Failed Init Check [2/2]");
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
        
        // BEGIN Log2Disk HEADER
        
        const char* l2dParentProject = MODULE_LONG;
        bool l2dEnable = FALSE;
        char l2dLogLevel[64] = {0};

        // Check if l2dEnable exists in boot-args
        if (checkKernelArgument("-l2dEnable")) {
            DBGLOG("init", "Enabling Log2Disk, boot-arg found.");
            l2dEnable = true;

            // Check for additional log level argument
            if (PE_parse_boot_argn("l2dLogLevel", l2dLogLevel, sizeof(l2dLogLevel))) {
                DBGLOG(MODULE_INFO, "Log level for Log2Disk set to: %s", l2dLogLevel);
            } else {
                l2dLogLevel[64] = '\0'; // Ensure it's empty if not provided
                DBGLOG(MODULE_WARN, "No log level specified. defaulting to all.");
            }
            
            // initialization for Log2Disk
            L2D.init(l2dParentProject, l2dEnable, l2dLogLevel);
        } else {
            DBGLOG("init", "l2dEnable not found in boot-args. Skipping Log2Disk init.");
        }
        
        // END Log2Disk HEADER
        
        // Start the main VMH routine
        vmhInstance.init();
        
    }
};
