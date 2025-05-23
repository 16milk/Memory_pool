#pragma once
#include "Common.h"
#include <mutex>

namespace Kama_memoryPool
{
class CentralCache
{
public:
    // 单例模式
    static CentralCache& getInstance()
    {
        static CentralCache instance;
        return instance;
    }
    // 从中心缓存获取内存块
    void* fetchRange(size_t index);
    // 将一段连续的内存块链表归还到中心缓存对应大小的自由列表中
    void returnRange(void* start, size_t size, size_t bytes);

private:
    // 所有原子指针为 nullptr
    CentralCache()
    {
        for (auto& ptr : centralFreeList_)
        {
            ptr.store(nullptr, std::memory_order_relaxed);
        }
        // 初始化所有锁
        for (auto& lock : locks_)
        {
            // 释放锁，设为 false
            lock.clear();
        }
    }

    // 从页缓存获取内存
    void* fetchFromPageCache(size_t size);

private:
    // 中心缓存的自由链表
    std::array<std::atomic<void*>, FREE_LIST_SIZE> centralFreeList_;
    
    // 用于同步的自旋锁
    std::array<std::atomic_flag, FREE_LIST_SIZE> locks_; 
};

}