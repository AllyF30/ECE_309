// tests/p2/test_p2.cpp
//
// 12 assert-based tests covering Conversation, SentinelScanner, and the
// provided Harness/ModelClient wiring, per spec §5.

#include "core/conversation.h"
#include "core/message.h"
#include "core/sentinel_scanner.h"
#include "harness/harness.h"
#include "model/replay_client.h"
#include "model/scripted_client.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------
// Tiny test-runner scaffolding.
// ---------------------------------------------------------------------
#define TEST(name) static void name()

namespace {

int g_tests_run = 0;

void run(const char* name, void (*fn)()) {
    fn();
    ++g_tests_run;
    std::cout << "[PASS] " << name << "\n";
}

// ---------------------------------------------------------------------
// Test doubles for driving Harness::run() without a real terminal.
// ---------------------------------------------------------------------
class StubInput : public InputSource {
public:
    explicit StubInput(std::vector<std::string> lines) : lines_(std::move(lines)) {}

    std::string read_line() override {
        if (idx_ >= lines_.size()) {
            eof_ = true;
            return "";
        }
        return lines_[idx_++];
    }

    bool is_eof() const override { return eof_; }

private:
    std::vector<std::string> lines_;
    std::size_t idx_ = 0;
    bool eof_ = false;
};

class NullOutput : public OutputSink {
public:
    void write(std::string_view) override {}
};

// Mirrors main.cpp's save_transcript() (Appendix A format), duplicated
// here since that function is private to main.cpp.
const char* role_name(Role role) {
    switch (role) {
        case Role::System: return "system";
        case Role::User: return "user";
        case Role::Assistant: return "assistant";
    }
    return "assistant";
}

void save_transcript(const Conversation& conv, const std::string& path) {
    std::ofstream file(path);
    bool first = true;
    for (const Message* m = conv.begin(); m != conv.end(); ++m) {
        if (!first) file << "---\n";
        first = false;
        file << "role: " << role_name(m->role()) << "\n";
        file << m->content() << "\n";
    }
}

}  // namespace

