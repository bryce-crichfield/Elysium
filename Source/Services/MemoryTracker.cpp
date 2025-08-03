#include "Services/MemoryTracker.h"
#include <algorithm>

namespace Elysium::Services {

MemoryTracker& MemoryTracker::GetInstance() {
    static MemoryTracker instance;
    return instance;
}

void MemoryTracker::RecordAllocation(size_t size) {
    totalAllocated_.fetch_add(size);
    currentAllocated_.fetch_add(size);
    allocationCount_.fetch_add(1);
    
    // Update peak allocated
    size_t current = currentAllocated_.load();
    size_t peak = peakAllocated_.load();
    while (current > peak && !peakAllocated_.compare_exchange_weak(peak, current)) {
        // Retry if another thread updated peak
    }
}

void MemoryTracker::RecordDeallocation(size_t size) {
    totalDeallocated_.fetch_add(size);
    currentAllocated_.fetch_sub(size);
    deallocationCount_.fetch_add(1);
}

void MemoryTracker::Reset() {
    totalAllocated_.store(0);
    totalDeallocated_.store(0);
    currentAllocated_.store(0);
    allocationCount_.store(0);
    deallocationCount_.store(0);
    peakAllocated_.store(0);
}

} // namespace Elysium::Services