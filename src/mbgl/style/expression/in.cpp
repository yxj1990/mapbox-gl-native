#include <mbgl/style/expression/in.hpp>

namespace mbgl {
namespace style {
namespace expression {

EvaluationResult In::evaluate(const EvaluationParameters& params) const {
    const EvaluationResult needleValue = needle->evaluate(params);
    if (!needleValue) {
        return needleValue.error();
    }
    
    const EvaluationResult evaluatedHaystack = haystack->evaluate(params);
    if (!evaluatedHaystack) {
        return evaluatedHaystack.error();
    }
    
    const auto haystackValue = evaluatedHaystack->get<std::vector<Value>>();
    
    type::Type needleType = typeOf(*needleValue);
    if (needleType == type::Object || needleType == type::Color || needleType.template is<type::Array>()) {
        return EvaluationError {
            R"("contains" does not support searching for values of type )" + toString(needleType) + "."
        };
    }
    
    return EvaluationResult(std::any_of(haystackValue.begin(),
                            haystackValue.end(),
                            [&](const Value& v) { return v == *needleValue; }));
};

void In::accept(std::function<void(const Expression*)> visit) const {
    visit(this);
    needle->accept(visit);
    haystack->accept(visit);
};

} // namespace expression
} // namespace style
} // namespace mbgl
