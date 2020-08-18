// Copyright 2019, 2020 Pokitec
// All rights reserved.

#pragma once

#include "stencil_node.hpp"

namespace tt {

struct stencil_block_node final: stencil_node {
    std::string name;
    statement_vector children;

    formula_post_process_context::function_type function;
    formula_post_process_context::function_type super_function;

    stencil_block_node(parse_location location, formula_post_process_context &context, std::unique_ptr<formula_node> name_expression) noexcept :
        stencil_node(std::move(location)), name(name_expression->get_name())
    {
        name = name_expression->get_name();

        super_function = context.set_function(name,
            [&](formula_evaluation_context &context, datum::vector const &arguments) {
            return this->evaluate_call(context, arguments);
        }
        );
    }

    /** Append a template-piece to the current template.
    */
    bool append(std::unique_ptr<stencil_node> x) noexcept override {
        append_child(children, std::move(x));
        return true;
    }

    void post_process(formula_post_process_context &context) override {
        if (std::ssize(children) > 0) {
            children.back()->left_align();
        }

        function = context.get_function(name);
        tt_assert(function);

        context.push_super(super_function);
        for (ttlet &child: children) {
            child->post_process(context);
        }
        context.pop_super();
    }

    datum evaluate(formula_evaluation_context &context) override {
        datum tmp;
        try {
            tmp = function(context, datum::vector{});
        } catch (invalid_operation_error &e) {
            e.merge_location(location);
            throw;
        }

        if (tmp.is_break()) {
            TTAURI_THROW(invalid_operation_error("Found #break not inside a loop statement.").set_location(location));

        } else if (tmp.is_continue()) {
            TTAURI_THROW(invalid_operation_error("Found #continue not inside a loop statement.").set_location(location));

        } else if (tmp.is_undefined()) {
            return {};

        } else {
            TTAURI_THROW(invalid_operation_error("Can not use a #return statement inside a #block.").set_location(location));
        }
    }

    datum evaluate_call(formula_evaluation_context &context, datum::vector const &arguments) {
        context.push();
        auto tmp = evaluate_children(context, children);
        context.pop();

        if (tmp.is_break()) {
            TTAURI_THROW(invalid_operation_error("Found #break not inside a loop statement.").set_location(location));

        } else if (tmp.is_continue()) {
            TTAURI_THROW(invalid_operation_error("Found #continue not inside a loop statement.").set_location(location));

        } else if (tmp.is_undefined()) {
            return {};

        } else {
            TTAURI_THROW(invalid_operation_error("Can not use a #return statement inside a #block.").set_location(location));
        }
    }

    std::string string() const noexcept override {
        std::string s = "<block ";
        s += name;
        s += join(transform<std::vector<std::string>>(children, [](auto &x) { return to_string(*x); }));
        s += ">";
        return s;
    }
};

}