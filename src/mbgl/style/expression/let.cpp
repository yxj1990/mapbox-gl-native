#include <mbgl/style/expression/let.hpp>

namespace mbgl {
namespace style {
namespace expression {

EvaluationResult Let::evaluate(const EvaluationParameters& params) const {
    return result->evaluate(params);
}

void Let::accept(std::function<void(const Expression*)> visit) const {
    visit(this);
    for (auto it = bindings.begin(); it != bindings.end(); it++) {
        it->second->accept(visit);
    }
    result->accept(visit);
}

EvaluationResult Var::evaluate(const EvaluationParameters& params) const {
    return value->evaluate(params);
}

void Var::accept(std::function<void(const Expression*)> visit) const {
    visit(this);
}


} // namespace expression
} // namespace style
} // namespace mbgl
