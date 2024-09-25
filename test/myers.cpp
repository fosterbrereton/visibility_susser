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

TEST(myers, replacement_middle) {
    {
    const patch computed = diff("explicit application(apollo::any_scheduler, apollo::any_error_handler, apollo::any_crumb_handler, apollo::application::idle_timer, apollo::core_settings)",
                                "explicit application(any_scheduler, any_error_handler, any_crumb_handler, idle_timer, core_settings)");
    const patch expected = {
        { operation::cpy, "banana_" },
        { operation::del, "foofoofoo" },
        { operation::ins, "barbarbar" },
    };
    examine_results(computed, expected);
    }
}

//----------------------------------------------------------------------------------------------------------------------

struct transcribe_pair {
    std::string src;
    std::string dst;
};

using transcribe_pairs = std::vector<transcribe_pair>;

// This is O(N^2), where N is the size of both `src` and `dst`. Therefore transcription
// should only be run when it is shown to be necessary. At the same time, if your code base
// has enough overrides to really slow this algorithm down, the performance of this routine
// is the least of your concerns.
transcribe_pairs derive_transcribe_pairs(std::vector<std::string> src, std::vector<std::string> dst) {
    if (src.size() != dst.size()) {
        std::cerr << "WARNING: transcription key count mismatch\n";
    }

    const auto score = [](const myers::patch& p) {
        std::size_t score = 0;

        for (const auto& c : p) {
            switch (c.operation) {
                case myers::operation::cpy:
                    break;
                case myers::operation::del:
                case myers::operation::ins: {
                    score += c.text.size();
                } break;
            }
        }

        return score;
    };

    transcribe_pairs result;

    while (!src.empty()) {
        transcribe_pair cur_pair;

        // pop a key off the old name set
        cur_pair.src = std::move(src.back());
        src.pop_back();

        // find the best match of the dst keys to the src key
        std::size_t best_match = std::numeric_limits<std::size_t>::max();
        std::size_t best_index = 0;
        for (std::size_t i = 0; i < dst.size(); ++i) {
            // generate the meyers diff of the src key and the candidate dst
            const myers::patch patch = myers::diff(cur_pair.src, dst[i]);
            std::size_t cur_match = score(patch);

            if (cur_match > best_match) {
                continue;
            }

            // if this dst candidate is better than what we've seen, remember that.
            best_match = cur_match;
            best_index = i;
        }

        // pair the best match dst and src keys and remove dst
        cur_pair.dst = std::move(dst[best_index]);
        dst.erase(dst.begin() + best_index);

        // save off the pair and repeat
        result.emplace_back(std::move(cur_pair));
    }

    return result;
}

//----------------------------------------------------------------------------------------------------------------------

TEST(myers, transcription_pairing_0) {
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

    const auto result = derive_transcribe_pairs(src_keys, dst_keys);

    EXPECT_EQ(result[0].src, "explicit banana_preset(std::shared_ptr<implementation>)");
    EXPECT_EQ(result[0].dst, "explicit banana_preset(std::shared_ptr<implementation>)");
    EXPECT_EQ(result[1].src, "banana_preset(const foobar::banana_preset &)");
    EXPECT_EQ(result[1].dst, "banana_preset(const banana_preset &)");
    EXPECT_EQ(result[2].src, "banana_preset(foobar::banana_preset &&)");
    EXPECT_EQ(result[2].dst, "banana_preset(banana_preset &&)");
}

//----------------------------------------------------------------------------------------------------------------------

TEST(myers, transcription_pairing_1) {
    std::vector<std::string> src_keys = {
        "application()",
        "application(foobar::application &&)",
        "application(const foobar::application &)",
        "explicit application(foobar::any_scheduler, foobar::any_error_handler, foobar::any_crumb_handler, foobar::application::idle_timer, foobar::core_settings)",
    };

    std::vector<std::string> dst_keys = {
        "application()",
        "application(application &&)",
        "application(const application &)",
        "explicit application(any_scheduler, any_error_handler, any_crumb_handler, idle_timer, core_settings)",
    };

    const auto result = derive_transcribe_pairs(src_keys, dst_keys);

    EXPECT_EQ(result[0].src, "application()");
    EXPECT_EQ(result[0].dst, "application()");
    EXPECT_EQ(result[1].src, "application(foobar::application &&)");
    EXPECT_EQ(result[1].dst, "application(application &&)");
    EXPECT_EQ(result[2].src, "application(const foobar::application &)");
    EXPECT_EQ(result[2].dst, "application(const application &)");
    EXPECT_EQ(result[3].src, "explicit application(foobar::any_scheduler, foobar::any_error_handler, foobar::any_crumb_handler, foobar::application::idle_timer, foobar::core_settings)");
    EXPECT_EQ(result[3].dst, "explicit application(any_scheduler, any_error_handler, any_crumb_handler, idle_timer, core_settings)");
}

//----------------------------------------------------------------------------------------------------------------------
