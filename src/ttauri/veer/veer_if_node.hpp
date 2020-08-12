// Copyright 2019, 2020 Pokitec
// All rights reserved.

#pragma once

#include "veer_node.hpp"

namespace tt {

struct veer_if_node final: veer_node {
    std::vector<statement_vector> children_groups;
    std::vector<std::unique_ptr<expression_node>> expressions;
    std::vector<parse_location> expression_locations;

    veer_if_node(parse_location location, std::unique_ptr<expression_node> expression) noexcept :
        veer_node(location)
    {
        expressions.push_back(std::move(expression));
        expression_locations.push_back(location);
        children_groups.emplace_back();
    }

    bool found_elif(parse_location _location, std::unique_ptr<expression_node> expression) noexcept override {
        if (children_groups.size() != expressions.size()) {
            return false;
        }

        expressions.push_back(std::move(expression));
        expression_locations.push_back(std::move(_location));
        children_groups.emplace_back();
        return true;
    }

    bool found_else(parse_location _location) noexcept override {
        if (children_groups.size() != expressions.size()) {
            return false;
        }

        children_groups.emplace_back();
        return true;
    }

    /** Append a template-piece to the current template.
    */
    bool append(std::unique_ptr<veer_node> x) noexcept override {
        append_child(children_groups.back(), std::move(x));
        return true;
    }

    void post_process(expression_post_process_context &context) override {
        tt_assert(std::ssize(expressions) == std::ssize(expression_locations));
        for (ssize_t i = 0; i != std::ssize(expressions); ++i) {
            post_process_expression(context, *expressions[i], expression_locations[i]);
        }

        for (ttlet &children: children_groups) {
            if (std::ssize(children) > 0) {
                children.back()->left_align();
            }

            for (ttlet &child: children) {
                child->post_process(context);
            }
        }
    }

    datum evaluate(expression_evaluation_context &context) override {
        tt_assume(std::ssize(expressions) == std::ssize(expression_locations));
        for (ssize_t i = 0; i != std::ssize(expressions); ++i) {
            if (evaluate_expression_without_output(context, *expressions[i], expression_locations[i])) {
                return evaluate_children(context, children_groups[i]);
            }
        }
        if (std::ssize(children_groups) > std::ssize(expressions)) {
            return evaluate_children(context, children_groups[std::ssize(expressions)]);
        }
        return {};
    }

    std::string string() const noexcept override {
        tt_assert(expressions.size() > 0);
        std::string s = "<if ";
        s += to_string(*expressions[0]);
        s += join(transform<std::vector<std::string>>(children_groups[0], [](auto &x) { return to_string(*x); }));

        for (size_t i = 1; i != expressions.size(); ++i) {
            s += "elif ";
            s += to_string(*expressions[i]);
            s += join(transform<std::vector<std::string>>(children_groups[i], [](auto &x) { return to_string(*x); }));
        }

        if (children_groups.size() != expressions.size()) {
            s += "else ";
            s += join(transform<std::vector<std::string>>(children_groups.back(), [](auto &x) { return to_string(*x); }));
        }

        s += ">";
        return s;
    }
};

}