//
//  main.c
//  test-sip
//
//  Created by RoyalGraphX on 5/12/25.
//
//  Checks SIP status using three methods:
//  1. Indirectly via kern.securelevel sysctl.
//  2. Directly via executing the /usr/bin/csrutil command.
//  3. Programmatically by calling the kernel's csr_get_active_config and csr_check functions.
//

#include <stdio.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h> // For geteuid()

// --- Programmatic SIP Check Definitions (from Gist) ---

// Define csr_config_t if it's not in a system header
typedef uint32_t csr_config_t;

// Prototypes for the undocumented kernel functions.
// The linker will resolve these against the running system's kernel.
extern int csr_get_active_config(csr_config_t *config);
extern int csr_check(csr_config_t mask);

// Known CSR flags
#define CSR_ALLOW_UNTRUSTED_KEXTS       (1 << 0)
#define CSR_ALLOW_UNRESTRICTED_FS       (1 << 1)
#define CSR_ALLOW_TASK_FOR_PID          (1 << 2)
#define CSR_ALLOW_KERNEL_DEBUGGER       (1 << 3)
#define CSR_ALLOW_APPLE_INTERNAL        (1 << 4)
#define CSR_ALLOW_UNRESTRICTED_DTRACE   (1 << 5)
#define CSR_ALLOW_UNRESTRICTED_NVRAM    (1 << 6)
#define CSR_ALLOW_DEVICE_CONFIGURATION  (1 << 7)
#define CSR_ALLOW_ANY_RECOVERY_OS       (1 << 8)
#define CSR_ALLOW_UNAPPROVED_KEXTS      (1 << 9)
#define CSR_ALLOW_EXECUTABLE_POLICY_OVERRIDE (1 << 10)
#define CSR_ALLOW_UNAUTHENTICATED_ROOT  (1 << 11)

// Struct to hold flag info for easy iteration
typedef struct {
    csr_config_t mask;
    const char *name;
} sip_flag_t;

sip_flag_t all_sip_flags[] = {
    {CSR_ALLOW_UNTRUSTED_KEXTS,       "Kext Signing"},
    {CSR_ALLOW_UNRESTRICTED_FS,       "Filesystem Protections"},
    {CSR_ALLOW_TASK_FOR_PID,          "Task for PID"},
    {CSR_ALLOW_KERNEL_DEBUGGER,       "Debugging Restrictions"},
    {CSR_ALLOW_APPLE_INTERNAL,        "Apple Internal"},
    {CSR_ALLOW_UNRESTRICTED_DTRACE,   "DTrace Restrictions"},
    {CSR_ALLOW_UNRESTRICTED_NVRAM,    "NVRAM Protections"},
    {CSR_ALLOW_DEVICE_CONFIGURATION,  "Device Configuration"},
    {CSR_ALLOW_ANY_RECOVERY_OS,       "BaseSystem Verification"},
    {CSR_ALLOW_UNAUTHENTICATED_ROOT,  "Authenticated Root"},
    // Add other flags here if needed
};

// --- End Programmatic SIP Check Definitions ---


// Enum to represent the possible SIP statuses from csrutil
typedef enum {
    SIP_STATUS_UNKNOWN,
    SIP_STATUS_ENABLED,
    SIP_STATUS_DISABLED // Includes "disabled." and "unknown (Custom Configuration)."
} sip_status_t;

