//
//  BlockList.hpp
//  VMHide
//
//  Created by Zormeister on 1/12/2024.
//

#ifndef BlockList_hpp
#define BlockList_hpp

#include <Headers/plugin_start.hpp>
#include <Headers/kern_patcher.hpp>
#include <Headers/kern_api.hpp>
#include <Headers/kern_util.hpp>
#include <mach/i386/vm_types.h>
#include <libkern/libkern.h>
#include <sys/sysctl.h>

class BlockList {
    static BlockList *newList();
    
    void init();
    
public:
    bool addProcess(const char *name, pid_t pid);
};

#endif
