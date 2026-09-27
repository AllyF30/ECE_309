# Design Log — Project 2

(500–800 words total. See spec §5 for what each section must cover.)

## Growth factor and amortized cost



## Rule of Five evidence



## Sentinel scanner: bounded pending_ proof

# Design Log — Project 2

## Growth factor and amortized cost

`Conversation::append` uses a doubling growth factor: when `size_ == capacity_`,
the new capacity is `capacity_ == 0 ? 1 : capacity_ * 2`. Starting from an
empty conversation, capacity follows 0 -> 1 -> 2 -> 4 -> 8 -> 16 -> ... - each
reallocation happens exactly when the buffer is full, and the new buffer is
twice the size of the old one.

**Claim:** `append` is amortized O(1), i.e. `n` calls to `append` cost O(n)
total, even though any individual call can cost O(k) (copying/moving `k`
existing elements during a reallocation).

**Proof (aggregate method).** Consider `n` appends starting from an empty
container. Reallocations happen only at sizes `1, 2, 4, 8, ..., 2^m` where
`2^m` is the largest power of two less than `n`. At the reallocation that
grows capacity from `2^i` to `2^(i+1)`, exactly `2^i` elements are moved.
The total work done by all reallocations across the whole sequence of `n`
appends is therefore bounded by:

```
1 + 2 + 4 + 8 + ... + 2^m  <  2 * 2^m  <  2n
```

(a geometric series sums to less than twice its largest term). Every append
also does O(1) work writing the new element itself, contributing another
O(n) total. So the total cost of `n` appends is O(n) + O(n) = O(n), which
means the *average* cost per append is O(n)/n = O(1) - amortized constant
time, even though the worst-case single call is O(n).

The key intuition: each reallocation of cost `k` is "paid for" by the `k`
cheap appends that happened since the previous reallocation (which is why
the buffer was exactly full when this one triggered) - the expensive step
never happens without that much cheap work backing it.

## Rule of Five evidence

- **Copy constructor / copy assignment** allocate a brand-new `Message[]`
  buffer sized to the source's capacity and copy every live element into it
  with a loop (`data_[i] = other.data_[i]`), rather than copying the
  pointer. `RuleOfFiveCopy` in `test_p2.cpp` asserts `copy.begin() !=
  original.begin()` (different buffer) and that appending to one doesn't
  change the other's `size()`.
- Copy assignment allocates and populates the *new* buffer before touching
  `*this`, and only then frees the old buffer and swaps pointers in. A
  `new` failure therefore leaves the target unmodified, and self-assignment
  (`this == &other`) is checked explicitly.
- **Move constructor / move assignment** steal the three fields (`data_`,
  `size_`, `capacity_`) and null out the source's fields, with no
  per-element work - O(1) regardless of size. `RuleOfFiveMove` asserts the
  destination's `begin()` pointer is bit-for-bit the pointer the source
  used to own, and that the source is left at `size() == 0` (safe to
  destroy or reuse).
- The destructor is a single `delete[] data_`, a documented no-op on
  `nullptr`, so destroying a moved-from or default-constructed
  `Conversation` is always safe.
- Running the whole test suite, the interactive CLI, `--save`, and a
  replay round-trip under `-fsanitize=address,undefined` produced zero
  leak/UB reports - the strongest available evidence against shallow-copy
  double-frees and use-after-move bugs.

## Sentinel scanner: bounded pending_ proof

**Invariant:** after every call to `feed()` returns, `pending_.size() <=
sentinel_.size() - 1`.

**Proof.** Let `L = sentinel_.size()`. On each call, `feed` appends the
incoming chunk to `pending_`, then searches all of `pending_` for the
sentinel.

If found, `pending_` is cleared entirely (size 0), so the invariant holds
trivially. If not found, no substring of `pending_` equals the full
sentinel. Anything older than the trailing `L - 1` characters can never
become part of a future match: if it were part of one, that occurrence
would end within text already searched, and `find` would already have
reported it. So it's safe to emit everything except the trailing
`min(pending_.size(), L - 1)` characters - exactly what the code does:
`keep = L - 1`; if `pending_.size() > keep`, the prefix up to
`pending_.size() - keep` is emitted and erased, leaving exactly `keep`
characters. If `pending_.size() <= keep` already, nothing changes - still
within bound by hypothesis.

By induction over successive `feed` calls, the invariant holds throughout:
`pending_` is capped at `L - 1` characters regardless of how many chunks
arrive or how they're split. This keeps memory use O(1) per chunk instead
of the O(n^2) blowup from concatenating the whole stream and re-searching
it from scratch each time.

`ScannerBoundedMemory` in `test_p2.cpp` exercises this directly: it feeds
an adversarial stream (repeated sentinel prefixes) one byte at a time and
asserts `scanner.pending_size() <= sentinel.size() - 1` after every single
byte.

## What I would change differently

I'd replace the naive `pending_.find(sentinel_)` re-scan with
Knuth-Morris-Pratt (the spec's stretch goal). The current approach re-runs
`find` over up to `L` characters per call, fine for a short, fixed sentinel
like `<|end_conversation|>`, but on adversarial input like
`<|end_<|end_<|end_...` it can re-examine overlapping prefixes it's
effectively already ruled out - more than the O(1)-per-character bound
KMP's failure function would guarantee. Not necessary here since `L` is
small and fixed, but the right fix for a longer or user-supplied sentinel.


## What I would change differently
