//
//  kern_start.hpp
//  VMHide
//
//  Created by RoyalGraphX on 10/15/24.
//

#ifndef kern_start_h
#define kern_start_h

#include <Headers/plugin_start.hpp>
#include <Headers/kern_patcher.hpp>
#include <Headers/kern_api.hpp>
#include <Headers/kern_util.hpp>
#include <mach/i386/vm_types.h>
#include <libkern/libkern.h>
#include <sys/sysctl.h>

// Macros for sysctl types
#define CTLTYPE             0xf     // Mask for the type
#define CTLTYPE_NODE        1       // name is a node
#define CTLTYPE_INT         2       // name describes an integer
#define CTLTYPE_STRING      3       // name describes a string
#define CTLTYPE_QUAD        4       // name describes a 64-bit number
#define CTLTYPE_OPAQUE      5       // name describes a structure
#define CTLTYPE_STRUCT      CTLTYPE_OPAQUE  // name describes a structure

#define SYSCTL_OUT(r, p, l) (r->oldfunc)(r, p, l)

// Function prototypes
void vmhInit();
mach_vm_address_t parseSysctlChildren();
bool reRouteHvVmm(mach_vm_address_t sysctlChildrenAddress);
int vmh_sysctl_vmm_present(struct sysctl_oid *oidp, void *arg1, int arg2, struct sysctl_req *req);

// Enum to represent VMHide states
enum VmhState {
    VMH_DISABLED,
    VMH_ENABLED,
    VMH_PASSTHROUGH
};

#endif /* kern_start_hpp */
