#pragma once

#include <atomic>
#include <memory>
#include <cstddef>
#include <cstdlib>
#include <new>

namespace Elysium::Services {

class MemoryTracker {
public:
    static MemoryTracker& GetInstance();
    
    void RecordAllocation(size_t size);
    void RecordDeallocation(size_t size);
    
    size_t GetTotalAllocated() const { return totalAllocated_.load(); }
    size_t GetTotalDeallocated() const { return totalDeallocated_.load(); }
    size_t GetCurrentAllocated() const { return currentAllocated_.load(); }
    size_t GetAllocationCount() const { return allocationCount_.load(); }
    size_t GetDeallocationCount() const { return deallocationCount_.load(); }
    size_t GetPeakAllocated() const { return peakAllocated_.load(); }
    
    void Reset();
    
private:
    MemoryTracker() = default;
    ~MemoryTracker() = default;
    MemoryTracker(const MemoryTracker&) = delete;
    MemoryTracker& operator=(const MemoryTracker&) = delete;
    
    std::atomic<size_t> totalAllocated_{0};
    std::atomic<size_t> totalDeallocated_{0};
    std::atomic<size_t> currentAllocated_{0};
    std::atomic<size_t> allocationCount_{0};
    std::atomic<size_t> deallocationCount_{0};
    std::atomic<size_t> peakAllocated_{0};
};

// Custom allocator that tracks allocations
template<typename T>
class TrackedAllocator {
public:
    using value_type = T;
    
    TrackedAllocator() = default;
    template<typename U>
    TrackedAllocator(const TrackedAllocator<U>&) {}
    
    T* allocate(size_t n) {
        size_t size = n * sizeof(T);
        T* ptr = static_cast<T*>(malloc(size));
        if (!ptr) {
            throw std::bad_alloc();
        }
        MemoryTracker::GetInstance().RecordAllocation(size);
        return ptr;
    }
    
    void deallocate(T* ptr, size_t n) {
        if (ptr) {
            size_t size = n * sizeof(T);
            MemoryTracker::GetInstance().RecordDeallocation(size);
            free(ptr);
        }
    }
    
    template<typename U>
    bool operator==(const TrackedAllocator<U>&) const { return true; }
    
    template<typename U>
    bool operator!=(const TrackedAllocator<U>&) const { return false; }
};

// Helper macros for tracked allocations
#define TRACKED_MALLOC(size) ([](size_t s) { \
    void* ptr = malloc(s); \
    if (ptr) Elysium::Services::MemoryTracker::GetInstance().RecordAllocation(s); \
    return ptr; \
})(size)

#define TRACKED_FREE(ptr, size) do { \
    if (ptr) { \
        Elysium::Services::MemoryTracker::GetInstance().RecordDeallocation(size); \
        free(ptr); \
    } \
} while(0)

} // namespace Elysium::Services