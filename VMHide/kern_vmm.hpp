//
//  kern_vmm.hpp
//  VMHide
//
//  Created by RoyalGraphX on 6/3/25.
//

#ifndef kern_vmm_hpp
#define kern_vmm_hpp

// Include Parent Module
#include "kern_start.hpp"

// Logging Defs
#define MODULE_VMM "VMM"
#define MODULE_RRHVM "RRHVM"
#define MODULE_CVMM "CVMM"

// VMM Patcher Class
class VMM {
public:
	
	// Declaration for the init function
	static void init(KernelPatcher &Patcher);

	// Presence Tracker
	static int hvVmmPresent;
	
	// Store the original handler
	static sysctl_handler_t originalHvVmmHandler;

	// Declaration for the array of processes to filter
    static const VMH::DetectedProcess filteredProcs[];
	
	// is Process in Filter Tracker
	static bool isProcFiltered;

private:
	
	// None at the moment
	
};

#endif /* kern_vmm_hpp */
