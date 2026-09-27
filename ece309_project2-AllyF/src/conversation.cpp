#include "core/conversation.h"

#include <stdexcept>
#include <utility>

// Empty conversation: size() == 0, no allocation yet.
Conversation::Conversation() = default;

// Releases all owned Message storage. No effect if already empty
// (e.g. moved-from), since delete[] on nullptr is a no-op.
Conversation::~Conversation() {
    delete[] data_;
}

// Deep copy: allocates its own buffer sized to match other's capacity
// and copies every live Message into it.
Conversation::Conversation(const Conversation& other)
    : data_(other.capacity_ > 0 ? new Message[other.capacity_] : nullptr),
      size_(other.size_),
      capacity_(other.capacity_) {
    for (std::size_t i = 0; i < size_; ++i) {
        data_[i] = other.data_[i];
    }
}

Conversation& Conversation::operator=(const Conversation& other) {
    if (this == &other) {
        return *this;
    }

    // Allocate and populate the new buffer BEFORE touching our own state.
    // If `new` throws, *this is left completely untouched (strong
    // exception guarantee) instead of ending up half-destroyed.
    Message* new_data = other.capacity_ > 0 ? new Message[other.capacity_] : nullptr;
    for (std::size_t i = 0; i < other.size_; ++i) {
        new_data[i] = other.data_[i];
    }

    delete[] data_;
    data_ = new_data;
    size_ = other.size_;
    capacity_ = other.capacity_;
    return *this;
}

// Steals other's buffer — no per-element copying. Afterward, other
// is left valid and empty (nullptr/0/0), safe to destroy or reassign.
Conversation::Conversation(Conversation&& other) noexcept
    : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
}

Conversation& Conversation::operator=(Conversation&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    delete[] data_;

    data_ = other.data_;
    size_ = other.size_;
    capacity_ = other.capacity_;

    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
    return *this;
}

// Appends m, growing the backing array if needed.
//
// Growth factor: doubling (capacity 0 -> 1 -> 2 -> 4 -> 8 -> ...). See
// docs/design-log-p2.md for the amortized O(1) proof.
void Conversation::append(Message m) {
    if (size_ == capacity_) {
        std::size_t new_capacity = (capacity_ == 0) ? 1 : capacity_ * 2;
        Message* new_data = new Message[new_capacity];
        for (std::size_t i = 0; i < size_; ++i) {
            new_data[i] = std::move(data_[i]);
        }
        delete[] data_;
        data_ = new_data;
        capacity_ = new_capacity;
    }

    data_[size_] = std::move(m);
    ++size_;
}

// Number of messages currently stored.
std::size_t Conversation::size() const noexcept {
    return size_;
}

// Bounds-checked access: throws std::out_of_range on i >= size(), so a
// caller mistake surfaces immediately instead of reading garbage/UB.
const Message& Conversation::at(std::size_t i) const {
    if (i >= size_) {
        throw std::out_of_range("Conversation::at: index out of range");
    }
    return data_[i];
}

// Range-for iteration, oldest message first. When size_ == 0, data_ is
// either nullptr (never allocated) or a valid empty-in-use buffer; either
// way begin() == end() so no element is visited.
const Message* Conversation::begin() const noexcept {
    return data_;
}

const Message* Conversation::end() const noexcept {
    return data_ + size_;
}
