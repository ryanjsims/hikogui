// Copyright 2019, 2020 Pokitec
// All rights reserved.

#pragma once

#include "veer_node.hpp"

namespace tt {

struct veer_string_node final: veer_node {
    std::string text;

    veer_string_node(parse_location location, std::string text) :
        veer_node(std::move(location)), text(std::move(text)) {}

    [[nodiscard]] bool should_left_align() const noexcept override { return false; }

    void left_align() noexcept override {
        // first check if there are only spaces and tabs after the last line feed.
        auto new_text_length = std::ssize(text);
        for (auto i = text.crbegin(); i != text.crend(); ++i, --new_text_length) {
            if (*i == ' ' || *i == '\t') {
                // Strip spaces and tabs.
                continue;
            } else if (*i == '\n') {
                // Stop here, we are stripping a line upto the last line feed.
                break;
            } else {
                // If there are characters after the last line feed, we do not want to strip the line.
                return;
            }
        }
        text.resize(new_text_length);
    }

    std::string string() const noexcept override {
        return fmt::format("<text {}>", text);
    }

    datum evaluate(expression_evaluation_context &context) override {
        context.write(text);
        return {};
    }
};

}