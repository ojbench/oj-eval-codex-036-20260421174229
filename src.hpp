#pragma once
#include <vector>
#include <cstdint>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <string>

namespace sjtu {

struct dynamic_bitset {
    using u64 = std::uint64_t;

    dynamic_bitset() : n_(0) {}
    ~dynamic_bitset() = default;
    dynamic_bitset(const dynamic_bitset &) = default;
    dynamic_bitset &operator=(const dynamic_bitset &) = default;

    explicit dynamic_bitset(std::size_t n) : n_(n) {
        data_.assign(word_count_for(n_), 0);
        mask_tail();
    }

    explicit dynamic_bitset(const std::string &str) {
        n_ = str.size();
        data_.assign(word_count_for(n_), 0);
        for (std::size_t i = 0; i < n_; ++i) if (str[i] == '1') set(i, true);
        mask_tail();
    }

    bool operator[](std::size_t n) const {
        if (n >= n_) return false;
        std::size_t w = n >> 6;
        std::size_t b = n & 63u;
        return (data_[w] >> b) & 1u;
    }

    dynamic_bitset &set(std::size_t n, bool val = true) {
        if (n >= n_) return *this;
        std::size_t w = n >> 6;
        std::size_t b = n & 63u;
        u64 mask = u64(1) << b;
        if (val) data_[w] |= mask; else data_[w] &= ~mask;
        return *this;
    }

    dynamic_bitset &push_back(bool val) {
        std::size_t old_n = n_;
        ++n_;
        if (word_count_for(old_n) != word_count_for(n_)) data_.push_back(0);
        set(n_ - 1, val);
        return *this;
    }

    bool none() const {
        if (n_ == 0) return true;
        std::size_t wc = word_count_for(n_);
        for (std::size_t i = 0; i < wc; ++i) {
            u64 v = data_[i];
            if (i + 1 == wc) v &= tail_mask();
            if (v != 0) return false;
        }
        return true;
    }

    bool all() const {
        if (n_ == 0) return true;
        std::size_t wc = word_count_for(n_);
        if (wc == 0) return true;
        for (std::size_t i = 0; i + 1 < wc; ++i) if (data_[i] != ~u64(0)) return false;
        std::size_t r = n_ & 63u;
        u64 expect = (r == 0) ? ~u64(0) : ((u64(1) << r) - 1);
        return (data_[wc - 1] & expect) == expect;
    }

    std::size_t size() const { return n_; }

    dynamic_bitset &operator|=(const dynamic_bitset &other) {
        bitwise_apply(other, [](u64 &a, u64 b) { a |= b; });
        return *this;
    }

    dynamic_bitset &operator&=(const dynamic_bitset &other) {
        bitwise_apply(other, [](u64 &a, u64 b) { a &= b; });
        return *this;
    }

    dynamic_bitset &operator^=(const dynamic_bitset &other) {
        bitwise_apply(other, [](u64 &a, u64 b) { a ^= b; });
        return *this;
    }

    dynamic_bitset &operator<<=(std::size_t n) {
        if (n == 0 || n_ == 0) {
            n_ += n;
            if (word_count_for(n_) > data_.size()) data_.resize(word_count_for(n_), 0);
            mask_tail();
            return *this;
        }
        std::size_t old_n = n_;
        std::vector<u64> old = data_;
        n_ = old_n + n;
        std::size_t new_wc = word_count_for(n_);
        data_.assign(new_wc, 0);
        std::size_t wshift = n >> 6;
        std::size_t bshift = n & 63u;
        std::size_t old_wc = word_count_for(old_n);
        for (std::size_t i = 0; i < old_wc; ++i) {
            std::size_t j = i + wshift;
            if (j < new_wc) data_[j] |= (bshift == 0) ? old[i] : (old[i] << bshift);
            if (bshift && (j + 1) < new_wc) data_[j + 1] |= old[i] >> (64 - bshift);
        }
        mask_tail();
        return *this;
    }

    dynamic_bitset &operator>>=(std::size_t n) {
        if (n == 0) return *this;
        if (n >= n_) { n_ = 0; data_.clear(); return *this; }
        std::size_t old_n = n_;
        std::vector<u64> old = data_;
        n_ = old_n - n;
        std::size_t new_wc = word_count_for(n_);
        data_.assign(new_wc, 0);
        std::size_t wshift = n >> 6;
        std::size_t bshift = n & 63u;
        std::size_t old_wc = word_count_for(old_n);
        for (std::size_t t = 0; t < new_wc; ++t) {
            std::size_t s = t + wshift;
            u64 v = 0;
            if (s < old_wc) v |= (bshift == 0) ? old[s] : (old[s] >> bshift);
            if (bshift && (s + 1) < old_wc) v |= old[s + 1] << (64 - bshift);
            data_[t] = v;
        }
        mask_tail();
        return *this;
    }

    dynamic_bitset &set() {
        std::size_t wc = word_count_for(n_);
        if (wc == 0) return *this;
        std::fill(data_.begin(), data_.end(), ~u64(0));
        mask_tail();
        return *this;
    }
    dynamic_bitset &flip() {
        for (auto &x : data_) x = ~x;
        mask_tail();
        return *this;
    }
    dynamic_bitset &reset() {
        std::fill(data_.begin(), data_.end(), 0);
        return *this;
    }

private:
    std::size_t n_;
    std::vector<u64> data_;

    static std::size_t word_count_for(std::size_t nbits) { return (nbits + 63) >> 6; }

    u64 tail_mask() const {
        if (n_ == 0) return 0;
        std::size_t r = n_ & 63u;
        return (r == 0) ? ~u64(0) : ((u64(1) << r) - 1);
    }
    void mask_tail() {
        if (data_.empty()) return;
        std::size_t wc = word_count_for(n_);
        if (wc == 0) return;
        data_[wc - 1] &= tail_mask();
    }

    template <class Op>
    void bitwise_apply(const dynamic_bitset &other, Op op) {
        std::size_t len = std::min(n_, other.n_);
        if (len == 0) return;
        std::size_t full = len >> 6;
        for (std::size_t i = 0; i < full; ++i) op(data_[i], other.data_[i]);
        std::size_t rem = len & 63u;
        if (rem) {
            u64 mask = (u64(1) << rem) - 1;
            u64 a = data_[full] & mask;
            u64 b = other.data_[full] & mask;
            u64 tmp = a; op(tmp, b);
            data_[full] = (data_[full] & ~mask) | (tmp & mask);
        }
    }
};

} // namespace sjtu
