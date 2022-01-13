#include "fpu_allocator.hpp"

#include "buddy.hpp"
#include "hpt.hpp"
#include "lock_guard.hpp"
#include "space_mem.hpp"

void* Fpu_allocator::alloc()
{
    Lock_guard<Spinlock> guard(lock_);

    if (free_chunks_ > 0) {
        const long int free_idx { bit_scan_forward(free_chunks_) };
        free_chunks_ = free_chunks_ & ~(1 << free_idx);
        return reinterpret_cast<void*>(walk(free_idx));
    }

    const size_t free_idx { last_chunk_++ };
    return reinterpret_cast<void*>(walk(free_idx));
}

void Fpu_allocator::free(void* vaddr)
{
    const size_t idx { virt_to_idx(reinterpret_cast<mword>(vaddr)) };

    Lock_guard<Spinlock> guard(lock_);
    free_chunks_ = free_chunks_ | (1 << idx);
}

mword Fpu_allocator::walk(size_t idx)
{
    const mword vaddr { idx_to_virt(idx) };
    allocate_if_necessary(vaddr);

    if (touches_next_page(vaddr)) {
        allocate_if_necessary(idx_to_virt(idx+1));
    }

    return vaddr;
}

void Fpu_allocator::allocate_if_necessary(mword vaddr)
{
    const Paddr frame_0 = Buddy::ptr_to_phys(&PAGE_0);
    Paddr paddr;

    if (space_mem_->lookup(vaddr, &paddr) && (paddr & ~PAGE_MASK) != frame_0) {
        // if a mapping for vaddr is present and the mapping does not
        // point to the zero page, there is nothing to do
        return;
    }

    void* empty_page_vaddr { Buddy::allocator.alloc(0, Buddy::FILL_0) };
    Paddr empty_page_paddr { Buddy::ptr_to_phys(empty_page_vaddr)     };
    space_mem_->replace(vaddr,
            empty_page_paddr | Hpt::PTE_NX | Hpt::PTE_D | Hpt::PTE_A
                             | Hpt::PTE_W | Hpt::PTE_P);
}
