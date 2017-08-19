#pragma once

#include <mbgl/style/conversion/get_json_type.hpp>
#include <mbgl/style/expression/parse.hpp>
#include <mbgl/style/expression/let.hpp>
#include <mbgl/style/expression/parsing_context.hpp>
#include <mbgl/style/expression/type.hpp>
#include <mbgl/style/conversion.hpp>

namespace mbgl {
namespace style {
namespace expression {

struct ParseLet {
    template <typename V>
    static ParseResult parse(const V& value, ParsingContext ctx) {
        using namespace mbgl::style::conversion;
        assert(isArray(value));
        
        std::size_t length = arrayLength(value);
        
        if (length < 4) {
            ctx.error("Expected at least 3 arguments, but found " + std::to_string(length - 1) + " instead.");
            return ParseResult();
        }
        
        std::map<std::string, std::shared_ptr<Expression>> bindings;
        for(std::size_t i = 1; i < length - 1; i += 2) {
            optional<std::string> name = toString(arrayMember(value, i));
            if (!name) {
                ctx.error("Expected string, but found " + getJSONType(arrayMember(value, i)) + " instead.", i);
                return ParseResult();
            }
            
            ParseResult bindingValue = parseExpression(arrayMember(value, i + 1), ParsingContext(ctx, i + 1));
            if (!bindingValue) {
                return ParseResult();
            }
            
            bindings.emplace(*name, std::move(*bindingValue));
        }
        
        auto resultContext = ParsingContext(ctx, length - 1, ctx.expected, bindings);
        ParseResult result = parseExpression(arrayMember(value, length - 1), resultContext);
        if (!result) {
            return ParseResult();
        }
        
        return ParseResult(std::make_unique<Let>(std::move(bindings), std::move(*result)));
    }
};

struct ParseVar {
    template <typename V>
    static ParseResult parse(const V& value, ParsingContext ctx) {
        using namespace mbgl::style::conversion;
        assert(isArray(value));
        
        if (arrayLength(value) != 2 || !toString(arrayMember(value, 1))) {
            ctx.error("'var' expression requires exactly one string literal argument.");
            return ParseResult();
        }
        
        std::string name = *toString(arrayMember(value, 1));
        
        optional<std::shared_ptr<Expression>> bindingValue = ctx.getBinding(name);
        if (!bindingValue) {
            ctx.error("Unknown variable \"" + name + "\". Make sure \"" +
                name + "\" has been bound in an enclosing \"let\" expression before using it.", 1);
            return ParseResult();
        }
        
        return ParseResult(std::make_unique<Var>(name, std::move(*bindingValue)));
    }
};

} // namespace expression
} // namespace style
} // namespace mbgl
