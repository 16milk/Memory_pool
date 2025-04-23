# Memory_pool
C++ 内存池


# PageCache内存回收中的合并空闲内存逻辑分析

这段代码实现了PageCache中内存回收时合并相邻空闲内存块的功能。下面我将详细解释这个逻辑：

## 基本流程

1. **锁定互斥锁**：使用`std::lock_guard`确保线程安全
2. **查找对应的Span**：通过`spanMap_`查找要释放的内存块对应的Span对象
3. **尝试合并相邻的Span**：主要关注后面的相邻Span
4. **将Span插入空闲列表**：使用头插法将Span插入对应大小的空闲链表

## 合并相邻空闲内存的核心逻辑

合并操作主要检查并处理后面相邻的内存块：

1. **查找相邻Span**：
   ```cpp
   void* nextAddr = static_cast<char*>(ptr) + numPages * PAGE_SIZE;
   auto nextIt = spanMap_.find(nextAddr);
   ```

2. **如果找到相邻Span**：
   - 检查该相邻Span是否在空闲链表中
   - 从空闲链表中移除该Span
   - 合并两个Span

3. **从空闲链表中移除相邻Span**：
   - 首先检查是否是链表头节点
   - 如果不是头节点则遍历链表查找
   - 使用`found`标志记录是否成功找到并移除

4. **合并Span**：
   ```cpp
   span->numPages += nextSpan->numPages;  // 合并页数
   spanMap_.erase(nextAddr);  // 从映射中移除相邻Span
   delete nextSpan;  // 删除Span对象
   ```

## 将Span重新插入空闲列表

无论是否合并成功，最后都会将Span插入对应大小的空闲链表：
```cpp
auto& list = freeSpans_[span->numPages];
span->next = list;
list = span;
```

## 特点与注意事项

1. **只合并后面的相邻Span**：这段代码只处理了后面的相邻Span，实际实现中通常也会检查前面的相邻Span

2. **线程安全**：通过互斥锁保护所有操作

3. **高效查找**：使用`spanMap_`哈希表快速定位Span

4. **空闲链表管理**：`freeSpans_`是按Span大小组织的空闲链表数组

5. **头插法**：插入空闲链表时使用头插法提高效率

这种合并机制可以有效减少内存碎片，将小的空闲内存块合并成大的连续内存块，提高内存利用率。
