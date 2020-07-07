
#include "ttauri/required.hpp"
#include "ttauri/bits.hpp"
#include "ttauri/numeric_cast.hpp"
#include <nonstd/span>
#include <vector>

namespace tt {

template<typename T>
class huffman_tree {
    static_assert(std::is_integral_v<T> && std::is_signed_v<T>);

    using state_t = T const *;

    /** The internal data structure holding the tree.
     * A pair of values are added to the list for each tree-node.
     * The values have the following meaning:
     *  - negative number are relative offset from the current
     *    position to the next tree node. So that it is possible
     *    to simply add the negative value to the iterator.
     *  - positive number is the symbol value + 1.
     *  - zero was unused or not yet assigned.
     */
    std::vector<T> tree;

public:
    huffman_tree() {
        tree.push_back(0);
        tree.push_back(0);
    }

    /** Add a symbol to the huffman_tree.
     * 
     */
    void add(int symbol, int code, int code_length) noexcept {
        tt_assume(code_length >= 1);

        int offset = 0;
        while (--code_length > 0) {
            int select = (code >> code_length) & 1;
            offset += select;
          
            int value = tree[offset];

            // value may not be a leaf.
            tt_assume(value <= 0); 

            if (value == 0) {
                // Unused node entry. Point to the first of two new entries.
                value = -(static_cast<int>(ssize(tree)) - offset);

                tree[offset] = numeric_cast<T>(value);
                tree.push_back(0);
                tree.push_back(0);
            }

            // Go to the next entry.
            offset -= value;
        }

        // place the symbol as a leaf.
        int select = code & 1;
        offset += select;

        tt_assume(tree[offset] == 0); 
        tree[offset] = numeric_cast<T>(symbol + 1);
    }

    [[nodiscard]] state_t start() const noexcept {
        return tree.data();
    }

    /** Get a symbol from the huffman-tree.
     *
     * Before `get()` is called use `start()` to create a state to
     * pass between invocations. State is invalid after `get()` returns
     * a symbol or throws.
     *
     * @param code_bit The next bit from the huffman encoded stream.
     * @param state The state carried between invocations @see start().
     * @return Positive numbers are symbols, negative means more code_bits
     *         are needed.
     * @throw parse-error on invalid code-bit sequence.
     */
    [[nodiscard]] int get(bool code_bit, state_t &state) const {
        state += static_cast<ptrdiff_t>(code_bit);

        if (*state == 0) {
            TTAURI_THROW(parse_error("Code not in huffman tree."));
        }

        auto value = *state;
        state -= static_cast<ptrdiff_t>(value);
        return value - 1;
    }

    [[nodiscard]] int get_symbol(nonstd::span<std::byte const> bytes, ssize_t &bit_offset) const noexcept {
        auto state = start();
        while (true) {
            int symbol;
            if ((symbol = get(get_bit(bytes, bit_offset), state)) >= 0) {
                return symbol;
            }
        }
    }
    
    /** Build a canonical-huffman table from a set of lengths.
     */
    [[nodiscard]] static huffman_tree from_lengths(int const *lengths, ssize_t nr_symbols) {
        struct symbol_length_t {
            int symbol;
            int length;

            symbol_length_t(int symbol, int length) : symbol(symbol), length(length) {}
        };

        std::vector<symbol_length_t> symbol_lengths;
        symbol_lengths.reserve(nr_symbols);

        for (int symbol = 0; symbol != nr_symbols; ++symbol) {
            symbol_lengths.emplace_back(symbol, lengths[symbol]);
        }

        // Sort the table based on the length of the code, followed by symbol
        std::sort(symbol_lengths.begin(), symbol_lengths.end(), [](ttlet &a, ttlet &b) {
            if (a.length == b.length) {
                return a.symbol < b.symbol;
            } else {
                return a.length < b.length;
            }
        });

        auto r = huffman_tree{};

        int code = 0;
        int prev_length = 0;
        for (auto &&entry: symbol_lengths) {
            if (entry.length != 0) {
                code <<= (entry.length - prev_length);

                r.add(entry.symbol, code, entry.length);
                ++code;
            }

            prev_length = entry.length;
        }

        return r;
    }

    [[nodiscard]] static huffman_tree from_lengths(std::vector<int> const &lengths) {
        return from_lengths(lengths.data(), ssize(lengths));
    }
};



}