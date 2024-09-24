/*
 * Diff (without the Match and Patch) (and a number of other reductions)
 * Copyright 2018 The diff-match-patch Authors.
 * Copyright 2019 Victor Grishchenko
 * Copyright 2024 Adobe
 * https://github.com/google/diff-match-patch
 * https://github.com/gritzko/myers-diff
 * https://github.com/fosterbrereton/myers-diff/
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef FOSTERBRERETON_DIFF_MYERS_HPP
#define FOSTERBRERETON_DIFF_MYERS_HPP

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

//----------------------------------------------------------------------------------------------------------------------

namespace myers {

//----------------------------------------------------------------------------------------------------------------------

enum class operation { cpy, ins, del };

constexpr char to_char(operation op) {
    switch (op) {
            // clang-format off
        case operation::cpy: return '=';
        case operation::ins: return '+';
        case operation::del: return '-';
            // clang-format on
    }
}

//----------------------------------------------------------------------------------------------------------------------

struct change {
    operation operation;
    std::string_view text;
};

bool operator==(const change& a, const change& b) { return a.operation == b.operation && a.text == b.text; }

using patch = std::vector<change>;

//----------------------------------------------------------------------------------------------------------------------

patch diff(std::string_view text1, std::string_view text2);

//----------------------------------------------------------------------------------------------------------------------
/**
 * Determine the common prefix of two strings
 * @param text1 First string.
 * @param text2 Second string.
 * @return The number of characters common to the start of each string.
 */
std::size_t common_prefix(std::string_view text1, std::string_view text2) {
    // Performance analysis: https://neil.fraser.name/news/2007/10/09/
    std::size_t n = std::min(text1.size(), text2.size());
    for (int i = 0; i < n; i++) {
        if (text1[i] != text2[i]) {
            return i;
        }
    }
    return n;
}

/**
 * Determine the common suffix of two strings
 * @param text1 First string.
 * @param text2 Second string.
 * @return The number of characters common to the end of each string.
 */
std::size_t common_suffix(std::string_view text1, std::string_view text2) {
    // Performance analysis: https://neil.fraser.name/news/2007/10/09/
    std::size_t text1_length = text1.size();
    std::size_t text2_length = text2.size();
    std::size_t n = std::min(text1_length, text2_length);
    for (std::size_t i = 1; i <= n; i++) {
        if (text1[text1_length - i] != text2[text2_length - i]) {
            return i - 1;
        }
    }
    return n;
}

/**
 * Given the location of the 'middle snake', split the diff in two parts
 * and recurse.
 * @param text1 Old string to be diffed.
 * @param text2 New string to be diffed.
 * @param x Index of split point in text1.
 * @param y Index of split point in text2.
 * @return std::vector of Diff objects.
 */
patch bisect_split(std::string_view text1, std::string_view text2, std::size_t x, std::size_t y) {
    std::string_view text1a = text1.substr(0, x);
    std::string_view text2a = text2.substr(0, y);
    std::string_view text1b = text1.substr(x);
    std::string_view text2b = text2.substr(y);

    // Compute both diffs serially.
    patch diffs = diff(text1a, text2a);
    patch diffsb = diff(text1b, text2b);

    diffs.insert(diffs.end(), diffsb.begin(), diffsb.end());
    return diffs;
}

/**
 * Find the 'middle snake' of a diff, split the problem in two
 * and return the recursively constructed diff.
 * See Myers 1986 paper: An O(ND) Difference Algorithm and Its Variations.
 * @param text1 Old string to be diffed.
 * @param text2 New string to be diffed.
 * @return std::vector of Diff objects.
 */
