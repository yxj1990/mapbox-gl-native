#pragma once

#include <map>
#include <mbgl/util/interpolate.hpp>
#include <mbgl/util/range.hpp>
#include <mbgl/util/unitbezier.hpp>
#include <mbgl/style/expression/expression.hpp>
#include <mbgl/style/expression/compound_expression.hpp>

namespace mbgl {
namespace style {
namespace expression {

class StepInterpolator {
public:
    double interpolationFactor(const Range<double>&, const double&) const {
        return 0;
    }
    
};

class ExponentialInterpolator {
public:
    ExponentialInterpolator(double base_) : base(base_) {}

    double base;
    
    double interpolationFactor(const Range<double>& inputLevels, const double& input) const {
        return util::interpolationFactor(base,
                                         Range<float> {
                                            static_cast<float>(inputLevels.min),
                                            static_cast<float>(inputLevels.max)
                                         },
                                         input);
    }
};

class CubicBezierInterpolator {
public:
    CubicBezierInterpolator(double x1_, double y1_, double x2_, double y2_) : ub(x1_, y1_, x2_, y2_) {}
    
    double interpolationFactor(const Range<double>& inputLevels, const double& input) const {
        return ub.solve(input / (inputLevels.max - inputLevels.min), 1e-6);
    }
    
    util::UnitBezier ub;
};

template <typename T>
class Curve : public Expression {
public:
    using Interpolator = std::conditional_t<
        util::Interpolatable<T>::value,
        variant<StepInterpolator,
                ExponentialInterpolator,
                CubicBezierInterpolator>,
        variant<StepInterpolator>>;
    
    Curve(const type::Type& type_,
          Interpolator interpolator_,
          std::unique_ptr<Expression> input_,
          std::map<double, std::unique_ptr<Expression>> stops_
    ) : Expression(type_),
        interpolator(std::move(interpolator_)),
        input(std::move(input_)),
        stops(std::move(stops_))
    {}
    
    EvaluationResult evaluate(const EvaluationParameters& params) const override {
        const Result<float> x = input->evaluate<float>(params);
        if (!x) { return x.error(); }
        
        if (stops.empty()) {
            return EvaluationError { "No stops in exponential curve." };
        }

        auto it = stops.upper_bound(*x);
        if (it == stops.end()) {
            return stops.rbegin()->second->evaluate(params);
        } else if (it == stops.begin()) {
            return stops.begin()->second->evaluate(params);
        } else {
            float t = interpolationFactor({ std::prev(it)->first, it->first }, *x);
            
            if (t == 0.0f) {
                return std::prev(it)->second->evaluate(params);
            }
            if (t == 1.0f) {
                return it->second->evaluate(params);
            }
            
            EvaluationResult lower = std::prev(it)->second->evaluate(params);
            if (!lower) {
                return lower.error();
            }
            EvaluationResult upper = it->second->evaluate(params);
            if (!upper) {
                return upper.error();
            }

            return interpolate({*lower, *upper}, t);
        }
        
    }
    
    void accept(std::function<void(const Expression*)> visit) const override {
        visit(this);
        input->accept(visit);
        
        for (auto it = stops.begin(); it != stops.end(); it++) {
            it->second->accept(visit);
        }
    }
    
    bool isZoomCurve() const {
        if (auto z = dynamic_cast<CompoundExpressionBase*>(input.get())) {
            return z->getName() == "zoom";
        }
        return false;
    }
    
    double interpolationFactor(const Range<double>& inputLevels, const double& inputValue) const {
        return interpolator.match(
            [&](const auto& interp) { return interp.interpolationFactor(inputLevels, inputValue); }
        );
    }
    
private:
    template <typename OutputType = T>
    static EvaluationResult interpolate(const Range<Value>&, const double&,
                                        typename std::enable_if<!util::Interpolatable<OutputType>::value>::type* = 0) {
        // Assume that Curve::evaluate() will always short circuit due to
        // interpolationFactor always returning 0.
        assert(false);
        return Null;
    }
    
    template <typename OutputType = T>
    static EvaluationResult interpolate(const Range<Value>& outputs, const double& t,
                                        typename std::enable_if<util::Interpolatable<OutputType>::value>::type* = 0) {
        optional<T> lower = fromExpressionValue<T>(outputs.min);
        if (!lower) {
            // TODO - refactor fromExpressionValue to return EvaluationResult<T> so as to
            // consolidate DRY up producing this error message.
            return EvaluationError {
                "Expected value to be of type " + toString(valueTypeToExpressionType<T>()) +
                ", but found " + toString(typeOf(outputs.min)) + " instead."
            };
        }
        const optional<T> upper = fromExpressionValue<T>(outputs.max);
        if (!upper) {
            return EvaluationError {
                "Expected value to be of type " + toString(valueTypeToExpressionType<T>()) +
                ", but found " + toString(typeOf(outputs.min)) + " instead."
            };
        }
        T result = util::interpolate(*lower, *upper, t);
        return toExpressionValue(result);
    }
    

    Interpolator interpolator;
    std::unique_ptr<Expression> input;
    std::map<double, std::unique_ptr<Expression>> stops;
};

} // namespace expression
} // namespace style
} // namespace mbgl
