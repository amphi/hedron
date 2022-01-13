#include "static_allocator.hpp"

#include "assert.hpp"
#include "initprio.hpp"
#include "lock_guard.hpp"
#include "string.hpp"

extern char _mempool_e;

INIT_PRIORITY (PRIO_BUDDY)
Static_allocator Static_allocator::allocator(
        (reinterpret_cast<mword>(&_mempool_e) + PAGE_SIZE) & ~PAGE_MASK) ;

void* Static_allocator::alloc()
{
    Lock_guard<Spinlock> guard(lock_);

    const mword mem { index_to_page(last_idx_) };
    last_idx_++;

    return reinterpret_cast<void*>(mem);
}
