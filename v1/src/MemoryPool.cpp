#include "../include/MemoryPool.h"

namespace memoryPool
{
MemoryPool::MemoryPool(size_t BlockSize)
    : BlockSize_(BlockSize)
{}

MemoryPool::~MemoryPool()
{
    // 删除连续的Block
    Slot* cur = firstBlock_;
    while (cur)
    {
        Slot* next = cur->next;
        operator delete(reinterpret_cast<void*>(cur));
        cur = next;
    }
}

void MemoryPool::init(size_t size)
{
    assert(size > 0);
    SlotSize_ = size;
    firstBlock_ = nullptr;
    curSlot_ = nullptr;
    freeList_ = nullptr;
    lastSlot_ = nullptr;
}

void* MemoryPool::allocate()
{
    // 优先使用空闲链表中的内存槽
    if (freeList_ != nullptr)
    {
        std::lock_guard<std::mutex> lock(mutexForFreeList_);
        if (freeList_ != nullptr)
        {
            Slot* temp = freeList_;
            freeList_ = freeList_->next;
            return temp;
        }
    }
    Slot* temp;
    std::lock_guard<std::mutex> lock(mutexForBlock_);
    if (curSlot_ >= lastSlot_)
    {
        // 无Slot可用， 开辟一个新的Block
        allocateNewBlock();
    }
    temp = curSlot_;
    // curSlot_是Slot*类型, 槽间都是连续的，没有指针，只有freeList中才有指针
    curSlot_ += SlotSize_ / sizeof(Slot);

    return temp;
}

void MemoryPool::deallocate(void* ptr)
{
    if (ptr)
    {
        std::lock_guard<std::mutex> lock(mutexForFreeList_);
        // 将void* 类型装换成Slot*，变成结点头插入
        reinterpret_cast<Slot*>(ptr)->next = freeList_;
        freeList_ = reinterpret_cast<Slot*>(ptr);
    }
}

void MemoryPool::allocateNewBlock()
{
    // 新申请的内存块要头插
    void* newBlock = operator new(BlockSize_);
    reinterpret_cast<Slot*>(newBlock)->next = firstBlock_;
    firstBlock_ = reinterpret_cast<Slot*>(newBlock);

    char* body = reinterpret_cast<char*>(newBlock) + sizeof(Slot*);
    // 计算对齐需要填充内存的大小
    size_t paddingSize = padPointer(body, SlotSize_);
    curSlot_ = reinterpret_cast<Slot*>(body + paddingSize);

    // lastSlot_是指向当前内存块中最后能储存槽的标志，等于或大于这个位置就代表
    // 当前内存块不足构成一个槽位
    lastSlot_ = reinterpret_cast<Slot*>(reinterpret_cast<size_t>(newBlock) + BlockSize_ - SlotSize_ + 1);

    freeList_ = nullptr;
}

size_t MemoryPool::padPointer(char* p, size_t align)
{
    // align是槽大小
    return (align - reinterpret_cast<size_t>(p)) % align;
}

void HashBucket::initMemoryPool()
{
    for (int i = 0; i < MEMORY_POOL_NUM; i ++) {
        getMemoryPool(i).init((i + 1) * SLOT_BASE_SIZE);
    }
}

// 单例模式
MemoryPool& HashBucket::getMemoryPool(int index)
{
    // 静态
    static MemoryPool memoryPool[MEMORY_POOL_NUM];
    return memoryPool[index];
}

}