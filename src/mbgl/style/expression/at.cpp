#include <mbgl/style/expression/at.hpp>

namespace mbgl {
namespace style {
namespace expression {

EvaluationResult At::evaluate(const EvaluationParameters& params) const {
    const EvaluationResult evaluatedIndex = index->evaluate(params);
    const EvaluationResult evaluatedInput = input->evaluate(params);
    if (!evaluatedIndex) {
        return evaluatedIndex.error();
    }
    if (!evaluatedInput) {
        return evaluatedInput.error();
    }
    
    const auto i = *fromExpressionValue<double>(*evaluatedIndex);
    const auto inputArray = *fromExpressionValue<std::vector<Value>>(*evaluatedInput);
    
    if (i < 0 || i >= inputArray.size()) {
        return EvaluationError {
            "Array index out of bounds: " + stringify(i) +
            " > " + std::to_string(inputArray.size()) + "."
        };
    }
    if (i != std::floor(i)) {
        return EvaluationError {
            "Array index must be an integer, but found " + stringify(i) + " instead."
        };
    }
    return inputArray[static_cast<std::size_t>(i)];
}

void At::accept(std::function<void(const Expression*)> visit) const {
    visit(this);
    index->accept(visit);
    input->accept(visit);
}

} // namespace expression
} // namespace style
} // namespace mbgl
