//
//  kern_start.cpp
//  VMHide
//
//  Created by RoyalGraphX on 10/12/24.
//

#include "kern_start.hpp"

static VMH vmhInstance;

VMH *VMH::callbackVMH;

// Main VMH Routine function
void VMH::init() {
    
    callbackVMH = this;
    DBGLOG(MODULE_INIT, "Hello World from VMHide!");
    
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
