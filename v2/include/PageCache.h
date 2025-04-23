#pragma once
#include "Common.h"
#include <map>
#include <mutex>

namespace Kama_memoryPool
{

class PageCache
{
public:
    static const size_t PAGE_SIZE = 4096;  // 4K页大小

    // 单例模式
    static PageCache& getInstance()
    {
        static PageCache instance;
        return instance;
    }

    // 分配指定页数的span
    void* allocateSpan(size_t numPages);

    // 释放span
    void deallocateSpan(void* ptr, size_t numPages);

private:
    PageCache() = default;

    // 向系统申请内存
    void* systemAlloc(size_t numPages);

private:
    struct Span
    {
        void* pageAddr;    // 页起始地址
        size_t numPages;   // 页数
        Span* next;        // 链表指针
    };

    // 按页数管理空闲span， 不同页数对应不同span链表 
    std::map<size_t, Span*> freeSpans_;
    // 分配内存地址 (pageAddr) -> 链表指针 (Span*)
    std::map<void*, Span*> spanMap_;   // 管理已分配内存，用于回收
    
    std::mutex mutex_;
};

} // namespace memoryPool 