// include/core/sentinel_scanner.h
#pragma once

#include <cstddef>
#include <string>
#include <string_view>

// Incrementally scans a stream of arbitrary-sized chunks for a fixed
// sentinel string, without ever holding the full reply in memory and
// without re-scanning text it has already cleared.
//
// Invariant: pending_.size() is always <= sentinel_.size() - 1 whenever
// feed() returns (see docs/design-log-p2.md for the proof). Those bytes
// are the only ones that might still turn into the start of a sentinel
// once more input arrives, so anything older is safe to emit immediately.
class SentinelScanner {
public:
    explicit SentinelScanner(std::string sentinel);

    struct Out {
        std::string safe_text;
        bool sentinel_found;
    };

    // Feed the next chunk. Returns text guaranteed NOT to be part of the
    // sentinel (safe to print immediately) and whether the sentinel has
    // now been fully seen. Once sentinel_found has been reported once,
    // further feed() calls are no-ops (empty safe_text, sentinel_found
    // still true) — the reply is considered over.
    Out feed(std::string_view chunk);

    // Call once, after the stream ends, to release any text still being
    // held back in pending_ (it turned out not to be a sentinel after all).
    Out flush();

    // Test hook, not part of the spec's required interface: exposes the
    // current size of the held-back buffer so tests can assert the
    // bounded-memory invariant without needing to befriend the test file.
    std::size_t pending_size() const noexcept { return pending_.size(); }

private:
    std::string sentinel_;
    std::string pending_;   // holds back at most sentinel_.size() - 1
                             // trailing characters that could still
                             // become the start of the sentinel
    bool found_ = false;
};