patch bisect(std::string_view text1, std::string_view text2) {
    // Cache the text lengths to prevent multiple calls.
    std::size_t text1_length = text1.size();
    std::size_t text2_length = text2.size();
    std::size_t max_d = (text1_length + text2_length + 1) / 2;
    std::size_t v_offset = max_d;
    std::size_t v_length = 2 * max_d;
    std::vector<std::size_t> v1;
    v1.resize(v_length);
    std::vector<std::size_t> v2;
    v2.resize(v_length);
    for (std::size_t x = 0; x < v_length; x++) {
        v1[x] = std::string_view::npos;
        v2[x] = std::string_view::npos;
    }
    v1[v_offset + 1] = 0;
    v2[v_offset + 1] = 0;
    std::size_t delta = text1_length - text2_length;
    // If the total number of characters is odd, then the front path will
    // collide with the reverse path.
    bool front = (delta % 2 != 0);
    // Offsets for start and end of k loop.
    // Prevents mapping of space beyond the grid.
    std::size_t k1start = 0;
    std::size_t k1end = 0;
    std::size_t k2start = 0;
    std::size_t k2end = 0;
    for (std::size_t d = 0; d < max_d; d++) {
        // Walk the front path one step.
        for (std::size_t k1 = -d + k1start; k1 <= d - k1end; k1 += 2) {
            std::size_t k1_offset = v_offset + k1;
            std::size_t x1;
            if (k1 == -d || (k1 != d && v1[k1_offset - 1] < v1[k1_offset + 1])) {
                x1 = v1[k1_offset + 1];
            } else {
                x1 = v1[k1_offset - 1] + 1;
            }
            std::size_t y1 = x1 - k1;
            while (x1 < text1_length && y1 < text2_length && text1[x1] == text2[y1]) {
                x1++;
                y1++;
            }
            v1[k1_offset] = x1;
            if (x1 > text1_length) {
                // Ran off the right of the graph.
                k1end += 2;
            } else if (y1 > text2_length) {
                // Ran off the bottom of the graph.
                k1start += 2;
            } else if (front) {
                std::size_t k2_offset = v_offset + delta - k1;
                if (k2_offset >= 0 && k2_offset < v_length && v2[k2_offset] != std::string_view::npos) {
                    // Mirror x2 onto top-left coordinate system.
                    std::size_t x2 = text1_length - v2[k2_offset];
                    if (x1 >= x2) {
                        // Overlap detected.
                        return bisect_split(text1, text2, x1, y1);
                    }
                }
            }
        }

        // Walk the reverse path one step.
        for (std::size_t k2 = -d + k2start; k2 <= d - k2end; k2 += 2) {
            std::size_t k2_offset = v_offset + k2;
            std::size_t x2;
            if (k2 == -d || (k2 != d && v2[k2_offset - 1] < v2[k2_offset + 1])) {
                x2 = v2[k2_offset + 1];
            } else {
                x2 = v2[k2_offset - 1] + 1;
            }
            std::size_t y2 = x2 - k2;
            while (x2 < text1_length && y2 < text2_length &&
                   text1[text1_length - x2 - 1] == text2[text2_length - y2 - 1]) {
                x2++;
                y2++;
            }
            v2[k2_offset] = x2;
            if (x2 > text1_length) {
                // Ran off the left of the graph.
                k2end += 2;
            } else if (y2 > text2_length) {
                // Ran off the top of the graph.
                k2start += 2;
            } else if (!front) {
                std::size_t k1_offset = v_offset + delta - k2;
                if (k1_offset >= 0 && k1_offset < v_length && v1[k1_offset] != std::string_view::npos) {
                    std::size_t x1 = v1[k1_offset];
                    std::size_t y1 = v_offset + x1 - k1_offset;
                    // Mirror x2 onto top-left coordinate system.
                    x2 = text1_length - x2;
                    if (x1 >= x2) {
                        // Overlap detected.
                        return bisect_split(text1, text2, x1, y1);
                    }
                }
            }
        }
    }
    // Number of diffs equals number of characters; no commonality at all.
    patch diffs;
    diffs.emplace_back(change{operation::del, text1});
    diffs.emplace_back(change{operation::ins, text2});
    return diffs;
}

/**
 * Find the differences between two texts.  Assumes that the texts do not
 * have any common prefix or suffix.
 * @param text1 Old string to be diffed.
 * @param text2 New string to be diffed.
 * @return std::vector of Diff objects.
 */
patch compute(std::string_view text1, std::string_view text2) {
    patch diffs;

    if (text1.size() == 0) {
        // Just add some text (speedup).
        diffs.emplace_back(change{operation::ins, text2});
        return diffs;
    }

    if (text2.size() == 0) {
        // Just delete some text (speedup).
        diffs.emplace_back(change{operation::del, text1});
        return diffs;
    }

    const bool is_t1_longer = text1.size() > text2.size();
    std::string_view longtext = is_t1_longer ? text1 : text2;
    std::string_view shorttext = is_t1_longer ? text2 : text1;
    std::size_t i = longtext.find(shorttext);

    if (i != std::string_view::npos) {
        // Shorter text is inside the longer text (speedup).
        operation op = is_t1_longer ? operation::del : operation::ins;
        diffs.emplace_back(change{op, longtext.substr(0, i)});
        diffs.emplace_back(change{operation::cpy, shorttext});
        diffs.emplace_back(change{op, longtext.substr(i + shorttext.size())});
        return diffs;
    }

    if (shorttext.size() == 1) {
        // Single character string.
        // After the previous speedup, the character can't be an equality.
        diffs.emplace_back(change{operation::del, text1});
        diffs.emplace_back(change{operation::ins, text2});
        return diffs;
    }

    return bisect(text1, text2);
}

/**
 * Find the differences between two texts. Simplifies the problem by
 * stripping any common prefix or suffix off the texts before diffing.
 * @return std::vector of `change` objects.
 */
patch diff(std::string_view text1, std::string_view text2) {
    // Check for equality (speedup).
    patch diffs;
    if (text1 == text2) {
        if (text1.size() != 0) {
            diffs.emplace_back(change{operation::cpy, text1});
        }
        return diffs;
    }

    // Trim off common prefix (speedup).
    std::size_t commonlength = common_prefix(text1, text2);
    std::string_view commonprefix = text1.substr(0, commonlength);
    text1 = text1.substr(commonlength);
    text2 = text2.substr(commonlength);

    // Trim off common suffix (speedup).
    commonlength = common_suffix(text1, text2);
    std::string_view commonsuffix = text1.substr(text1.size() - commonlength);
    text1 = text1.substr(0, text1.size() - commonlength);
    text2 = text2.substr(0, text2.size() - commonlength);

    // Compute the diff on the middle block.
    diffs = compute(text1, text2);

    // Restore the prefix and suffix.
    if (commonprefix.size() != 0) {
        diffs.insert(diffs.begin(), change{operation::cpy, commonprefix});
    }
    if (commonsuffix.size() != 0) {
        diffs.emplace_back(change{operation::cpy, commonsuffix});
    }

    return diffs;
}

//----------------------------------------------------------------------------------------------------------------------

} // namespace myers

//----------------------------------------------------------------------------------------------------------------------

#endif // FOSTERBRERETON_DIFF_MYERS_HPP

//----------------------------------------------------------------------------------------------------------------------
