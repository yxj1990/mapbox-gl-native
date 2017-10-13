#pragma once

#include <memory>
#include <mbgl/style/conversion.hpp>
#include <mbgl/style/expression/expression.hpp>
#include <mbgl/style/expression/parsing_context.hpp>


namespace mbgl {
namespace style {
namespace expression {

using namespace mbgl::style;

/*
    Parse the given style-spec JSON value into an Expression object.
    Specifically, this function is responsible for determining the expression
    type (either Literal, or the one named in value[0]) and dispatching to the
    appropriate ParseXxxx::parse(const V&, ParsingContext) method.
*/
ParseResult parseExpression(const mbgl::style::conversion::Value& value, ParsingContext context);


} // namespace expression
} // namespace style
} // namespace mbgl
