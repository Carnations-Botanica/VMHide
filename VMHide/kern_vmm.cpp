//
//  kern_vmm.cpp
//  VMHide
//
//  Created by RoyalGraphX on 6/3/25.
//
#include "kern_vmm.hpp"

// static integer to keep track of initial and post reroute presence.
int VMM::hvVmmPresent = 0;
size_t hvVmmIntSize = sizeof(VMM::hvVmmPresent);
sysctl_handler_t VMM::originalHvVmmHandler = nullptr;

/**
 * @brief Defines the list of processes to filter for the VMM module.
 * If a process calling kern.hv_vmm_present is in this list, the call will return 1.
 * For all other processes, the call will return 0.
 * The pid is not used in this check, so it can be left as 0.
 */
const VMH::DetectedProcess VMM::filteredProcs[] = {
	{"SoftwareUpdateNo", -1},
	{"softwareupdated", -1},
	{"com.apple.Mobile", -1},
	{"osinstallersetup", -1}
};

// VMHide's custom sysctl VMM present function
int VMH_sysctl_vmm_present(struct sysctl_oid *oidp, void *arg1, int arg2, struct sysctl_req *req) {
	
	// Retrieve the current process information
	proc_t currentProcess = current_proc();
	pid_t procPid = proc_pid(currentProcess);
	char procName[MAX_PROC_NAME_LEN] = {0};
	proc_name(procPid, procName, sizeof(procName));
	
	// Default to 0 (VMM not present). This will be the value for any process NOT in our list.
	int value_to_return = 0;
	bool isFiltered = false;

	// Determine the number of processes in our filter list
	const size_t num_filtered = sizeof(VMM::filteredProcs) / sizeof(VMM::filteredProcs[0]);

	// Loop through the filteredProcs list to check for a match
	for (size_t i = 0; i < num_filtered; ++i) {
		// Use strcmp to compare the current process name with the name in our list
		if (strcmp(procName, VMM::filteredProcs[i].name) == 0) {
			// Match found! Set the return value to 1 (VMM is present).
			value_to_return = 1;
			isFiltered = true;
			break; // Exit the loop since we found our match
		}
	}

	// Log the action for debugging purposes
	if (isFiltered) {
		DBGLOG(MODULE_CVMM, "Process '%s' (PID: %d) is on the filter list. Reporting hv_vmm_present as %d.", procName, procPid, value_to_return);
	} else {
		DBGLOG(MODULE_CVMM, "Process '%s' (PID: %d) is NOT on the filter list. Reporting hv_vmm_present as %d.", procName, procPid, value_to_return);
	}
	
	// Use the kernel macro to properly return the value to the calling process, depending on our context
	return SYSCTL_OUT(req, &value_to_return, sizeof(value_to_return));
}

