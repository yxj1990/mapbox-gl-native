#include <mbgl/style/expression/case.hpp>

namespace mbgl {
namespace style {
namespace expression {

EvaluationResult Case::evaluate(const EvaluationParameters& params) const {
    for (const auto& branch : branches) {
        const auto& condition = branch.first->template evaluate<bool>(params);
        if (!condition) {
            return condition.error();
        }
        if (*condition) {
            return branch.second->evaluate(params);
        }
    }
    
    return otherwise->evaluate(params);
}

void Case::accept(std::function<void(const Expression*)> visit) const {
    visit(this);
    for (const Branch& branch : branches) {
        branch.first->accept(visit);
        branch.second->accept(visit);
    }
    otherwise->accept(visit);
}



} // namespace expression
} // namespace style
} // namespace mbgl
