//
//  zutil.c
//  itlwm
//
//  Created by qcwap on 2020/9/4.
//  Copyright © 2020 钟先耀. All rights reserved.
//

#include <zutil.h>
#include <stddef.h>

extern "C" {
    typedef struct z_mem
    {
        size_t alloc_size;
        uint8_t data[] __attribute__((aligned(16)));
    } z_mem;
    
    void *zcalloc(void *opaque, uint items, uint size)
    {
        size_t total = (size_t)items * (size_t)size;
        size_t allocSize = sizeof(z_mem) + total;
        
        z_mem *zmem = (z_mem*)IOMallocZero(allocSize);
        if (!zmem)
            return NULL;
        
        zmem->alloc_size = allocSize;
        return (void*)zmem->data;
    }
    
    void zcfree(void *opaque, void *ptr)
    {
        if (!ptr)
            return;
        uintptr_t addr = (uintptr_t)ptr - offsetof(z_mem, data);
        z_mem *zmem = (z_mem*)addr;
        IOFree((void*)zmem, zmem->alloc_size);
    }
}

int _stop(struct kmod_info*, void*) {
    IOLog("_stop(struct kmod_info*, void*) has been invoked\n");
    return 0;
};
int _start(struct kmod_info*, void*) {
    IOLog("_start(struct kmod_info*, void*) has been invoked\n");
    return 0;
};