// Function to reroute kern.hv_vmm_present function to our own custom one
bool reRouteHvVmm(KernelPatcher &patcher) {

	// Ensure that sysctlChildrenAddress exists before continuing
	if (!VMH::gSysctlChildrenAddr) {
		DBGLOG(MODULE_ERROR, "Failed to resolve _sysctl__children passed to function reRouteHvVmm.");
		panic(MODULE_RRHVM, "Failed to resolve _sysctl__children passed to function reRouteHvVmm");
		return false;
	} else {
		DBGLOG(MODULE_RRHVM, "Got address 0x%llx for _sysctl__children passed to function reRouteHvVmm.", VMH::gSysctlChildrenAddr);
	}

	// Case the address to sysctl_oid_list*
	sysctl_oid_list *sysctlChildren = reinterpret_cast<sysctl_oid_list *>(VMH::gSysctlChildrenAddr);

	// traverse the sysctl tree to locate 'kern'
	sysctl_oid *kernNode = nullptr;
	SLIST_FOREACH(kernNode, sysctlChildren, oid_link) {
		// Add a null-check for oid_name as a good safety practice within the loop
		if (kernNode && kernNode->oid_name && strcmp(kernNode->oid_name, "kern") == 0) {
			DBGLOG(MODULE_RRHVM, "Found 'kern' node.");
			break;
		}
	}

	// check if kern node was found
	if (!kernNode) {
		DBGLOG(MODULE_RRHVM, "Failed to locate 'kern' node in sysctl tree.");
		return false;
	}

	// --- SAFETY CHECKS ADDED ---
	// Before traversing children, validate that the kern node is structured as we expect.
	// 1. It must be a NODE type, indicating it's a parent in the tree.
	// 2. The pointer to its children (oid_arg1) must not be null.
	if (!(kernNode->oid_kind & CTLTYPE_NODE) || !kernNode->oid_arg1) {
		DBGLOG(MODULE_ERROR, "'kern' sysctl OID is not a valid node or has no children. Cannot traverse.");
		return false;
	}
	// --- END SAFETY CHECKS ---

	// Now it's safe to traverse the children of 'kern'
	sysctl_oid_list *kernChildren = reinterpret_cast<sysctl_oid_list *>(kernNode->oid_arg1);
	sysctl_oid *vmmNode = nullptr;
	SLIST_FOREACH(vmmNode, kernChildren, oid_link) {
		if (vmmNode && vmmNode->oid_name && strcmp(vmmNode->oid_name, "hv_vmm_present") == 0) {
			DBGLOG(MODULE_RRHVM, "Found 'hv_vmm_present' node.");
			break;
		}
	}

	// check if the vmm present entry was found
	if (!vmmNode) {
		DBGLOG(MODULE_RRHVM, "Failed to locate 'hv_vmm_present' sysctl entry.");
		return false;
	}

	// Check if the found vmmNode's handler is NULL, which might be unexpected.
	if (vmmNode->oid_handler == nullptr) {
		DBGLOG(MODULE_RRHVM, "Failed to save original 'hv_vmm_present' sysctl handler: The existing handler was NULL.");
		return false; // Return false as this is considered a failure condition.
	}
	
	// Save the original handler
	VMM::originalHvVmmHandler = vmmNode->oid_handler;
	DBGLOG(MODULE_RRHVM, "Successfully saved original 'hv_vmm_present' sysctl handler.");
	 
	// On macOS Ventura (Darwin 22) and newer (?), we must disable kernel write protection.
	// Not too sure when this began to be a requirement, but let's do it for Vent+ for now.
	if (getKernelVersion() >= KernelVersion::Ventura) {
		DBGLOG(MODULE_RRHVM, "Ventura or newer detected. Disabling kernel write protection...");
		PANIC_COND(MachInfo::setKernelWriting(true, patcher.kernelWriteLock) != KERN_SUCCESS, MODULE_SHORT, "Failed to disable kernel write protection.");
	}
	
	// Reroute the handler to our custom function.
	vmmNode->oid_handler = VMH_sysctl_vmm_present;
	
	// Re-enable kernel write protection if we disabled it.
	if (getKernelVersion() >= KernelVersion::Ventura) {
		DBGLOG(MODULE_RRHVM, "Re-enabling kernel write protection.");
		MachInfo::setKernelWriting(false, patcher.kernelWriteLock);
	}
	
	DBGLOG(MODULE_RRHVM, "Successfully rerouted 'hv_vmm_present' sysctl handler.");
	return true;
	
}

// Function for the VMM init routine
void VMM::init(KernelPatcher &Patcher) {

	// Register a request to reroute to our custom function
	DBGLOG(MODULE_VMM, "VMM::init() called. VMM module is starting.");
	
	if (!VMH::gSysctlChildrenAddr) {
		DBGLOG(MODULE_ERROR, "VMH::gSysctlChildrenAddr is not set. Cannot perform VMM rerouting.");
		panic(MODULE_LONG, "VMH::gSysctlChildrenAddr is not set.");
		return;
	}
	
	// Perform rerouting, as Patcher is available and gSysctlChildrenAddr is known (hopefully by now, yes it is)
	if (!reRouteHvVmm(Patcher)) {
		DBGLOG(MODULE_ERROR, "Failed to reroute kern.hv_vmm_present.");
		panic(MODULE_LONG, "Failed to reroute kern.hv_vmm_present.");
	} else {
		DBGLOG(MODULE_INFO, "kern.hv_vmm_present rerouted successfully.");
	}

}
