#include "Message.h"

namespace Elysium {
    void MessageQueue::Push(std::unique_ptr<Message> msg) {
        std::lock_guard<std::mutex> lock(mutex_);
        writeBuffer_->push_back(std::move(msg));
    }

    std::vector<std::unique_ptr<Message>> MessageQueue::Swap() {
        std::vector<std::unique_ptr<Message>> localBuffer;
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            // 1. Move the messages out of the current write buffer
            localBuffer = std::move(*writeBuffer_);
            
            // 2. Clear the current write buffer (move leaves it in valid but unspecified state)
            writeBuffer_->clear();
            
            // 3. Flip the pointer to the other internal buffer for the next frame
            writeBuffer_ = (writeBuffer_ == &bufferA_) ? &bufferB_ : &bufferA_;
        }
        
        return localBuffer;
    }
}