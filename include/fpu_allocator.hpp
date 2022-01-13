#pragma once

/**
 * FPU allocator 
 * 
 * Allocates pd-local memory for the FPU.
 */

// it should be relatively easy to turn this into a more generic allocator later
class Fpu_allocator 
{
public:
    Fpu_allocator () = default;

    // allocates memory from the fpu-space and returns a pointer to the
    // allocated memory
    void* alloc();

    // marks the memory at the given address as free
    void free(void* vaddr);
};
