#pragma once
#include <utility> //std::swap std::forward
#include <cstdint>
#include <cstddef>

template <typename T, size_t BlockSize = 4096>
class MemoryPool
{
public:
    template <typename U>
    struct rebind
    {
        typedef MemoryPool<U> other;
    };
    // Construct function
    MemoryPool() noexcept;
    // Copy construct function
    MemoryPool(const MemoryPool &other) noexcept;
    // Move construct function
    MemoryPool(MemoryPool &&other) noexcept;
    // template copy construct function
    template <class _T>
    MemoryPool(const MemoryPool<_T> &other) noexcept;

    ~MemoryPool() noexcept;

    //-----assignment operator-----//
    MemoryPool &operator=(const MemoryPool &other) = delete;
    MemoryPool &operator=(MemoryPool &&other) noexcept;
    // We can only use '=' when we need to assign a new value to the object

    //-----FIND ADDRESS-----//
    inline T *address(T &element) const noexcept
    {
        return &element;
    }
    inline const T *address(const T &element) const noexcept
    {
        return &element;
    }

    //-----ALLOCATE-----//
    inline T *allocate(size_t n = 1, const T *hint = nullptr)
    {
        if (freeSlots_ != nullptr)
        {
            T *result = reinterpret_cast<T *>(freeSlots_); //
            freeSlots_ = freeSlots_->next;
            return result;
        }
        else
        {
            if (currentSlot_ >= lastSlot_)
            {
                allocateBlock();
            }
            return reinterpret_cast<T *>(currentSlot_++);
        }
    }
    //-----DEALLOCATE-----//
    inline void deallocate(T *p, size_t = 1)
    {
        if (p != nullptr)
        {
            // insert into free list's head
            reinterpret_cast<Slot_ *>(p)->next = freeSlots_;
            freeSlots_ = reinterpret_cast<Slot_ *>(p);
        }
    }

    // construct and destroy single object
    template <typename U, typename... Args>
    inline void construct(U *p, Args &&...args)
    {
        new (p) U(std::forward<Args>(args)...);
    }
    template <typename U>
    inline void destroy(U *p)
    {
        p->~U();
    }
    // construct and destroy array object
    template <typename... Args>
    inline T *newElement(Args &&...args)
    {
        T *p = allocate();
        construct(p, std::forward<Args>(args)...);
        return p;
    }
    inline void deleteElement(T *p)
    {
        if (p != nullptr)
        {
            p->~T();
            deallocate(p);
        }
    }
    inline size_t max_size() const noexcept
    {
        // The maximum number of elements that can be stored
        size_t max_block = size_t(-1) / BlockSize;
        return (BlockSize - sizeof(char *)) / sizeof(Slot_) * max_block;
    }

private:
    union Slot_
    {
        T element;
        Slot_ *next;
    };
    Slot_ *currentBlock_;
    Slot_ *currentSlot_;
    Slot_ *lastSlot_;
    Slot_ *freeSlots_;

    // Allocate a new block of memory
    inline size_t padPointer(char *p, size_t align)
    {
        uintptr_t addr = reinterpret_cast<uintptr_t>(p);
        size_t padding = (align - addr) % align;
        return padding;
    }
    void allocateBlock();

    // assert to avoid memory leak
    static_assert(BlockSize >= 2 * sizeof(Slot_), "BlockSize too small.");
};

// basic construct function
template <typename T, size_t BlockSize>
MemoryPool<T, BlockSize>::MemoryPool() noexcept
    : currentBlock_(nullptr), currentSlot_(nullptr), lastSlot_(nullptr), freeSlots_(nullptr) {}

template <typename T, size_t BlockSize>
MemoryPool<T, BlockSize>::MemoryPool(const MemoryPool &other) noexcept
    : MemoryPool() {}

template <typename T, size_t BlockSize>
MemoryPool<T, BlockSize>::MemoryPool(MemoryPool &&other) noexcept
    : MemoryPool()
{
    swap(currentBlock_, other.currentBlock_);
    other.currentBlock_ = nullptr;
    swap(currentSlot_, other.currentSlot_);
    swap(lastSlot_, other.lastSlot_);
    swap(freeSlots_, other.freeSlots_);
}
template <typename T, size_t BlockSize>
template <class _T>
MemoryPool<T, BlockSize>::MemoryPool(const MemoryPool<_T> &other) noexcept
    : MemoryPool() {}

// distroy function
template <typename T, size_t BlockSize>
MemoryPool<T, BlockSize>::~MemoryPool() noexcept
{
    Slot_ *curr = currentBlock_;
    while (curr != nullptr)
    {
        Slot_ *next = curr->next;
        ::operator delete(reinterpret_cast<void *>(curr));
        curr = next;
    }
}
// assignment operator
template <typename T, size_t BlockSize>
MemoryPool<T, BlockSize> &MemoryPool<T, BlockSize>::operator=(MemoryPool &&other) noexcept
{
    if (this != &other)
    {
        std::swap(currentBlock_, other.currentBlock_);
        currentSlot_ = other.currentSlot_;
        lastSlot_ = other.lastSlot_;
        freeSlots_ = other.freeSlots_;
    }
    return *this;
}

// allocate a new block of memory
template <typename T, size_t BlockSize>
void MemoryPool<T, BlockSize>::allocateBlock()
{
    char *newBlock = reinterpret_cast<char *>(::operator new(BlockSize));
    reinterpret_cast<Slot_ *>(newBlock)->next = currentBlock_;
    currentBlock_ = reinterpret_cast<Slot_ *>(newBlock);

    // we need to align the memory and leave space for the header
    // after the header, we can use the memory to store slots
    char *body = newBlock + sizeof(Slot_ *);
    size_t bodyPadding = padPointer(body, alignof(Slot_));
    currentSlot_ = reinterpret_cast<Slot_ *>(body + bodyPadding);

    lastSlot_ = reinterpret_cast<Slot_ *>(newBlock + BlockSize - sizeof(Slot_) + 1);
}
