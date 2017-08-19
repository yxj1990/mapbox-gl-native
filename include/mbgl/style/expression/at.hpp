#pragma once

#include <mbgl/style/expression/expression.hpp>

namespace mbgl {
namespace style {
namespace expression {

class At : public Expression {
public:
    At(std::unique_ptr<Expression> index_, std::unique_ptr<Expression> input_) :
        Expression(input_->getType().get<type::Array>().itemType),
        index(std::move(index_)),
        input(std::move(input_))
    {}
    
    EvaluationResult evaluate(const EvaluationParameters& params) const override;
    void accept(std::function<void(const Expression*)>) const override;

private:
    std::unique_ptr<Expression> index;
    std::unique_ptr<Expression> input;
};

} // namespace expression
} // namespace style
} // namespace mbgl
