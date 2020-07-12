// Copyright 2020 Pokitec
// All rights reserved.

#include "base64.hpp"
#include "../required.hpp"
#include "../exceptions.hpp"

namespace tt {

static int base64_decode(char value) noexcept
{
    if (value >= 'A' && value <= 'Z') {
        return value - 'A';
    } else if (value >= 'a' && value <= 'z') {
        return value - 'a' + 26;
    } else if (value >= '0' && value <= '9') {
        return value - '0' + 52;
    } else {
        switch (value) {
        case '+':
        case '-':
            return 62;
        case '/':
        case '_':
            return 63;
        case ' ':
        case '\n':
        case '\r':
        case '=':
            return -1;
        default:
            return -2;
        }
    }
}

bstring base64_decode(std::string_view src)
{
    auto dst = bstring{};

    auto group_size = 0;
    auto group = 0;
    for (ttlet c : src) {
        switch (auto value = base64_decode(c)) {
        case -2:
            TTAURI_THROW(parse_error("Unexpected character"));
        case -1:
            break;
        default:
            group <<= 6;
            group |= value;
            ++group_size;
        }

        if (group_size == 4) {
            dst.push_back(std::byte{group >> 16});
            dst.push_back(std::byte{group >> 8 & 0xff});
            dst.push_back(std::byte{group & 0xff});
            group = 0;
            group_size = 0;
        }
    }

    switch (group_size) {
    case 0:
        return dst;
    case 1:
        TTAURI_THROW(parse_error("Unexpected number of characters"));
    case 2:
        group <<= 12;
        dst.push_back(std::byte{group >> 16});
        return dst;
    case 3:
        group <<= 6;
        dst.push_back(std::byte{group >> 16});
        dst.push_back(std::byte{group >> 8 & 0xff});
        return dst;
    default:
        tt_no_default;
    }

    tt_unreachable();
}

static char base64_encode(int value) noexcept
{
    auto value_ = static_cast<char>(value);

    if (value_ < 26) {
        return value_ + 'A';
    } else if (value_ < 52) {
        return value_ - 26 + 'a';
    } else if (value_ < 62) {
        return value_ - 52 + '0';
    } else if (value_ < 63) {
        return '+';
    } else {
        return '/';
    }
}

std::string base64_encode(nonstd::span<std::byte const> src) noexcept
{
    auto dst = std::string{};
    ttlet nr_groups = (nonstd::ssize(src) + 2) / 3;
    dst.reserve(nr_groups * 4);

    ttlet nr_full_groups = nonstd::ssize(src) / 3;
    ttlet full_group_end = src.begin() + nr_full_groups * 3;
    auto i = src.begin();
    while (i != full_group_end) {
        ttlet group =
            static_cast<int>(*(i++)) << 16 |
            static_cast<int>(*(i++)) << 8 |
            static_cast<int>(*(i++));

        dst.push_back(base64_encode(group >> 18));
        dst.push_back(base64_encode(group >> 12 & 0x3f));
        dst.push_back(base64_encode(group >> 6 & 0x3f));
        dst.push_back(base64_encode(group & 0x3f));
    }

    if (i == src.end()) {
        return dst;
    }

    auto group = static_cast<uint32_t>(*(i++)) << 16;
    if (i == src.end()) {
        dst.push_back(base64_encode(group >> 18));
        dst.push_back(base64_encode(group >> 12 & 0x3f));
        dst.push_back('=');
        dst.push_back('=');
        return dst;
    }
    
    group |= static_cast<uint32_t>(*i) << 8;
    dst.push_back(base64_encode(group >> 18));
    dst.push_back(base64_encode(group >> 12 & 0x3f));
    dst.push_back(base64_encode(group >> 6 & 0x3f));
    dst.push_back('=');
    return dst;
}

}
