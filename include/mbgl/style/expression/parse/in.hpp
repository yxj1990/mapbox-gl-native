#pragma once

#include <mbgl/style/expression/parse.hpp>
#include <mbgl/style/expression/in.hpp>
#include <mbgl/style/expression/parsing_context.hpp>
#include <mbgl/style/expression/type.hpp>

namespace mbgl {
namespace style {
namespace expression {

struct ParseIn {
    template <typename V>
    static ParseResult parse(const V& value, ParsingContext ctx) {
        using namespace mbgl::style::conversion;
        assert(isArray(value));
        
        std::size_t length = arrayLength(value);
        if (length != 3) {
            ctx.error("Expected 2 arguments, but found " + std::to_string(length - 1) + " instead.");
            return ParseResult();
        }

        ParseResult haystack = parseExpression(arrayMember(value, 2), ParsingContext(ctx, 2, {type::Array(type::Value)}));
        if (!haystack) return ParseResult();
        
        type::Type itemType = (*haystack)->getType().template get<type::Array>().itemType;
        
        ParseResult needle = parseExpression(arrayMember(value, 1), ParsingContext(ctx, 1, {itemType}));
        if (!needle) return ParseResult();
        
        type::Type needleType = (*needle)->getType();
        if (needleType == type::Object || needleType == type::Color || needleType.template is<type::Array>()) {
            ctx.error(R"("contains" does not support searching for values of type )" + toString(needleType) + ".");
            return ParseResult();
        }
        
        return ParseResult(std::make_unique<In>(std::move(*needle), std::move(*haystack)));
    }
};

} // namespace expression
} // namespace style
} // namespace mbgl
