//
//  kern_start.cpp
//  VMHide
//
//  Created by RoyalGraphX on 10/12/24.
//

#include "kern_start.hpp"
#include "kern_vmm.hpp"
#include "kern_kextmanager.hpp"
#include "kern_securelevel.hpp"
#include "kern_csr.hpp"
#include "kern_ioreg.hpp"

static VMH vmhInstance;
VMH *VMH::callbackVMH;

// default to enabled, why else would someone use this?
VMH::VmhState VMH::vmhStateEnum = VMH::VMH_DEFAULT;

// Definition for the global _sysctl__children address
mach_vm_address_t VMH::gSysctlChildrenAddr = 0;

// To only be modified by CarnationsInternal, to display various Internal logs and headers
const bool VMH::IS_INTERNAL = true; // MUST CHANCE THIS TO FALSE BEFORE CREATING COMMITS

// Function to process a Proc's Uniqueness in terms of a seen/unseen basis
bool VMH::processCurrentProcessUnique(const char* procName, pid_t procPid, bool isFiltered) {

    // Check if the process name is already in the array
    for (int i = 0; i < uniqueProcessCount; i++) {
        if (strncmp(uniqueProcesses[i], procName, MAX_PROC_NAME_LEN) == 0) {
            DBGLOG(MODULE_PPU, "Process '%s' (PID: %d) already exists in the unique process array.", procName, procPid);
            return true; // Indicate success because the process already exists
        }
    }

    // If array is not full, add the new process name
    if (uniqueProcessCount < MAX_PROCESSES) {
        strlcpy(uniqueProcesses[uniqueProcessCount], procName, MAX_PROC_NAME_LEN);
        uniqueProcessCount++;
        DBGLOG(MODULE_PPU, "Process '%s' (PID: %d) added to the unique process array.", procName, procPid);
        return true; // Indicate success because we added a process to the session array.
    } else {
        // Array is full; log a warning
        DBGLOG(MODULE_PPU, "Unique process array is full. Cannot add process '%s' (PID: %d).", procName, procPid);
        return false; // Indicate failure because the array is full
    }

}

// Function to get _sysctl__children memory address
mach_vm_address_t VMH::sysctlChildrenAddr(KernelPatcher &patcher) {
	
    // Resolve the _sysctl__children symbol with the given patcher
    mach_vm_address_t resolvedAddress = patcher.solveSymbol(KernelPatcher::KernelID, "_sysctl__children");

    // Check if the address was successfully resolved, else return 0
    if (resolvedAddress) {
        DBGLOG(MODULE_SYSCA, "Resolved _sysctl__children at address: 0x%llx", resolvedAddress);

        // Optional: Iterate and log OIDs for debugging (can be extensive)
        #if DEBUG
        sysctl_oid_list *sysctlChildrenList = reinterpret_cast<sysctl_oid_list *>(resolvedAddress);
        DBGLOG(MODULE_SYSCA, "Sysctl children list at address: 0x%llx", reinterpret_cast<mach_vm_address_t>(sysctlChildrenList));
        sysctl_oid *oid;
        SLIST_FOREACH(oid, sysctlChildrenList, oid_link) {
            DBGLOG(MODULE_SYSCA, "OID Name: %s, OID Number: %d", oid->oid_name, oid->oid_number);
        }
        #endif
        
        return resolvedAddress;
    } else {
        KernelPatcher::Error err = patcher.getError();
        DBGLOG(MODULE_SYSCA, "Failed to resolve _sysctl__children. (Lilu returned: %d)", err);
        patcher.clearError();
        return 0;
    }
	
}

// Callback function to solve for and store _sysctl__children address
void VMH::solveSysCtlChildrenAddr(void *user __unused, KernelPatcher &Patcher) {
    DBGLOG(MODULE_SSYSCTL, "VMH::solveSysCtlChildrenAddr called successfully. Attempting to resolve and store _sysctl__children address.");
	
    VMH::gSysctlChildrenAddr = VMH::sysctlChildrenAddr(Patcher);
	
    if (VMH::gSysctlChildrenAddr) {
        DBGLOG(MODULE_SSYSCTL, "Successfully resolved and stored _sysctl__children address: 0x%llx", VMH::gSysctlChildrenAddr);
    } else {
        DBGLOG(MODULE_SSYSCTL, "Failed to resolve _sysctl__children address. VMH::gSysctlChildrenAddr is NULL.");
		panic(MODULE_SHORT, "Failed to resolve _sysctl__children address. VMH::gSysctlChildrenAddr is NULL.");
    }
	
    // Now, initialize dependent modules, passing the KernelPatcher instance
    DBGLOG(MODULE_INIT, "Initializing VMM module.");
    VMM::init(Patcher);
	
    DBGLOG(MODULE_SSYSCTL, "VMH::solveSysCtlChildrenAddr finished.");
}

