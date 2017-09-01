#include <mbgl/style/expression/in.hpp>

namespace mbgl {
namespace style {
namespace expression {

EvaluationResult In::evaluate(const EvaluationParameters& params) const {
    const EvaluationResult needleValue = needle->evaluate(params);
    if (!needleValue) {
        return needleValue.error();
    }
    
    const Result<std::vector<Value>> haystackValue = haystack->evaluate<std::vector<Value>>(params);
    if (!haystackValue) {
        return haystackValue.error();
    }
    
    type::Type needleType = typeOf(*needleValue);
    if (needleType == type::Object || needleType == type::Color || needleType.template is<type::Array>()) {
        return EvaluationError {
            R"("contains" does not support searching for values of type )" + toString(needleType) + "."
        };
    }
    
    return EvaluationResult(std::any_of(haystackValue->begin(),
                            haystackValue->end(),
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