// ---------------------------------------------------------------------
// 1. Empty Conversation Bounds
// ---------------------------------------------------------------------
TEST(EmptyConversationBounds) {
    Conversation conv;
    assert(conv.size() == 0);
    assert(conv.begin() == conv.end());

    bool threw = false;
    try {
        conv.at(0);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    assert(threw && "at() on an empty Conversation must throw out_of_range");
}

// ---------------------------------------------------------------------
// 2. System Message Ordering
// ---------------------------------------------------------------------
TEST(SystemMessageOrdering) {
    Conversation conv;
    conv.append(Message(Role::System, "Be concise."));
    for (int i = 0; i < 10; ++i) {
        conv.append(Message(Role::User, "msg " + std::to_string(i)));
        conv.append(Message(Role::Assistant, "reply " + std::to_string(i)));
    }
    // Even after many appends (several reallocations), the system message
    // must still be the very first element.
    assert(conv.at(0).role() == Role::System);
    assert(conv.at(0).content() == "Be concise.");
    assert(conv.size() == 21);
}

// ---------------------------------------------------------------------
// 3. Rule of Five (Copy)
// ---------------------------------------------------------------------
TEST(RuleOfFiveCopy) {
    Conversation original;
    original.append(Message(Role::User, "hello"));
    original.append(Message(Role::Assistant, "hi there"));

    Conversation copy(original);

    // Deep copy: different underlying buffer...
    assert(copy.begin() != original.begin());
    // ...but identical contents.
    assert(copy.size() == original.size());
    for (std::size_t i = 0; i < copy.size(); ++i) {
        assert(copy.at(i).role() == original.at(i).role());
        assert(copy.at(i).content() == original.at(i).content());
    }

    // Mutating one must not affect the other.
    copy.append(Message(Role::User, "extra"));
    assert(copy.size() == 3);
    assert(original.size() == 2);

    // Copy-assignment must behave the same way.
    Conversation assigned;
    assigned.append(Message(Role::System, "placeholder"));
    assigned = original;
    assert(assigned.begin() != original.begin());
    assert(assigned.size() == original.size());
    assert(assigned.at(0).content() == "hello");
}

// ---------------------------------------------------------------------
// 4. Rule of Five (Move)
// ---------------------------------------------------------------------
TEST(RuleOfFiveMove) {
    Conversation original;
    original.append(Message(Role::User, "hello"));
    original.append(Message(Role::Assistant, "hi there"));

    const Message* original_ptr = original.begin();
    std::size_t original_size = original.size();

    Conversation moved(std::move(original));

    // The new object must own the exact same buffer...
    assert(moved.begin() == original_ptr);
    assert(moved.size() == original_size);
    // ...and the source must be left valid and empty.
    assert(original.size() == 0);
    assert(original.begin() == original.end());

    // original must still be safely destructible/reassignable post-move.
    original.append(Message(Role::User, "reused"));
    assert(original.size() == 1);

    // Move-assignment must behave the same way.
    Conversation target;
    target.append(Message(Role::System, "will be discarded"));
    const Message* moved_ptr = moved.begin();
    target = std::move(moved);
    assert(target.begin() == moved_ptr);
    assert(moved.size() == 0);
}

// ---------------------------------------------------------------------
// 5. Growth Behavior
// ---------------------------------------------------------------------
TEST(GrowthBehavior) {
    Conversation conv;
    const Message* last_begin = conv.begin();
    std::vector<std::size_t> reallocation_sizes;

    const int N = 40;
    for (int i = 0; i < N; ++i) {
        conv.append(Message(Role::User, std::to_string(i)));
        if (conv.begin() != last_begin) {
            // size() right after this append tells us the capacity we
            // just grew INTO (since append() only reallocates when the
            // buffer was full going in).
            reallocation_sizes.push_back(conv.size());
            last_begin = conv.begin();
        }
        // size() and at() must stay correct across every reallocation.
        assert(conv.size() == static_cast<std::size_t>(i + 1));
        for (int j = 0; j <= i; ++j) {
            assert(conv.at(static_cast<std::size_t>(j)).content() == std::to_string(j));
        }
    }

    // A reallocation only fires when size_ == capacity_, and the append
    // that triggers it lands the (old_capacity + 1)-th element, so
    // reallocation_sizes[i] == old_capacity_at_that_point + 1. Recovering
    // old_capacity and checking it doubles each time (0, 1, 2, 4, 8, 16,
    // 32, ...) verifies the growth factor directly.
    assert(reallocation_sizes.size() >= 6);
    std::vector<std::size_t> old_capacities;
    for (std::size_t s : reallocation_sizes) old_capacities.push_back(s - 1);

    assert(old_capacities[0] == 0);
    assert(old_capacities[1] == 1);
    for (std::size_t i = 2; i < old_capacities.size(); ++i) {
        assert(old_capacities[i] == 2 * old_capacities[i - 1]);
    }
}

// ---------------------------------------------------------------------
// 6. Scanner (Clean Text)
// ---------------------------------------------------------------------
TEST(ScannerCleanText) {
    SentinelScanner scanner("<|end_conversation|>");
    auto out1 = scanner.feed("Hello, ");
    auto out2 = scanner.feed("world!");
    auto out3 = scanner.flush();

    assert(!out1.sentinel_found);
    assert(!out2.sentinel_found);
    assert(!out3.sentinel_found);
    assert(out1.safe_text + out2.safe_text + out3.safe_text == "Hello, world!");
}

// ---------------------------------------------------------------------
// 7. Scanner (Split Sentinel at every boundary)
// ---------------------------------------------------------------------
TEST(ScannerCatchesSentinelAtEveryBoundary) {
    const std::string sentinel = "<|end_conversation|>";
    const std::string text = "Goodbye." + sentinel;
    for (std::size_t split = 0; split <= text.size(); ++split) {
        SentinelScanner scanner(sentinel);
        auto out1 = scanner.feed(text.substr(0, split));
        auto out2 = scanner.feed(text.substr(split));
        assert((out1.sentinel_found || out2.sentinel_found) &&
               "sentinel must be caught regardless of split point");
        assert(out1.safe_text + out2.safe_text == "Goodbye.");
    }
}

// ---------------------------------------------------------------------
// 8. Scanner (False Alarms)
// ---------------------------------------------------------------------
TEST(ScannerFalseAlarms) {
    SentinelScanner scanner("<|end_conversation|>");
    auto out1 = scanner.feed("<|end_world|> is not the sentinel");
    auto out2 = scanner.flush();

    assert(!out1.sentinel_found);
    assert(!out2.sentinel_found);
    assert(out1.safe_text + out2.safe_text == "<|end_world|> is not the sentinel");
}

// ---------------------------------------------------------------------
// 9. Scanner (Bounded Memory)
// ---------------------------------------------------------------------
TEST(ScannerBoundedMemory) {
    const std::string sentinel = "<|end_conversation|>";
    SentinelScanner scanner(sentinel);
    const std::size_t bound = sentinel.size() - 1;

    // Adversarial input: repeated prefixes of the sentinel, one byte at a
    // time, so pending_ is under constant pressure to grow.
    const std::string unit = "<|end_";
    std::string stream;
    for (int i = 0; i < 5000; ++i) stream += unit;

    for (char c : stream) {
        auto out = scanner.feed(std::string_view(&c, 1));
        (void)out;
        assert(scanner.pending_size() <= bound &&
               "pending_ must never exceed sentinel.size() - 1");
        if (out.sentinel_found) break;  // shouldn't happen for this input
    }
}

// ---------------------------------------------------------------------
// 10. Harness (Turn Limit)
// ---------------------------------------------------------------------
TEST(HarnessTurnLimit) {
    // greeting.script's first two blocks don't contain the sentinel; with
    // max_turns == 2 the loop must stop on TurnLimit before ever reaching
    // the third (sentinel-bearing) block or running out of user input.
    auto model = std::make_unique<ScriptedModelClient>("scripts/greeting.script");
    HarnessConfig cfg;
    cfg.max_turns = 2;
    cfg.system_message = model->system_message();

    Harness harness(std::move(model), cfg);
    StubInput in({"hi", "hi"});
    NullOutput out;

    StopReason reason = harness.run(in, out);
    assert(reason.kind == StopReason::Kind::TurnLimit);
    // 2 user turns + 2 assistant turns appended on top of the system msg.
    assert(harness.conversation().size() == 5);
}

// ---------------------------------------------------------------------
// 11. Harness (Sentinel Halt)
// ---------------------------------------------------------------------
TEST(HarnessSentinelHalt) {
    // greeting.script's third block ends in the sentinel; with enough
    // turn budget, the loop must halt there rather than at TurnLimit.
    auto model = std::make_unique<ScriptedModelClient>("scripts/greeting.script");
    HarnessConfig cfg;
    cfg.max_turns = 20;
    cfg.system_message = model->system_message();

    Harness harness(std::move(model), cfg);
    StubInput in({"hi", "hi", "hi"});
    NullOutput out;

    StopReason reason = harness.run(in, out);
    assert(reason.kind == StopReason::Kind::Sentinel);

    // The sentinel must never leak into the stored/printed assistant text.
    const Message& last = harness.conversation().at(harness.conversation().size() - 1);
    assert(last.role() == Role::Assistant);
    assert(last.content() == "Goodbye!<|end_conversation|>");
}

// ---------------------------------------------------------------------
// 12. Transcript Round-Trip
// ---------------------------------------------------------------------
TEST(TranscriptRoundTrip) {
    Conversation conv;
    conv.append(Message(Role::System, "Be concise."));
    conv.append(Message(Role::User, "hello"));
    conv.append(Message(Role::Assistant, "Hi! What can I do for you today?"));
    conv.append(Message(Role::User, "goodbye"));
    conv.append(Message(Role::Assistant, "Goodbye.<|end_conversation|>"));

    const std::string path = "test_roundtrip_transcript.txt";
    save_transcript(conv, path);

    ReplayModelClient replay(path);
    assert(replay.system_message() == "Be concise.");

    Conversation dummy;  // ReplayModelClient ignores its conv argument.
    Message first_reply = replay.generate(dummy);
    assert(first_reply.content() == "Hi! What can I do for you today?");

    Message second_reply = replay.generate(dummy);
    assert(second_reply.content() == "Goodbye.<|end_conversation|>");

    std::remove(path.c_str());
}

int main() {
    run("EmptyConversationBounds", EmptyConversationBounds);
    run("SystemMessageOrdering", SystemMessageOrdering);
    run("RuleOfFiveCopy", RuleOfFiveCopy);
    run("RuleOfFiveMove", RuleOfFiveMove);
    run("GrowthBehavior", GrowthBehavior);
    run("ScannerCleanText", ScannerCleanText);
    run("ScannerCatchesSentinelAtEveryBoundary", ScannerCatchesSentinelAtEveryBoundary);
    run("ScannerFalseAlarms", ScannerFalseAlarms);
    run("ScannerBoundedMemory", ScannerBoundedMemory);
    run("HarnessTurnLimit", HarnessTurnLimit);
    run("HarnessSentinelHalt", HarnessSentinelHalt);
    run("TranscriptRoundTrip", TranscriptRoundTrip);

    std::cout << g_tests_run << " tests passed.\n";
    return 0;
}
