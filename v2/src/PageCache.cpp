#include "PageCache.h"
#include <sys/mman.h>
#include <cstring>

namespace Kama_memoryPool
{

void PageCache::systemAlloc(size_t numPages)
{
    size_t size = numPages * PAGE_SIZE;

    // 使用 mmap 分配内存
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (ptr == MAP_FAILED) return nullptr;

    // 清零内存
    memset(ptr, 0, size);
    return ptr;
}

}