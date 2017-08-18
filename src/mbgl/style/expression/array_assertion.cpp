#include <mbgl/style/expression/array_assertion.hpp>

namespace mbgl {
namespace style {
namespace expression {

EvaluationResult ArrayAssertion::evaluate(const EvaluationParameters& params) const {
    auto result = input->evaluate(params);
    if (!result) {
        return result.error();
    }
    type::Type expected = getType();
    type::Type actual = typeOf(*result);
    if (checkSubtype(expected, actual)) {
        return EvaluationError {
            "Expected value to be of type " + toString(expected) +
            ", but found " + toString(actual) + " instead."
        };
    }
    return *result;
}

void ArrayAssertion::accept(std::function<void(const Expression*)> visit) const {
    visit(this);
    input->accept(visit);
}


} // namespace expression
} // namespace style
} // namespace mbgl
