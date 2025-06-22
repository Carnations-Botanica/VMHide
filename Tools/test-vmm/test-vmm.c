//
//  main.c
//  test-vmm
//
//  Created by RoyalGraphX on 5/12/25.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysctl.h> // Required for sysctlbyname
#include <errno.h>      // Required for errno

int main(int argc, const char * argv[]) {
    int vmm_present = 0;
    size_t len = sizeof(vmm_present);
    const char* vmm_sysctl_name = "kern.hv_vmm_present";

    printf("Attempting to read sysctl: %s\n", vmm_sysctl_name);

    // Attempt to get the sysctl value
    // int sysctlbyname(const char *, void *, size_t *, void *, size_t);
    if (sysctlbyname(vmm_sysctl_name, &vmm_present, &len, NULL, 0) == -1) {
        perror("Error calling sysctlbyname");
        if (errno == ENOENT) {
            printf("Sysctl '%s' does not exist. This could mean sysctl is unavailable.\n", vmm_sysctl_name);
        }
        return 1; // Indicate an error
    }

    if (len == sizeof(vmm_present)) {
        printf("Sysctl '%s' value: %d\n", vmm_sysctl_name, vmm_present);
        if (vmm_present == 1) {
            printf("Indicates a Virtual Machine Environment is present.\n");
        } else {
            printf("Indicates a Virtual Machine Environment is NOT present (or value is 0).\n");
        }
    } else {
        printf("Sysctl '%s' returned an unexpected length: %zu\n", vmm_sysctl_name, len);
        return 1; // Indicate an error
    }

    printf("test-vmm finished.\n");
    return 0; // Indicate success
}