// Function to execute "csrutil status" and parse its output
sip_status_t get_csrutil_status(void) {
    FILE *pipe = NULL;
    char buffer[256];
    sip_status_t status = SIP_STATUS_UNKNOWN;
    const char *command = "/usr/bin/csrutil status";
    const char *enabled_str = "enabled.";
    const char *disabled_str = "disabled.";
    const char *custom_config_str = "unknown (Custom Configuration).";


    printf("\nExecuting command: %s\n", command);

    pipe = popen(command, "r");
    if (!pipe) {
        perror("popen failed");
        return SIP_STATUS_UNKNOWN;
    }

    // Read the output line by line
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        printf("  Found status: %s", buffer); // Print the raw line for context
        // Check for the "enabled." string
        if (strstr(buffer, enabled_str) != NULL) {
            status = SIP_STATUS_ENABLED;
            break; 
        }
        // Check for "disabled."
        if (strstr(buffer, disabled_str) != NULL) {
            status = SIP_STATUS_DISABLED;
            break;
        }
        // Check for "unknown (Custom Configuration)." and interpret as disabled/spoofed
        if (strstr(buffer, custom_config_str) != NULL) {
            printf(" -> Interpreting as Disabled\n");
            status = SIP_STATUS_DISABLED;
            break;
        }
    }

    // Check the exit status of the command
    int exit_status = pclose(pipe);
    if (exit_status != 0) {
        // WIFEXITED and WEXITSTATUS are standard ways to check exit codes from child processes
        if (WIFEXITED(exit_status)) {
             printf("Warning: csrutil command exited with status %d.\n", WEXITSTATUS(exit_status));
        } else {
             printf("Warning: csrutil command terminated abnormally.\n");
        }
    }


    return status;
}

// NEW: Function to check SIP programmatically
void check_sip_programmatically(void) {
    csr_config_t config = 0;
    int result = 0;

    // First, get the active configuration bitmask
    result = csr_get_active_config(&config);
    if (result != 0) {
        printf("Failed to get active SIP configuration. Error: %d (%s)\n", errno, strerror(errno));
        return;
    }

    printf("Programmatically retrieved SIP configuration mask: 0x%x\n", config);

    // Now, check each individual protection.
    // Remember: csr_check returns 0 if the protection is DISABLED.
    size_t num_flags = sizeof(all_sip_flags) / sizeof(all_sip_flags[0]);
    for (size_t i = 0; i < num_flags; ++i) {
        result = csr_check(all_sip_flags[i].mask);
        printf("  %-28s: %s\n", all_sip_flags[i].name, (result == 0) ? "Disabled" : "Enabled");
    }
}


int main(int argc, const char * argv[]) {
    
    printf("SIP Status Test Utility\n");
    printf("=======================\n");

    if (geteuid() != 0) {
        printf("Warning: This tool may provide more accurate results when run as root (sudo).\n\n");
    }
    
    // --- Checking kern.securelevel (Indirect Indicator) ---
    printf("--- Checking kern.securelevel (Indirect Indicator) ---\n");
    int securelevel = -1;
    size_t size = sizeof(securelevel);
    if (sysctlbyname("kern.securelevel", &securelevel, &size, NULL, 0) == -1) {
        if (errno == ENOENT) {
            printf("kern.securelevel sysctl does not exist on this system.\n");
        } else {
             printf("Failed to read kern.securelevel.\n");
        }
    } else {
        printf("kern.securelevel is %d\n", securelevel);
        if (securelevel > 0) {
            printf("System security level is enhanced (likely due to SIP being enabled).\n");
        } else {
            printf("System security level is at base level (SIP might be disabled or bypassed).\n");
        }
    }
     printf("Note: kern.securelevel is an indirect indicator and does not directly report the full SIP status.\n");


    // --- Checking programmatically via kernel functions ---
    printf("\n--- Checking Programmatically (Direct Kernel Functions) ---\n");
    check_sip_programmatically();


    // --- Checking using csrutil command (Official Status) ---
    printf("\n--- Checking using csrutil command (Direct/Official Status) ---\n");
    sip_status_t csrutil_status = get_csrutil_status();

    printf("\nOfficial SIP Status (from csrutil): ");
    switch (csrutil_status) {
        case SIP_STATUS_ENABLED:
            printf("Enabled\n");
            break;
        case SIP_STATUS_DISABLED:
            printf("Disabled (or Custom Configuration)\n");
            break;
        case SIP_STATUS_UNKNOWN:
            printf("Unknown (Could not execute command or parse output)\n");
            break;
    }

    printf("\nTest finished.\n");

    return (csrutil_status == SIP_STATUS_ENABLED || csrutil_status == SIP_STATUS_DISABLED) ? EXIT_SUCCESS : EXIT_FAILURE;
}
