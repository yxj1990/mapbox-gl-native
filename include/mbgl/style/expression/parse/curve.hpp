#pragma once

#include <map>
#include <memory>
#include <mbgl/util/optional.hpp>
#include <mbgl/style/expression/curve.hpp>
#include <mbgl/style/expression/parsing_context.hpp>
#include <mbgl/style/conversion.hpp>

namespace mbgl {
namespace style {
namespace expression {

struct ParseCurve {

    using Interpolator = variant<StepInterpolator,
                                 ExponentialInterpolator,
                                 CubicBezierInterpolator>;

    template <typename V>
    static ParseResult parse(const V& value, ParsingContext ctx) {
        using namespace mbgl::style::conversion;
        assert(isArray(value));
        auto length = arrayLength(value);
        if (length < 5) {
            ctx.error("Expected at least 4 arguments, but found only " + std::to_string(length - 1) + ".");
            return ParseResult();
        }
        
        // [curve, interp, input, 2 * (n pairs)...]
        if (length % 2 != 1) {
            ctx.error("Expected an even number of arguments.");
            return ParseResult();
        }
        
        const V& interp = arrayMember(value, 1);
        if (!isArray(interp) || arrayLength(interp) == 0) {
            ctx.error("Expected an interpolation type expression.");
            return ParseResult();
        }

        Interpolator interpolator;
        
        const optional<std::string> interpName = toString(arrayMember(interp, 0));
        ParsingContext interpContext = ParsingContext(ctx, 1);
        if (interpName && *interpName == "step") {
            interpolator = StepInterpolator();
        } else if (interpName && *interpName == "linear") {
            interpolator = ExponentialInterpolator(1.0);
        } else if (interpName && *interpName == "exponential") {
            optional<double> base;
            if (arrayLength(interp) == 2) {
                base = toDouble(arrayMember(interp, 1));
            }
            if (!base) {
                interpContext.error("Exponential interpolation requires a numeric base.", 1);
                return ParseResult();
            }
            interpolator = ExponentialInterpolator(*base);
        } else if (interpName && *interpName == "cubic-bezier") {
            optional<double> x1;
            optional<double> y1;
            optional<double> x2;
            optional<double> y2;
            if (arrayLength(interp) == 5) {
                x1 = toDouble(arrayMember(interp, 1));
                y1 = toDouble(arrayMember(interp, 2));
                x2 = toDouble(arrayMember(interp, 3));
                y2 = toDouble(arrayMember(interp, 4));
            }
            if (
                !x1 || !y1 || !x2 || !y2 ||
                *x1 < 0 || *x1 > 1 ||
                *y1 < 0 || *y1 > 1 ||
                *x2 < 0 || *x2 > 1 ||
                *y2 < 0 || *y2 > 1
            ) {
                interpContext.error("Cubic bezier interpolation requires four numeric arguments with values between 0 and 1.");
                return ParseResult();
                
            }
            interpolator = CubicBezierInterpolator(*x1, *y1, *x2, *y2);
        } else {
            interpContext.error("Unknown interpolation type " + (interpName ? *interpName : ""), 0);
            return ParseResult();
        }
        
        ParseResult input = parseExpression(arrayMember(value, 2), ParsingContext(ctx, 2, {type::Number}));
        if (!input) {
            return input;
        }
        
        std::map<double, std::unique_ptr<Expression>> stops;
        optional<type::Type> outputType = ctx.expected;
        
        double previous = - std::numeric_limits<double>::infinity();
        for (std::size_t i = 3; i + 1 < length; i += 2) {
            const optional<mbgl::Value> labelValue = toValue(arrayMember(value, i));
            optional<double> label;
            optional<std::string> labelError;
            if (labelValue) {
                labelValue->match(
                    [&](uint64_t n) {
                        if (n > std::numeric_limits<double>::max()) {
                            label = {std::numeric_limits<double>::infinity()};
                        } else {
                            label = {static_cast<double>(n)};
                        }
                    },
                    [&](int64_t n) {
                        if (n > std::numeric_limits<double>::max()) {
                            label = {std::numeric_limits<double>::infinity()};
                        } else {
                            label = {static_cast<double>(n)};
                        }
                    },
                    [&](double n) {
                        if (n > std::numeric_limits<double>::max()) {
                            label = {std::numeric_limits<double>::infinity()};
                        } else {
                            label = {static_cast<double>(n)};
                        }
                    },
                    [&](const auto&) {}
                );
            }
            if (!label) {
                ctx.error(labelError ? *labelError :
                    R"(Input/output pairs for "curve" expressions must be defined using literal numeric values (not computed expressions) for the input values.)",
                    i);
                return ParseResult();
            }
            
            if (*label < previous) {
                ctx.error(
                    R"(Input/output pairs for "curve" expressions must be arranged with input values in strictly ascending order.)",
                    i
                );
                return ParseResult();
            }
            previous = *label;
            
            auto output = parseExpression(arrayMember(value, i + 1), ParsingContext(ctx, i + 1, outputType));
            if (!output) {
                return ParseResult();
            }
            if (!outputType) {
                outputType = (*output)->getType();
            }

            stops.emplace(*label, std::move(*output));
        }
        
        assert(outputType);
        
        if (
            !interpolator.template is<StepInterpolator>() &&
            *outputType != type::Number &&
            *outputType != type::Color &&
            !(
                outputType->is<type::Array>() &&
                outputType->get<type::Array>().itemType == type::Number
            )
        )
        {
            ctx.error("Type " + toString(*outputType) +
                " is not interpolatable, and thus cannot be used as a " +
                *interpName + " curve's output type.");
            return ParseResult();
        }
        
        return outputType->match(
            [&](const type::NumberType&) -> ParseResult {
                return interpolator.match([&](const auto& interpolator_) {
                    return ParseResult(std::make_unique<Curve<double>>(
                        *outputType, interpolator_, std::move(*input), std::move(stops)
                    ));
                });
            },
            [&](const type::ColorType&) -> ParseResult {
                return interpolator.match([&](const auto& interpolator_) {
                    return ParseResult(std::make_unique<Curve<mbgl::Color>>(
                        *outputType, interpolator_, std::move(*input), std::move(stops)
                    ));
                });
            },
            [&](const type::Array& arrayType) -> ParseResult {
                return interpolator.match(
                    [&](const StepInterpolator& stepInterpolator) {
                        return ParseResult(std::make_unique<Curve<std::vector<Value>>>(
                            *outputType, stepInterpolator, std::move(*input), std::move(stops)
                        ));
                    },
                    [&](const auto& continuousInterpolator) {
                        if (arrayType.itemType != type::Number || !arrayType.N) {
                            assert(false); // interpolability already checked above.
                            return ParseResult();
                        }
                        return ParseResult(std::make_unique<Curve<std::vector<Value>>>(
                            *outputType, continuousInterpolator, std::move(*input), std::move(stops)
                        ));
                    }
                );
            },
            [&](const auto&) {
                // Null, Boolean, String, Object, Value output types only support step interpolation
                return interpolator.match(
                    [&](const StepInterpolator& stepInterpolator) {
                        return ParseResult(std::make_unique<Curve<double>>(
                            *outputType, stepInterpolator, std::move(*input), std::move(stops)
                        ));
                    },
                    [&](const auto&) {
                        assert(false); // interpolability already checked above.
                        return ParseResult();
                    }
                );
            }
        );
    }
};



} // namespace expression
} // namespace style
} // namespace mbgl
