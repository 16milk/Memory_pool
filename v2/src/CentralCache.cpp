#include "../include/CentralCache.h"
#include "../include/PageCache.h"
#include <cassert>
#include <thread>

namespace Kama_memoryPool
{

// 每次从 PageCache 获取 span 大小（以页为单位）
static const size_t SPAN_PAGES = 8;

// 从中心缓存获取内存块
void* CentralCache::fetchRange(size_t index)
{
    // 申请内存过大，应直接向系统申请
    if (index >= FREE_LIST_SIZE)
        return nullptr;

    // 自旋锁保护
    while (locks_[index].test_and_set(std::memory_order_acquire))
    {
        // 添加线程让步，避免忙等待，避免过度消耗CPU
        std::this_thread::yield(); 
    }

    void* result = nullptr;
    try
    {
        // 尝试从中心缓存获取内存块
        result = centralFreeList_[index].load(std::memory_order_relaxed);

        if (!result)
        {
            // 如果中心缓存为空，从页缓存获取新的内存块
            size_t size = (index + 1) * ALIGNMENT;
            result = fetchFromPageCache(size);

            // 页缓存也获取失败，则释放锁并返回空指针
            if (!result)
            {
                locks_[index].clear(std::memory_order_release);
                return nullptr;
            }

            // 将获取的内存切分成小块
            char* start = static_cast<char*>(result);
            size_t blockNum = (SPAN_PAGES * PageCache::PAGE_SIZE) / size;

            if (blockNum > 1)
            {
                // 确保至少有两个块才构建链表
                for (size_t i = 1; i < blockNum; ++i)
                {
                    void* current = start + (i - 1) * size;
                    void* next = start + i * size;
                    *reinterpret_cast<void**>(current) = next;
                }
                *reinterpret_cast<void**>(start + (blockNum - 1) * size) = nullptr;

                // 保存result的下一个节点
                void* next = *reinterpret_cast<void**>(result);
                // 将result与链表断开
                *reinterpret_cast<void**>(result) = nullptr;
                // 更新中心缓存
                centralFreeList_[index].store(next, std::memory_order_release);
            }
        }
        else
        {
            // 直接从中心缓存中分配
            void* next = *reinterpret_cast<void**>(result);
            *reinterpret_cast<void**>(result) = nullptr;
            centralFreeList_[index].store(next, std::memory_order_release);
        }
    }
    // 确保在发生异常时锁能被正确释放
    catch (...)
    {
        locks_[index].clear(std::memory_order_release);
        throw;
    }

    // 释放锁
    locks_[index].clear(std::memory_order_release);
    return result;

}

// 将一段连续的内存块链表归还到中心缓存对应大小的自由列表中
void CentralCache::returnRange(void* start, size_t size, size_t index)
{
    // 当索引大于等于FREE_LIST_SIZE时，说明内存过大应直接向系统归还
    if (!start || index >= FREE_LIST_SIZE)
        return;
    while (locks_[index].test_and_set(std::memory_order_acquire))
    {
        std::this_thread::yield();
    }

    try
    {
        // 找到要归还的链表的最后一个节点
        void* end = start;
        size_t count = 1;
        while (*reinterpret_cast<void**>(end) != nullptr && count < size) {
            end = *reinterpret_cast<void**>(end);
            count ++;
        }

        // 将归还的链表连接到中心缓存的链表头部
        void* current = centralFreeList_[index].load(std::memory_order_relaxed);
        // 将原链表头接到归还链表的尾部
        *reinterpret_cast<void**>(end) = current;
        // 将归还的链表头设为新的链表头
        centralFreeList_[index].store(start, std::memory_order_release);
    }
    catch (...)
    {
        locks_[index].clear(std::memory_order_release);
        throw;
    }
    locks_[index].clear(std::memory_order_release);
}

void* CentralCache::fetchFromPageCache(size_t size)
{
    // 1. 计算实际需要的页数
    size_t numPages = (size + PageCache::PAGE_SIZE - 1) / PageCache::PAGE_SIZE;

    // 2. 根据大小决定分配策略
    if (size <= SPAN_PAGES * PageCache::PAGE_SIZE)
    {
        // 小于等于 32KB 的请求，使用固定的 8 页
        return PageCache::getInstance().allocateSpan(SPAN_PAGES); 
    }
    else
    {
        // 大于32KB的请求，按实际需求分配
        return PageCache::getInstance().allocateSpan(numPages);
    }
}

}