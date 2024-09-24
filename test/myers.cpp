// stdc++
#include <iostream>

// gtest
#include "gtest/gtest.h"

// pbsdk
#include "diff/myers.hpp"

using namespace myers;

//----------------------------------------------------------------------------------------------------------------------
// save for debugging.
void dump(const patch& p) {
    for (const auto& c : p) {
        std::cout << to_char(c.operation) << "    " << c.text << '\n';
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST(myers, test0) {
    const patch result = diff("if (boost::size(result)) {", "if (adobe::token_range_size(result)) {");
    const patch expected = {
        { operation::cpy, "if (" },
        { operation::del, "boost::" },
        { operation::ins, "adobe::token_range_" },
        { operation::cpy, "size(result)) {" },
    };
    EXPECT_TRUE(std::equal(result.begin(), result.end(), expected.begin(), expected.end()));
}

//----------------------------------------------------------------------------------------------------------------------
