#pragma once

#include <mbgl/style/expression/parse.hpp>
#include <mbgl/style/expression/at.hpp>
#include <mbgl/style/expression/parsing_context.hpp>
#include <mbgl/style/expression/type.hpp>

namespace mbgl {
namespace style {
namespace expression {

struct ParseAt {
    template <typename V>
    static ParseResult parse(const V& value, ParsingContext ctx) {
        using namespace mbgl::style::conversion;
        assert(isArray(value));
        
        std::size_t length = arrayLength(value);
        if (length != 3) {
            ctx.error("Expected 2 arguments, but found " + std::to_string(length - 1) + " instead.");
            return ParseResult();
        }

        ParseResult index = parseExpression(arrayMember(value, 1), ParsingContext(ctx, 1, {type::Number}));
        ParseResult input = parseExpression(arrayMember(value, 2),
                                            ParsingContext(ctx, 2, {type::Array(ctx.expected ? *ctx.expected : type::Value)}));

        if (!index || !input) return ParseResult();

        return ParseResult(std::make_unique<At>(std::move(*index), std::move(*input)));

    }
};

} // namespace expression
} // namespace style
} // namespace mbgl
