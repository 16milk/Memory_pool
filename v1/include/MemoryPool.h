#pragma once

#include <memory>
#include <cassert>
#include <mutex>

namespace memoryPool
{
#define MEMORY_POOL_NUM 64
#define SLOT_BASE_SIZE 8
#define MAX_SLOT_SIZE 512

struct Slot
{
    Slot* next;
};

class MemoryPool
{
public:
    MemoryPool(size_t BlockSize = 4096);
    ~MemoryPool();

    void init(size_t);

    void* allocate();
    void deallocate(void*);

private:
    void allocateNewBlock();
    size_t padPointer(char* p, size_t align);

private:
    int BlockSize_;      // 内存块大小
    int SlotSize_;       // 槽大小
    Slot* firstBlock_;   // 指向内存池管理的首个实际内存块
    Slot* curSlot_;      // 指向当前未被使用过的槽
    Slot* freeList_;     // 指向空闲的槽（被使用过后又被释放的槽）
    Slot* lastSlot_;     // 作为当前内存块中最后能够存放元素的位置标识
    std::mutex mutexForFreeList_;  // 保证freeList_在多线程中操作的原子性
    std::mutex mutexForBlock_;  // 保证多线程情况下避免不必要的
};

class HashBucket
{
public:
    static void initMemoryPool();
    static MemoryPool& getMemoryPool(int index);
    
    static void* useMemory(size_t size)
    {
        if (size <= 0) 
            return nullptr;
        if (size > MAX_SLOT_SIZE)
            return operator new(size);
        return getMemoryPool(((size + 7) / SLOT_BASE_SIZE) - 1).allocate();
    }

    static void freeMemory(void* ptr, size_t size)
    {
        if (!ptr)
            return;
        if (size > MAX_SLOT_SIZE)
        {
            operator delete(ptr);
            return;
        }
        getMemoryPool(((size + 7) / SLOT_BASE_SIZE) - 1).deallocate(ptr);
    }

    template<typename T, typename... Args>
    friend T* newElement(Args&&... args);

    template<typename T>
    friend void deleteElement(T* p);

};


template<typename T, typename... Args>
T* newElement(Args&&... args)
{
    // 从内存池中申请大小为sizeof(T)的内存块，返回的void*强制转换为T*
    T* p = reinterpret_cast<T*>(HashBucket::useMemory(sizeof(T)));
    // 如果分配成功，通过placement new在地址 p 上构造 T 类型的对象
    if (p != nullptr)
        new(p) T(std::forward<Args>(args)...);
    return p;
}

template<typename T>
void deleteElement(T* p)
{
    if (p)
    {
        // 显式调用析构函数，释放对象资源，但不释放内存
        p->~T();
        HashBucket::freeMemory(reinterpret_cast<void*>(p), sizeof(T));
    }
}

}
