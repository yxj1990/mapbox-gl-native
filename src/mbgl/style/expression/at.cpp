#include <mbgl/style/expression/at.hpp>

namespace mbgl {
namespace style {
namespace expression {

EvaluationResult At::evaluate(const EvaluationParameters& params) const {
    auto i = index->evaluate<double>(params);
    auto inputArray = input->evaluate<std::vector<Value>>(params);
    if (!i) {
        return i.error();
    }
    if (!inputArray) {
        return inputArray.error();
    }
    
    if (*i < 0 || *i >= inputArray->size()) {
        return EvaluationError {
            "Array index out of bounds: " + stringify(*i) +
            " > " + std::to_string(inputArray->size()) + "."
        };
    }
    if (*i != ceilf(*i)) {
        return EvaluationError {
            "Array index must be an integer, but found " + stringify(*i) + " instead."
        };
    }
    return inputArray->at(static_cast<std::size_t>(*i));
}

void At::accept(std::function<void(const Expression*)> visit) const {
    visit(this);
    index->accept(visit);
    input->accept(visit);
}

} // namespace expression
} // namespace style
} // namespace mbgl
