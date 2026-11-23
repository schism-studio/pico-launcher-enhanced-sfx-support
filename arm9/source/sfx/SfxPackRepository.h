#pragma once
#include "common.h"
namespace SfxPackLimits { constexpr unsigned kPacks = 64; }
class SfxPackRepository {
public:
    struct Entry { char folder[256]; };
    void Scan(const bool* cancel = nullptr); // IO only; owner publishes completion.
    unsigned Count() const { return _count; }
    const Entry* Get(unsigned index) const { return index < _count ? &_entries[index] : nullptr; }
    bool Truncated() const { return _truncated; }
    bool ReadError() const { return _error; }
private:
    Entry _entries[SfxPackLimits::kPacks] = {};
    unsigned _count = 0;
    bool _truncated = false, _error = false;
};
