//
//  kern_start.cpp
//  VMHide
//
//  Created by RoyalGraphX on 10/12/24.
//

#include "kern_start.hpp"

static VMH vmhInstance;

VMH *VMH::callbackVMH;

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
    DBGLOG(MODULE_SYSCTL, "solveSysCtlChildrenAddr called after Patcher loaded successfully.");
    
    // Get the address of _sysctl__children here
    mach_vm_address_t sysCtlChildrenAddress = sysctlChildrenAddr(Patcher);
    
    // Log area
    DBGLOG(MODULE_SYSCTL, "mach_vm_address_t of sysCtlChildrenAddress is: 0x%llx", sysCtlChildrenAddress);
    
}

// Main VMH Routine function
void VMH::init() {
    
    // Start off the routine
    callbackVMH = this;
    DBGLOG(MODULE_INIT, "Hello World from VMHide!");
    
    // Register the root function to solve _sysctl__children on patcher load
    DBGLOG(MODULE_INIT, "Attempting to onPatcherLoadForce...");
    lilu.onPatcherLoadForce(solveSysCtlChildrenAddr);
    
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
