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
