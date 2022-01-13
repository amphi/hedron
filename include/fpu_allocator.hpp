#pragma once

/**
 * FPU allocator 
 * 
 * Allocates pd-local memory for the FPU.
 */

#include "assert.hpp"
#include "math.hpp"
#include "memory.hpp"
#include "spinlock.hpp"

class Space_mem;

// it should be relatively easy to turn this into a more generic allocator later
class Fpu_allocator 
{
private:
    Spinlock lock_;

    // the memory for the fpu has to be 64-bytes aligned, otherwise xsave/
    // xsaveopt and xrstor will throw a general protection exception
    static constexpr size_t ALIGNMENT { 64 };

    const size_t ELEM_SIZE;  // size of an element (fpu state + alignment)
    const size_t ELEM_COUNT; // maximum amount of elements

    size_t free_chunks_; // allocated freed chunks are bitwise encoded here
    size_t last_chunk_;  // the first unallocated chunk

    Space_mem* space_mem_;

public:
    Fpu_allocator(size_t fpu_size, Space_mem* space_mem)
        : ELEM_SIZE(align_up(fpu_size+1, ALIGNMENT))
        , ELEM_COUNT((SPC_LOCAL_FPU_E - SPC_LOCAL_FPU) / ELEM_SIZE)
        , free_chunks_(0)
        , last_chunk_(0)
        , space_mem_(space_mem)
    { }

    // allocates memory from the fpu-space and returns a pointer to the
    // allocated memory
    void* alloc();

    // marks the memory at the given address as free
    void free(void* vaddr);

private:
    // Returns the virtual address for the element with the given index
    inline mword idx_to_virt(size_t idx)
    {
        assert (idx < ELEM_COUNT);
        return SPC_LOCAL_FPU + idx * ELEM_SIZE;
    }

    // Returns the index for the element at the given address.
    inline size_t virt_to_idx(mword vaddr)
    {
        assert (vaddr >= SPC_LOCAL_FPU && vaddr < SPC_LOCAL_FPU_E);
        assert ((vaddr - SPC_LOCAL_FPU) % ELEM_SIZE == 0);
        return (vaddr - SPC_LOCAL_FPU) / ELEM_SIZE;
    }

    // Returns the virtual address for the element with the given index. If no
    // writeable mapping for the given index exists, this function allocates an
    // empty page from the kernel heap and maps it.
    mword walk(size_t idx);

    // checks if a writeable mapping for the given virtual address exists. If
    // it does, this function does nothing. Otherwise it allocates an empty
    // page from the kernel heap and maps it so it backs vaddr.
    void allocate_if_necessary(mword vaddr);

    // This function checks if the element at the given address crosses a page
    // boundary and returns true if it does, otherwise returns false.
    inline bool touches_next_page(mword vaddr)
    {
        return (vaddr & ~PAGE_MASK) != ((vaddr + ELEM_SIZE - 1) & ~PAGE_MASK);
    }

public:
    // handles page faults in the fpu space. As those should never happen, this
    // function always dies.
    NORETURN static void page_fault(mword, mword);
};
