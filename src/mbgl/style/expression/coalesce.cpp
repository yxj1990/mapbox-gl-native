#include <mbgl/style/expression/coalesce.hpp>

namespace mbgl {
namespace style {
namespace expression {

EvaluationResult Coalesce::evaluate(const EvaluationParameters& params) const {
    for (auto it = args.begin(); it != args.end(); it++) {
        const auto& result = (*it)->evaluate(params);
        if (!result && (std::next(it) != args.end())) {
            continue;
        }
        return result;
    }
    
    return Null;
}

void Coalesce::accept(std::function<void(const Expression*)> visit) const {
    visit(this);
    for (const std::unique_ptr<Expression>& arg : args) {
        arg->accept(visit);
    }
}

} // namespace expression
} // namespace style
} // namespace mbgl
