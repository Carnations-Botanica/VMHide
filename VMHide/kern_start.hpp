//
//  kern_start.hpp
//  VMHide
//
//  Created by RoyalGraphX on 10/15/24.
//

#ifndef kern_start_h
#define kern_start_h

// VMHide Includes
#include <Headers/plugin_start.hpp>
#include <Headers/kern_patcher.hpp>
#include <Headers/kern_api.hpp>
#include <Headers/kern_util.hpp>
#include <Headers/kern_mach.hpp>
#include <mach/i386/vm_types.h>
#include <libkern/libkern.h>
#include <IOKit/IOLib.h>
#include <sys/sysctl.h>
#include <i386/cpuid.h>

// Logging Defs
#define MODULE_INIT "INIT"
#define MODULE_SHORT "VMH"
#define MODULE_LONG "VMHide"
#define MODULE_ERROR "ERR"
#define MODULE_WARN "WARN"
#define MODULE_INFO "INFO"
#define MODULE_CUTE "\u2665"

// Function Logging Defs
#define MODULE_PPU "PPU"
#define MODULE_SYSCA "SYSCA"
#define MODULE_SSYSCTL "SSYSCTL"

// VMH Root/Parent Class
class VMH {
public:

    /**
     * Maximum number of processes we can track, Maximum length of a process name
     */
    #define MAX_PROCESSES 256
    #define MAX_PROC_NAME_LEN 256
	
	/**
	 * Process Uniqueness of a proc
	 */
	static char uniqueProcesses[MAX_PROCESSES][MAX_PROC_NAME_LEN];
	static int uniqueProcessCount;
	
	/**
	 * Declaration of Func to proc Uniqueness
	 */
	static bool processCurrentProcessUnique(const char* procName, pid_t procPid, bool isFiltered);

    /**
     * Standard Init and deInit functions
     */
    void init();
    void deinit();

    /**
     * Enum to represent VMHide states
     */
    enum VmhState {
        VMH_INVERTED,
        VMH_UNDERCOVER,
        VMH_INTERNAL,
        VMH_DISABLED,
        VMH_ENABLED,
        VMH_DEFAULT,
        VMH_STRICT,
    };
	
	/**
	* Publicly accessible state enum
	*/
	static VmhState vmhStateEnum;
	
	/**
	* Publicly accessible internal build flag
	*/
	static const bool IS_INTERNAL;
	
	/**
	* Struct to hold both process name and potential PID
	*/
	struct DetectedProcess {
		const char *name;
    	pid_t pid;
	};
	
    /**
     * @brief Stores the resolved address of the kernel's _sysctl__children list.
     * Populated by VMH::solveSysCtlChildrenAddr.
     */
    static mach_vm_address_t gSysctlChildrenAddr;
	
    /**
     * @brief Resolves the address of the kernel's _sysctl__children list.
     * This function directly uses the KernelPatcher to find the symbol.
     * @param patcher A reference to the KernelPatcher instance.
     * @return The address of _sysctl__children, or 0 if not found.
     */
    static mach_vm_address_t sysctlChildrenAddr(KernelPatcher &patcher);
	
    /**
     * @brief Callback for Lilu's onPatcherLoad. Solves for and stores _sysctl__children address.
     * This function calls VMH::sysctlChildrenAddr and stores the result in VMH::gSysctlChildrenAddr.
     * @param user User-defined pointer (unused).
     * @param Patcher A reference to the KernelPatcher instance.
     */
    static void solveSysCtlChildrenAddr(void *user, KernelPatcher &Patcher);
	
private:

    /**
     *  Private self instance for callbacks
     */
    static VMH *callbackVMH;

};

#endif /* kern_start_hpp */

#ifndef VMH_VERSION /* VMH_VERSION Macro */
#define VMH_VERSION "Unknown"

#endif /* VMH_VERSION Macro */
