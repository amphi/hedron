/*
 * Static Allocator
 *
 * This allocator only takes a start address, and from there on returns
 * consecutive page addresses when alloc() is called. This allocator can not
 * free pages, it only allocates.
 */

#pragma once

#include "extern.hpp"
#include "memory.hpp"
#include "spinlock.hpp"

class Static_allocator
{
private:
    Spinlock lock_;

    mword  virt_base_;
    size_t last_idx_;

    inline mword index_to_page(size_t idx)
    {
        return virt_base_ + idx * PAGE_SIZE;
    }

    inline mword virt_to_phys(mword virt)
    {
        return VIRT_TO_PHYS_NORELOC (virt) + PHYS_RELOCATION;
    }

    inline mword phys_to_virt(mword phys)
    {
        return PHYS_TO_VIRT_NORELOC (phys - PHYS_RELOCATION);
    }

public:
    static Static_allocator allocator;

    Static_allocator(mword virt_base)
        : virt_base_(virt_base)
        , last_idx_(0)
    { }

    void* alloc();
    //void  free(mword addr);

    static inline void* phys_to_ptr(Paddr phys)
    {
        return reinterpret_cast<void*>(allocator.phys_to_virt(static_cast<mword>(phys)));
    }

    static inline mword ptr_to_phys(void* virt)
    {
        return allocator.virt_to_phys(reinterpret_cast<mword>(virt));
    }
};
