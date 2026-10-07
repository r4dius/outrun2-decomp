// CPU-only anonymous mappings for the native PS5 title. Firmware 13.60 rejects
// the payload SDK's mmap syscall. Gallium uses these mappings for texture upload
// staging; the title's wrapped allocator already owns CPU-cached direct memory.
#include <errno.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

// Unlike the payload SDK's mmap implementation, munmap is imported from the
// native libkernel. GPU/direct-memory mappings must retain that release path.
int __real_munmap(void *address, size_t length);
int or2_ps5_native_heap_contains(const void *address);

enum { native_page_size = 16384 };
struct native_mapping {
    struct native_mapping *next;
    void *address;
    size_t length;
    size_t capacity;
};
static struct native_mapping *native_mappings;
static atomic_flag native_mappings_lock = ATOMIC_FLAG_INIT;

static void lock_mappings(void) {
    while (atomic_flag_test_and_set_explicit(&native_mappings_lock, memory_order_acquire)) {}
}
static void unlock_mappings(void) {
    atomic_flag_clear_explicit(&native_mappings_lock, memory_order_release);
}

// Deliberately supports only the complete, private, read/write mappings needed
// by Gallium. Unsupported file/shared/executable/fixed mappings fail normally;
// never forward them to the forbidden raw syscall.
void *__wrap_mmap(void *address, size_t length, int protection, int flags,
                  int fd, off_t offset) {
    if (!length || offset) { errno = EINVAL; return MAP_FAILED; }
    if (address || fd != -1 || protection != (PROT_READ | PROT_WRITE) ||
        flags != (MAP_PRIVATE | MAP_ANON)) {
        errno = ENOTSUP;
        return MAP_FAILED;
    }
    if (length > SIZE_MAX - (2u * native_page_size - 1u)) {
        errno = ENOMEM;
        return MAP_FAILED;
    }
    const size_t capacity = (length + native_page_size - 1u) & ~(size_t)(native_page_size - 1u);
    void *allocation = NULL;
    const int rc = posix_memalign(&allocation, native_page_size, capacity + native_page_size);
    if (rc) { errno = rc; return MAP_FAILED; }
    struct native_mapping *mapping = allocation;
    mapping->address = (char *)allocation + native_page_size;
    mapping->length = length;
    mapping->capacity = capacity;
    memset(mapping->address, 0, capacity);
    lock_mappings();
    mapping->next = native_mappings;
    native_mappings = mapping;
    unlock_mappings();
    return mapping->address;
}

int __wrap_munmap(void *address, size_t length) {
    lock_mappings();
    struct native_mapping **link = &native_mappings;
    while (*link && (*link)->address != address) {
        const uintptr_t candidate = (uintptr_t)address;
        const uintptr_t base = (uintptr_t)*link;
        // Never let a partial unmap remove pages from the allocator's heap.
        if (candidate >= base && candidate - base < (*link)->capacity + native_page_size) {
            unlock_mappings();
            errno = EINVAL;
            return -1;
        }
        link = &(*link)->next;
    }
    struct native_mapping *mapping = *link;
    if (!mapping) {
        unlock_mappings();
        // A released staging allocation still belongs to the mspace's backing
        // mapping. A double unmap must not punch a hole in that shared heap.
        if (or2_ps5_native_heap_contains(address)) {
            errno = EINVAL;
            return -1;
        }
        return __real_munmap(address, length);
    }
    if (mapping->length != length) {
        unlock_mappings();
        errno = EINVAL;
        return -1;
    }
    *link = mapping->next;
    unlock_mappings();
    free(mapping);
    return 0;
}
