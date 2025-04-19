#include "../include/MemoryPool.h"

namespace memoryPool
{
MemoryPool::MemoryPool(size_t BlockSize)
    : BlockSize_(BlockSize), SlotSize_(0), firstBlock_(nullptr),
      curSlot_(nullptr), freeList_(nullptr), lastSlot_(nullptr)
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
    freeList_.store(nullptr, std::memory_order_relaxed);
    lastSlot_ = nullptr;
}

void* MemoryPool::allocate()
{
    // 优先使用空闲链表中的内存槽
    Slot* slot = popFreeList();
    if (slot != nullptr) {
        return slot;
    }

    // 如果空闲链表为空，则分配新的内存
    std::lock_guard<std::mutex> lock(mutexForBlock_);
    if (curSlot_ >= lastSlot_) {
        allocateNewBlock();
    }

    Slot* result = curSlot_;
    curSlot_ = reinterpret_cast<Slot*>(reinterpret_cast<char*>(curSlot_) + SlotSize_);
    return result;
}

void MemoryPool::deallocate(void* ptr)
{
    if (!ptr) return;
    // static_cast适用于已知 ptr 原本就是 Slot* 类型的情况
    // 更安全，因为编译器会检查类型兼容性
    Slot* slot = static_cast<Slot*>(ptr);
    pushFreeList(slot);
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

// 实现无锁入队操作
bool MemoryPool::pushFreeList(Slot* slot) {
    while (true) {
        // 获取当前头节点
        Slot* oldHead = freeList_.load(std::memory_order_relaxed);
        // 将新节点的 next 指向当前头节点
        slot->next.store(oldHead, std::memory_order_relaxed);

        // 尝试将新节点设置为头节点
        if (freeList_.compare_exchange_weak(oldHead, slot, 
                                        std::memory_order_release,
                                        std::memory_order_relaxed)) {
            return true;
        }
        // CAS 失败则重试
    }
}

// 实现无锁出队操作
Slot* MemoryPool::popFreeList() {
    while (true) {
        Slot* oldHead = freeList_.load(std::memory_order_relaxed);
        if (oldHead == nullptr) {
            return nullptr; // 队列为空
        }
        // 获取下一个节点
        Slot* newHead = oldHead->next.load(std::memory_order_relaxed);
        // 尝试更新头节点
        if (freeList_.compare_exchange_weak(oldHead, newHead,
                                        std::memory_order_acquire,
                                        std::memory_order_relaxed)) {
            return oldHead;
        }
        // CAS 失败则重试
    }
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