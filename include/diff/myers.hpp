/*
 * Diff (without the Match and Patch) (and a number of other reductions)
 * Copyright 2018 The diff-match-patch Authors.
 * Copyright 2019 Victor Grishchenko
 * Copyright 2024 Adobe
 * https://github.com/google/diff-match-patch
 * https://github.com/gritzko/myers-diff
 * https://github.com/fosterbrereton/diff/
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

#include <string>
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

namespace detail {

//----------------------------------------------------------------------------------------------------------------------

patch diff(std::string_view text1, std::string_view text2);

//----------------------------------------------------------------------------------------------------------------------
/**
 * Given the location of the 'middle snake', split the diff in two parts and recurse.
 * @param text1 Old string to be diffed.
 * @param text2 New string to be diffed.
 * @param x Index of split point in text1.
 * @param y Index of split point in text2.
 * @return std::vector of `change` objects.
 */
patch bisect_split(std::string_view text1, std::string_view text2, std::size_t x, std::size_t y) {
    patch lhs = diff(text1.substr(0, x), text2.substr(0, y));
    patch rhs = diff(text1.substr(x), text2.substr(y));
    lhs.insert(lhs.end(), rhs.begin(), rhs.end());
    return lhs;
}

/**
 * Find the 'middle snake' of a diff, split the problem in two
 * and return the recursively constructed diff.
 * See Myers 1986 paper: An O(ND) Difference Algorithm and Its Variations.
 * @param text1 Old string to be diffed.
 * @param text2 New string to be diffed.
 * @return std::vector of `change` objects.
 */
patch bisect(std::string_view text1, std::string_view text2) {
    // Cache the text lengths to prevent multiple calls.
    const int text1_length = static_cast<int>(text1.size());
    const int text2_length = static_cast<int>(text2.size());
    const int max_d = (text1_length + text2_length + 1) / 2;
    const int v_offset = max_d;
    const int v_length = 2 * max_d;
    std::vector<int> v1(v_length, -1);
    std::vector<int> v2(v_length, -1);

    // If the total number of characters is odd, then the front path will
    // collide with the reverse path.
    const int delta = std::abs(text1_length - text2_length);
    const bool front = (delta % 2 != 0);

    v1[v_offset + 1] = 0;
    v2[v_offset + 1] = 0;

    // Offsets for start and end of k loop.
    // Prevents mapping of space beyond the grid.
    int k1start = 0;
    int k1end = 0;
    int k2start = 0;
    int k2end = 0;

    for (int d = 0; d < max_d; ++d) {
        // Walk the front path one step.
        for (int k1 = -d + k1start; k1 <= d - k1end; k1 += 2) {
            int k1_offset = v_offset + k1;
            int x1;
            if (k1 == -d || (k1 != d && v1[k1_offset - 1] < v1[k1_offset + 1])) {
                x1 = v1[k1_offset + 1];
            } else {
                x1 = v1[k1_offset - 1] + 1;
            }
            int y1 = x1 - k1;
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
                int k2_offset = v_offset + delta - k1;
                if (k2_offset >= 0 && k2_offset < v_length && v2[k2_offset] != -1) {
                    // Mirror x2 onto top-left coordinate system.
                    int x2 = text1_length - v2[k2_offset];
                    if (x1 >= x2) {
                        // Overlap detected.
                        return bisect_split(text1, text2, x1, y1);
                    }
                }
            }
        }

        // Walk the reverse path one step.
        for (int k2 = -d + k2start; k2 <= d - k2end; k2 += 2) {
            int k2_offset = v_offset + k2;
            int x2;
            if (k2 == -d || (k2 != d && v2[k2_offset - 1] < v2[k2_offset + 1])) {
                x2 = v2[k2_offset + 1];
            } else {
                x2 = v2[k2_offset - 1] + 1;
            }
            int y2 = x2 - k2;
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
                int k1_offset = v_offset + delta - k2;
                if (k1_offset >= 0 && k1_offset < v_length && v1[k1_offset] != -1) {
                    int x1 = v1[k1_offset];
                    int y1 = v_offset + x1 - k1_offset;
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

    // Number of changes equals number of characters
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
 * @return std::vector of `change` objects.
 */
patch compute(std::string_view text1, std::string_view text2) {
    if (text1.empty()) {
        return patch(1, {operation::ins, text2});
    }

    if (text2.empty()) {
        return patch(1, {operation::del, text1});
    }

    const bool is_t1_longer = text1.size() > text2.size();
    const std::string_view& longtext = is_t1_longer ? text1 : text2;
    const std::string_view& shorttext = is_t1_longer ? text2 : text1;
    const std::size_t i = longtext.find(shorttext);

    if (i != std::string_view::npos) {
        // Shorter text is inside the longer text (speedup).
        patch result;
        operation op = is_t1_longer ? operation::del : operation::ins;
        result.emplace_back(change{op, longtext.substr(0, i)});
        result.emplace_back(change{operation::cpy, shorttext});
        result.emplace_back(change{op, longtext.substr(i + shorttext.size())});
        return result;
    }

    if (shorttext.size() == 1) {
        // Single character string.
        // After the previous speedup, the character can't be an equality.
        patch result;
        result.emplace_back(change{operation::del, text1});
        result.emplace_back(change{operation::ins, text2});
        return result;
    }

    return bisect(text1, text2);
}

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
 * Find the differences between two texts. Simplifies the problem by
 * stripping any common prefix or suffix off the texts before diffing.
 * @return std::vector of `change` objects.
 */
patch diff(std::string_view text1, std::string_view text2) {
    // Check for equality (speedup).
    if (text1 == text2) {
        return text1.empty() ? patch() : patch(1, {operation::cpy, text1});
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

    patch result;

    if (!commonprefix.empty()) {
        result.emplace_back(change{operation::cpy, commonprefix});
    }

    auto mid_block = compute(text1, text2);
    result.insert(result.end(),
                  std::make_move_iterator(mid_block.begin()),
                  std::make_move_iterator(mid_block.end()));

    if (!commonsuffix.empty()) {
        result.emplace_back(change{operation::cpy, commonsuffix});
    }

    return result;
}

} // namespace detail

patch diff(std::string_view text1, std::string_view text2) {
    patch result = detail::diff(text1, text2);

    if (result.size() < 2) {
        return result;
    }

    // the implementation may subdivide along boundaries that
    // can be joined together to produce a "tighter" patch.
    // For example, two back-to-back `del` operations can be
    // united into one. Therefore, run through the set of
    // patches looking for adjacent pairs that have the same
    // operation, and merge them together.

    change* current = &result[0];
    change* next = &result[1];
    change* last = &result[result.size() - 1];

    while (next != last) {
        if (current->operation == next->operation) {
            current->text = std::string_view(current->text.data(), current->text.length() + next->text.length());
            next->text = std::string_view();
        } else {
            current = next;
        }

        ++next;
    }

    // Now that the views have been reconciled, we'll have some
    // with zero lengths. Erase those.
    auto new_end = std::remove_if(result.begin(), result.end(), [](const auto& change){ return change.text.empty(); });
    result.erase(new_end, result.end());

    return result;
}

//----------------------------------------------------------------------------------------------------------------------

} // namespace myers

//----------------------------------------------------------------------------------------------------------------------

#endif // FOSTERBRERETON_DIFF_MYERS_HPP

//----------------------------------------------------------------------------------------------------------------------
