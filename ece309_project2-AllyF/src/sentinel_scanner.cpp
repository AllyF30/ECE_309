#include "core/sentinel_scanner.h"

SentinelScanner::SentinelScanner(std::string sentinel)
    : sentinel_(std::move(sentinel)) {}

SentinelScanner::Out SentinelScanner::feed(std::string_view chunk) {
    if (found_) {
        // The reply is already over; discard anything streamed after the
        // sentinel instead of re-triggering or re-emitting text.
        return {std::string(), true};
    }

    pending_.append(chunk.data(), chunk.size());

    std::size_t pos = pending_.find(sentinel_);
    if (pos != std::string::npos) {
        std::string safe = pending_.substr(0, pos);
        pending_.clear();
        found_ = true;
        return {std::move(safe), true};
    }

    // No full match yet. sentinel_.size() - 1 trailing characters could
    // still be the start of a sentinel once more text arrives, so they
    // must stay in pending_; everything before that is safe to emit now.
    const std::size_t keep = sentinel_.size() - 1;
    if (pending_.size() <= keep) {
        return {std::string(), false};
    }

    const std::size_t emit_len = pending_.size() - keep;
    std::string safe = pending_.substr(0, emit_len);
    pending_.erase(0, emit_len);
    return {std::move(safe), false};
}

SentinelScanner::Out SentinelScanner::flush() {
    std::string remaining = std::move(pending_);
    pending_.clear();
    // If the sentinel was already found, there's nothing left to flush:
    // stored pending_ was cleared at that point.
    return {std::move(remaining), false};
}
