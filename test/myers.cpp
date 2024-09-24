// stdc++
#include <iostream>

// gtest
#include "gtest/gtest.h"

// pbsdk
#include "diff/myers.hpp"

using namespace myers;

//----------------------------------------------------------------------------------------------------------------------

std::string dump(const change& c) {
    return to_char(c.operation) + std::string("    ") + std::string(c.text.data(), c.text.size());
}

std::string dump(const patch& p) {
    std::string result;
    bool first = true;
    for (const auto& c : p) {
        if (first) {
            first = false;
        } else {
            result += '\n';
        }
        result += dump(c);
    }
    return result;
}

//----------------------------------------------------------------------------------------------------------------------

void examine_results(const patch& computed, const patch& expected) {
    bool failure = false;

    if (computed.size() != expected.size()) {
        failure = true;

        std::cout << "entry count mismatch:\n"
               << "  computed: \n"
               << dump(computed) << '\n'
               << "  expected: \n"
               << dump(expected) << '\n';
    }

    for (std::size_t i = 0; i < expected.size(); ++i) {
        const auto& comput = computed[i];
        const auto& expect = expected[i];
        if (comput == expect) {
            continue;
        }

        failure = true;

        std::cout << "entry " << i << " mismatch:\n"
               << "  computed: \n"
               << dump(comput) << '\n'
               << "  expected: \n"
               << dump(expect) << '\n';
    }

    if (failure) {
        FAIL() << "failure in examine_results";
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST(myers, replacement_0) {
    const patch computed = diff("if (foo::size(result)) {", "if (bar::get_size(result)) {");
    const patch expected = {
        { operation::cpy, "if (" },
        { operation::del, "foo::" },
        { operation::ins, "bar::get_" },
        { operation::cpy, "size(result)) {" },
    };
    examine_results(computed, expected);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(myers, equality) {
    const patch computed = diff("banana", "banana");
    const patch expected = {
        { operation::cpy, "banana" },
    };
    examine_results(computed, expected);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(myers, replacement_all) {
    const patch computed = diff("bar_banana_foo", "foo_banana_bar");
    const patch expected = {
        { operation::del, "bar_banana_foo" },
        { operation::ins, "foo_banana_bar" },
    };
    examine_results(computed, expected);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(myers, replacement_back) {
    {
    const patch computed = diff("banana_foofoofoo", "banana_barbarbar");
    const patch expected = {
        { operation::cpy, "banana_" },
        { operation::del, "foofoofoo" },
        { operation::ins, "barbarbar" },
    };
    examine_results(computed, expected);
    }

    {
    const patch computed = diff("bananaa", "bananab");
    const patch expected = {
        { operation::cpy, "banana" },
        { operation::del, "a" },
        { operation::ins, "b" },
    };
    examine_results(computed, expected);
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST(myers, replacement_front) {
    {
    const patch computed = diff("foofoofoo_banana", "barbarbar_banana");
    const patch expected = {
        { operation::del, "foofoofoo" },
        { operation::ins, "barbarbar" },
        { operation::cpy, "_banana" },
    };
    examine_results(computed, expected);
    }

    {
    const patch computed = diff("abanana", "bbanana");
    const patch expected = {
        { operation::del, "a" },
        { operation::ins, "b" },
        { operation::cpy, "banana" },
    };
    examine_results(computed, expected);
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST(myers, transcription_pairing) {
    std::vector<std::string> src_keys = {
        "banana_preset(foobar::banana_preset &&)",
        "banana_preset(const foobar::banana_preset &)",
        "explicit banana_preset(std::shared_ptr<implementation>)",
    };

    std::vector<std::string> dst_keys = {
        "explicit banana_preset(std::shared_ptr<implementation>)",
        "banana_preset(const banana_preset &)",
        "banana_preset(banana_preset &&)",
    };

    struct pair {
        std::string src;
        std::string dst;
    };
    std::vector<pair> result;

    const auto score = [](const patch& p) {
        std::size_t score = 0;

        for (const auto& c : p) {
            switch (c.operation) {
                case operation::cpy: break;
                case operation::del:
                case operation::ins: {
                    score += c.text.size();
                } break;
            }
        }

        return score;
    };

    while (!src_keys.empty()) {
        pair cur_pair;
        cur_pair.src = std::move(src_keys.back());
        src_keys.pop_back();
        std::size_t best_match = std::numeric_limits<std::size_t>::max();
        std::size_t best_index = 0;
        for (std::size_t i = 0; i < dst_keys.size(); ++i) {
            const auto patch = diff(cur_pair.src, dst_keys[i]);
            std::size_t cur_match = score(patch);
            if (cur_match > best_match) {
                continue;
            }
            best_match = cur_match;
            best_index = i;
        }
        cur_pair.dst = std::move(dst_keys[best_index]);
        dst_keys.erase(dst_keys.begin() + best_index);
        result.emplace_back(std::move(cur_pair));
    }

    EXPECT_EQ(result[0].src, "explicit banana_preset(std::shared_ptr<implementation>)");
    EXPECT_EQ(result[0].dst, "explicit banana_preset(std::shared_ptr<implementation>)");
    EXPECT_EQ(result[1].src, "banana_preset(const foobar::banana_preset &)");
    EXPECT_EQ(result[1].dst, "banana_preset(const banana_preset &)");
    EXPECT_EQ(result[2].src, "banana_preset(foobar::banana_preset &&)");
    EXPECT_EQ(result[2].dst, "banana_preset(banana_preset &&)");
}

//----------------------------------------------------------------------------------------------------------------------
