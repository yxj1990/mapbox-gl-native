#include <mbgl/style/expression/match.hpp>

namespace mbgl {
namespace style {
namespace expression {

template <typename T>
void Match<T>::accept(std::function<void(const Expression*)> visit) const {
    visit(this);
    input->accept(visit);
    for (const std::pair<T, std::shared_ptr<Expression>>& branch : branches) {
        branch.second->accept(visit);
    }
    otherwise->accept(visit);
}

template<> EvaluationResult Match<std::string>::evaluate(const EvaluationParameters& params) const {
    const EvaluationResult inputValue = input->evaluate(params);
    if (!inputValue) {
        return inputValue.error();
    }

    auto it = branches.find(inputValue->get<std::string>());
    if (it != branches.end()) {
        return (*it).second->evaluate(params);
    }

    return otherwise->evaluate(params);
}

template<> EvaluationResult Match<int64_t>::evaluate(const EvaluationParameters& params) const {
    const EvaluationResult inputValue = input->evaluate(params);
    if (!inputValue) {
        return inputValue.error();
    }
    
    const auto numeric = inputValue->get<double>();
    int64_t rounded = std::floor(numeric);
    if (numeric == rounded) {
        auto it = branches.find(rounded);
        if (it != branches.end()) {
            return (*it).second->evaluate(params);
        }
    }
    
    return otherwise->evaluate(params);
}

template class Match<int64_t>;
template class Match<std::string>;

} // namespace expression
} // namespace style
} // namespace mbgl
