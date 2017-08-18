#pragma once

#include <array>
#include <vector>
#include <memory>
#include <mbgl/util/optional.hpp>
#include <mbgl/util/variant.hpp>
#include <mbgl/util/color.hpp>
#include <mbgl/style/expression/check_subtype.hpp>
#include <mbgl/style/expression/expression.hpp>
#include <mbgl/style/expression/type.hpp>
#include <mbgl/style/expression/value.hpp>
#include <mbgl/style/expression/parsing_context.hpp>
#include <mbgl/style/conversion.hpp>

#define expand_pack(...) (void) std::initializer_list<int>{((void)(__VA_ARGS__), 0)...};

namespace mbgl {
namespace style {
namespace expression {

struct VarargsType { type::Type type; };
template <typename T>
struct Varargs : std::vector<T> { using std::vector<T>::vector; };

struct SignatureBase {
    SignatureBase(type::Type result_, variant<std::vector<type::Type>, VarargsType> params_) :
        result(std::move(result_)),
        params(std::move(params_))
    {}
    virtual ~SignatureBase() = default;
    virtual std::unique_ptr<Expression> makeExpression(const std::string& name, std::vector<std::unique_ptr<Expression>>) const = 0;
    type::Type result;
    variant<std::vector<type::Type>, VarargsType> params;
};


class CompoundExpressionBase : public Expression {
public:
    CompoundExpressionBase(std::string name_, const SignatureBase& signature) :
        Expression(signature.result),
        name(std::move(name_)),
        params(signature.params)
    {}
    
    std::string getName() const { return name; }
    optional<std::size_t> getParameterCount() const {
        return params.match(
            [&](const VarargsType&) { return optional<std::size_t>(); },
            [&](const std::vector<type::Type>& p) -> optional<std::size_t> { return p.size(); }
        );
    }
    
private:
    std::string name;
    variant<std::vector<type::Type>, VarargsType> params;
};

template <typename Signature>
class CompoundExpression : public CompoundExpressionBase {
public:
    using Args = typename Signature::Args;
    
    CompoundExpression(const std::string& name_,
                       Signature signature_,
                       typename Signature::Args args_) :
        CompoundExpressionBase(name_, signature_),
        signature(signature_),
        args(std::move(args_))
    {}
    
    EvaluationResult evaluate(const EvaluationParameters& params) const override {
        return signature.apply(params, args);
    }
    
    void accept(std::function<void(const Expression*)> visitor) const override {
        visitor(this);
        for (const std::unique_ptr<Expression>& e : args) {
            e->accept(visitor);
        }
    }
    
private:
    Signature signature;
    typename Signature::Args args;
};


struct CompoundExpressionRegistry {
    using Definition = std::vector<std::unique_ptr<SignatureBase>>;
    static std::unordered_map<std::string, Definition> definitions;
    
    static ParseResult create(const std::string& name,
                              const Definition& definition,
                              std::vector<std::unique_ptr<Expression>> args,
                              ParsingContext ctx);
    
        
    static ParseResult create(const std::string& name,
                              std::vector<std::unique_ptr<Expression>> args,
                              ParsingContext ctx)
    {
        return create(name, definitions.at(name), std::move(args), ctx);
    }

};


} // namespace expression
} // namespace style
} // namespace mbgl
