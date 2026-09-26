#include "core/conversation.h"


// Empty conversation: size() == 0, no allocation yet.
Conversation::Conversation()
{
    data_ = nullptr;
    size_ = 0;
    capacity_ = 0;
}

// Releases all owned Message storage. No effect if already empty
// (e.g. moved-from).
Conversation::~Conversation()
{
    delete[] data_;
}

// Deep copy: allocates its own buffer and copies every Message.
// this->begin() must differ from other.begin() afterward.
Conversation::Conversation(const Conversation& other)
{
    if (other.capacity_ == 0)
    {
        data_ = nullptr;
        size_ = 0;
        capacity_ = 0;
        return;
    }

    data_ = new Message[other.capacity_];
    try
    {
        for (std::size_t i = 0; i < other.size_; ++i)
        {
            data_[i] = other.data_[i];
        }
    }
    catch (...)
    {
        delete[] data_;
        throw;
    }

    size_ = other.size_;
    capacity_ = other.capacity_;
}

Conversation& Conversation::operator=(const Conversation& other)
{
    if (this == &other)
    {
        return *this;
    }

    if (other.capacity_ == 0)
    {
        delete[] data_;
        data_ = nullptr;
        size_ = 0;
        capacity_ = 0;
        return *this;
    }

    Message* new_data_ = new Message[other.capacity_];
    try {
        for (std::size_t i = 0; i < other.size_; ++i) {
            new_data_[i] = other.data_[i];
        }
    }
    catch (...)
    {
        delete[] new_data_;
        throw;
    }

    delete[] data_;
    data_ = new_data_;
    size_ = other.size_;
    capacity_ = other.capacity_;

    return *this;
}

// Steals other's buffer — no per-element copying. Afterward, other
// must be left valid and empty (safe to destroy or reassign).
Conversation::Conversation(Conversation&& other) noexcept
{
    data_ = other.data_;
    size_ = other.size_;
    capacity_ = other.capacity_;

    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
}

Conversation& Conversation::operator=(Conversation&& other) noexcept
{
    if (this == &other)
    {
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

// Appends m, growing the backing array if needed. Amortized O(1) —
// doubling capacity means the total copying cost across n appends
// stays a constant multiple of n (geometric series).
void Conversation::append(Message m)
{
    if (size_ == capacity_)
    {
        std::size_t new_capacity = (capacity_ == 0) ? 1 : capacity_ * 2;
        Message* new_data_ = new Message[new_capacity];
        try
        {
            for (std::size_t i = 0; i < size_; ++i)
            {
                new_data_[i] = std::move(data_[i]);
            }
        }
        catch (...)
        {
            delete[] new_data_;
            throw;
        }

        delete[] data_;
        data_ = new_data_;
        capacity_ = new_capacity;
    }

    data_[size_] = std::move(m);
    ++size_;
}

// Number of messages currently stored.
std::size_t Conversation::size() const noexcept
{
    return size_;
}

// Bounds-checked access. Throws std::out_of_range on i >= size().
const Message& Conversation::at(std::size_t i) const
{
    if (i >= size_)
    {
        throw std::out_of_range("Conversation::at: index out of range");
    }

    return data_[i];
}

// Range-for iteration, oldest message first. begin() == end() when
// size() == 0.
const Message* Conversation::begin() const noexcept
{
    return data_;
}

const Message* Conversation::end() const noexcept
{
    return data_ + size_;
}