// Main VMH Routine function
void VMH::init() {

    // Guest CPUID Check Header BEGIN
    // DO NOT MODIFY. NO EXPRESSED PERMISSION IS GIVEN BY CARNATIONS BOTANICA TO DO SO. NO EXCEPTIONS.
    char vendor[13];
    char hypervisor_vendor[13];
    bzero(vendor, sizeof(vendor));
    bzero(hypervisor_vendor, sizeof(hypervisor_vendor));

    // Get pointer to cpuid info here
    auto cpuinfo = cpuid_info();
	if (cpuinfo) {
		// Get Hypervisor string, with a manual reading
		uint32_t data[4] {0};
		asm volatile("cpuid" : "=a"(data[0]), "=b"(data[1]), "=c"(data[2]), "=d"(data[3]) : "a"(0x40000000));
		memcpy(&hypervisor_vendor[0], &data[1], 4); // EBX
		memcpy(&hypervisor_vendor[4], &data[2], 4); // ECX
		memcpy(&hypervisor_vendor[8], &data[3], 4); // EDX
		hypervisor_vendor[12] = '\0';

		// Get CPU vendor string
		memcpy(&vendor[0], cpuinfo->cpuid_vendor, 12);
		vendor[12] = '\0';
			
		DBGLOG(MODULE_INFO, "Hypervisor vendor detected: %s", hypervisor_vendor);
		DBGLOG(MODULE_INFO, "CPU vendor detected: %s", vendor);

		// DarwinKVM does not run on anything other than QEMU guests, for obvious reasons... this is clearly designed for DarwinKVM users.
		// DO NOT MODIFY THE MARKED HEADER SECTION FOR CPUID DETECTION. Do not include VMHide in your EFIs and share them. Do not redistribute VMHide
		// in any way, shape, or form. All users must source VMHide on their own, and apply the kernel extension with ProperTree manually.
		// If you are found to contain VMHide within your repository which provides an OpenCore EFI, you are subject to a DMCA request for violating
		// this incredibly clear warning and demonstration that we do not condone bypassing these efforts to limit the kind of users who can use VMHide.
		// For contributors, you can freely fork and work on improving VMHide without worry and with CI enabled. This header is still not
		// allowed for modification, even on forks with CI enabled, which allow for circumventing this header by providing an alternative
		// binary without said header. Again, your repository *will* be DMCA'd. You have been expressed no permission multiple times, and
		// we are allowed to protect our work as we see fit. Thank you for understanding, and spread education not prebuits.
		// Final note: Darwin-V is an idea floating in my head... that's why it's allowed here.
		if (hypervisor_vendor[0] != '\0' && // Check if string is not empty
			strcmp(hypervisor_vendor, "TCGTCGTCGTCG") != 0 && // Check if not TCG
			strcmp(hypervisor_vendor, "KVMKVMKVM") != 0 && // Check if not KVM
			strcmp(hypervisor_vendor, "MicrosoftHv") != 0) // Check if not MicrosoftHv
		{
			DBGLOG(MODULE_ERROR, "Unsupported Hypervisor vendor detected: %s. VMHide only supports QEMU/TCG, QEMU/KVM, Apple HVF, or Hyper-V guests.", hypervisor_vendor);
			panic(MODULE_SHORT, "Unsupported Hypervisor vendor detected: %s. VMHide only supports QEMU/TCG, QEMU/KVM, Apple HVF, or Hyper-V guests.", hypervisor_vendor);
			return; // Should not be reached due to panic, but good practice anyways
		}

		// If Hypervisor is QEMU, move onto checking the Vendor string.
		// DarwinKVM ensures a GenuineIntel CPUID regardless of Host CPU, and does not use AMD Vanilla Patches.
		// This allows for any/all other users on QEMU to safely still use VMHide, so long as their XML is configured properly.
		if (strcmp(vendor, "GenuineIntel") != 0) {
			DBGLOG(MODULE_ERROR, "Incorrect CPU vendor detected: %s. VMHide only supports guests reporting as GenuineIntel CPUs.", vendor);
			panic(MODULE_SHORT, "Incorrect CPU vendor detected: %s. VMHide only supports guests reporting as GenuineIntel CPUs.", vendor);
			return;
		}
	} else {
        DBGLOG(MODULE_ERROR, "Failed to retrieve CPUID information.");
        panic(MODULE_SHORT, "Failed to retrieve CPUID information.");
        return;
	}
    // DO NOT MODIFY. NO EXPRESSED PERMISSION IS GIVEN BY CARNATIONS BOTANICA. NO EXCEPTIONS.
    // Guest CPUID Check Header END
    
    // Start off the routine
    callbackVMH = this;
    int major = getKernelVersion();
    int minor = getKernelMinorVersion();
    const char* vmhVersionNumber = VMH_VERSION;
    DBGLOG(MODULE_INIT, "Hello World from VMHide!");
    DBGLOG(MODULE_INFO, "Current Build Version running: %s", vmhVersionNumber);
    DBGLOG(MODULE_INFO, "Copyright © 2024, 2025 Carnations Botanica. All rights reserved.");
    if (major > 0) {
        DBGLOG(MODULE_INFO, "Current Darwin Kernel version: %d.%d", major, minor);
    } else {
        DBGLOG(MODULE_ERROR, "WARNING: Failed to retrieve Darwin Kernel version.");
    }
    
    // Internal Header BEGIN
    if (VMH::IS_INTERNAL) {
        DBGLOG(MODULE_WARN, "");
        DBGLOG(MODULE_WARN, "==================================================================");
		DBGLOG(MODULE_WARN, "This build of %s is for CarnationsInternal usage only!", MODULE_LONG);
        DBGLOG(MODULE_WARN, "If you received a copy of this binary as a tester, DO NOT SHARE.");
        DBGLOG(MODULE_WARN, "=================================================================");
        DBGLOG(MODULE_WARN, "");
    }
    // Internal Header END
	
    // Register the main sysctl children address resolver
    DBGLOG(MODULE_INIT, "Registering VMH::solveSysCtlChildrenAddr with onPatcherLoadForce.");
    lilu.onPatcherLoadForce(&VMH::solveSysCtlChildrenAddr);

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
    LiluAPI::AllowNormal,
    // LiluAPI::AllowSafeMode |
    // LiluAPI::AllowInstallerRecovery,
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
