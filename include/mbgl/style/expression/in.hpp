#pragma once

#include <mbgl/style/expression/expression.hpp>

namespace mbgl {
namespace style {
namespace expression {

class In : public Expression {
public:
    In(std::unique_ptr<Expression> needle_, std::unique_ptr<Expression> haystack_) :
        Expression(type::Boolean),
        needle(std::move(needle_)),
        haystack(std::move(haystack_))
    {}
    
    EvaluationResult evaluate(const EvaluationParameters& params) const override;
    void accept(std::function<void(const Expression*)>) const override;

private:
    std::unique_ptr<Expression> needle;
    std::unique_ptr<Expression> haystack;
};

} // namespace expression
} // namespace style
} // namespace mbgl
