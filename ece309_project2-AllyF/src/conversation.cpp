#include "core/conversation.h"


// Empty conversation: size() == 0, no allocation yet.
Conversation::Conversation() {

}

// Releases all owned Message storage. No effect if already empty
// (e.g. moved-from).
Conversation::~Conversation() {

}

// Deep copy: allocates its own buffer and copies every Message.
// this->begin() must differ from other.begin() afterward.
Conversation::Conversation(const Conversation& other) {

}

Conversation& Conversation::operator=(const Conversation& other) {

}

// Steals other's buffer — no per-element copying. Afterward, other
// must be left valid and empty (safe to destroy or reassign).
Conversation::Conversation(Conversation&& other) noexcept {

}

Conversation& Conversation::operator=(Conversation&& other) noexcept {

}

// Appends m, growing the backing array if needed. Amortized O(1) —
// document and justify your growth strategy in the design log
// (see Appendix C if you want a refresher first).
void Conversation::append(Message m) {

}

// Number of messages currently stored.
std::size_t Conversation::size() const noexcept {

}

// Bounds-checked access. Decide what happens on i >= size() (throw,
// assert, whatever you pick) and test that behavior explicitly.
const Message& Conversation::at(std::size_t i) const {

}

// Range-for iteration, oldest message first. begin() == end() when
// size() == 0.
const Message* Conversation::begin() const noexcept {

}

const Message* Conversation::end() const noexcept {

}