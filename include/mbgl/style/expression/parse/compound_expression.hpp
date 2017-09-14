#pragma once

#include <array>
#include <vector>
#include <memory>
#include <mbgl/style/expression/compound_expression.hpp>
#include <mbgl/style/expression/type.hpp>
#include <mbgl/style/expression/value.hpp>
#include <mbgl/style/expression/parsing_context.hpp>
#include <mbgl/style/conversion.hpp>

namespace mbgl {
namespace style {
namespace expression {

struct ParseCompoundExpression {
    template <class V>
    static ParseResult parse(const std::string name, const V& value, ParsingContext ctx) {
        using namespace mbgl::style::conversion;
        assert(isArray(value) && arrayLength(value) > 0);
        
        auto it = CompoundExpressionRegistry::definitions.find(name);
        if (it == CompoundExpressionRegistry::definitions.end()) {
            ctx.error(
                 R"(Unknown expression ")" + name + R"(". If you wanted a literal array, use ["literal", [...]].)",
                0
            );
            return ParseResult();
        }
        const CompoundExpressionRegistry::Definition& definition = it->second;

        // parse subexpressions first
        std::vector<std::unique_ptr<Expression>> args;
        auto length = arrayLength(value);
        for (std::size_t i = 1; i < length; i++) {
            optional<type::Type> expected;
            if (definition.size() == 1) {
                expected = definition[0]->params.match(
                    [](const VarargsType& varargs) { return varargs.type; },
                    [&](const std::vector<type::Type>& params) { return params[i - 1]; }
                );
            }
        
            auto parsed = parseExpression(arrayMember(value, i), ParsingContext(ctx, i, expected));
            if (!parsed) {
                return parsed;
            }
            args.push_back(std::move(*parsed));
        }
        return CompoundExpressionRegistry::create(name, definition, std::move(args), ctx);
    }
};

} // namespace expression
} // namespace style
} // namespace mbgl
