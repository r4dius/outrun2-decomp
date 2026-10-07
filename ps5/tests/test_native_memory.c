#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

void *__wrap_mmap(void *, size_t, int, int, int, off_t);
int __wrap_munmap(void *, size_t);
// A host mock of native libkernel: only the synthetic GPU mapping is valid.
static void *const gpu_mapping = (void *)(uintptr_t)0x100000000ULL;
static unsigned gpu_releases;
static const void *released_staging;
int or2_ps5_native_heap_contains(const void *address) {
    return address == released_staging;
}
int __real_munmap(void *address, size_t length) {
    if (address == gpu_mapping && length == 16384) { ++gpu_releases; return 0; }
    errno = EINVAL;
    return -1;
}
#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "native mapping check failed: %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    abort(); } } while (0)
static void *map(size_t size) {
    return __wrap_mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
}
static void *worker(void *argument) {
    const unsigned char pattern = (unsigned char)(uintptr_t)argument;
    for (size_t n = 0; n < 200; ++n) {
        const size_t length = 65536 + n * 79;
        unsigned char *p = map(length);
        CHECK(p != MAP_FAILED && (uintptr_t)p % 16384 == 0);
        for (size_t i = 0; i < length; ++i) CHECK(p[i] == 0);
        memset(p, pattern, length);
        CHECK(__wrap_munmap(p, length) == 0);
    }
    return NULL;
}
int main(void) {
    CHECK(map(0) == MAP_FAILED && errno == EINVAL);
    CHECK(map(SIZE_MAX) == MAP_FAILED && errno == ENOMEM);
    CHECK(__wrap_mmap(NULL, 4096, PROT_READ, MAP_PRIVATE | MAP_ANON, -1, 0) == MAP_FAILED && errno == ENOTSUP);
    CHECK(__wrap_mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE, 7, 0) == MAP_FAILED && errno == ENOTSUP);
    CHECK(__wrap_mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0) == MAP_FAILED && errno == ENOTSUP);
    CHECK(__wrap_mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 4096) == MAP_FAILED && errno == EINVAL);
    CHECK(__wrap_mmap((void *)16384, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0) == MAP_FAILED && errno == ENOTSUP);
    CHECK(__wrap_munmap((void *)16384, 4096) == -1 && errno == EINVAL);
    CHECK(__wrap_munmap(gpu_mapping, 16384) == 0 && gpu_releases == 1);
    // Match Gallium's full setup-image staging upload (1920 x 1080 RGBA).
    const size_t size = 1920u * 1080u * 4u;
    unsigned char *p = map(size);
    CHECK(p != MAP_FAILED);
    CHECK(__wrap_munmap(p, size - 1) == -1 && errno == EINVAL);
    CHECK(__wrap_munmap(p + 16384, 16384) == -1 && errno == EINVAL);
    CHECK(__wrap_munmap(p - 16384, 16384) == -1 && errno == EINVAL);
    memset(p, 0x5a, size);
    CHECK(p[size - 1] == 0x5a);
    CHECK(__wrap_munmap(p, size) == 0);
    released_staging = p;
    CHECK(__wrap_munmap(p, size) == -1 && errno == EINVAL);
    pthread_t threads[8];
    for (uintptr_t i = 0; i < 8; ++i) CHECK(pthread_create(&threads[i], NULL, worker, (void *)(i + 1)) == 0);
    for (size_t i = 0; i < 8; ++i) CHECK(pthread_join(threads[i], NULL) == 0);
    puts("PS5 native anonymous mappings: upload, boundaries and concurrent lifetimes passed");
    return 0;
}
