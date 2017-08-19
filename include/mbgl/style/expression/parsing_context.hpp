#pragma once

#include <map>
#include <string>
#include <vector>
#include <mbgl/util/optional.hpp>
#include <mbgl/style/expression/type.hpp>

namespace mbgl {
namespace style {
namespace expression {

struct ParsingError {
    std::string message;
    std::string key;
};

class Expression;

namespace detail {

struct Scope {
    Scope(const std::map<std::string, std::shared_ptr<Expression>>& bindings_, std::shared_ptr<Scope> parent_ = nullptr) :
        bindings(bindings_),
        parent(std::move(parent_))
    {}

    const std::map<std::string, std::shared_ptr<Expression>>& bindings;
    std::shared_ptr<Scope> parent;
    
    optional<std::shared_ptr<Expression>> get(const std::string& name) {
        auto it = bindings.find(name);
        if (it != bindings.end()) {
            return {it->second};
        } else if (parent) {
            return parent->get(name);
        } else {
            return optional<std::shared_ptr<Expression>>();
        }
    }
};

} // namespace detail

class ParsingContext {
public:
    ParsingContext(std::vector<ParsingError>& errors_, optional<type::Type> expected_ = {})
        : errors(errors_),
          expected(std::move(expected_))
    {}
    
    ParsingContext(const ParsingContext& previous,
                   std::size_t index_,
                   optional<type::Type> expected_ = {})
        : key(previous.key + "[" + std::to_string(index_) + "]"),
          errors(previous.errors),
          expected(std::move(expected_)),
          scope(previous.scope)
    {}
    
    ParsingContext(const ParsingContext& previous,
                   std::size_t index_,
                   optional<type::Type> expected_,
                   const std::map<std::string, std::shared_ptr<Expression>>& bindings)
        : key(previous.key + "[" + std::to_string(index_) + "]"),
          errors(previous.errors),
          expected(std::move(expected_)),
          scope(std::make_shared<detail::Scope>(bindings, previous.scope))
    {}
    
    optional<std::shared_ptr<Expression>> getBinding(const std::string name) {
        if (!scope) return optional<std::shared_ptr<Expression>>();
        return scope->get(name);
    }

    void error(std::string message) {
        errors.push_back({message, key});
    }
    
    void  error(std::string message, std::size_t child) {
        errors.push_back({message, key + "[" + std::to_string(child) + "]"});
    }

    std::string key;
    std::vector<ParsingError>& errors;
    optional<type::Type> expected;
    std::shared_ptr<detail::Scope> scope;
};

} // namespace expression
} // namespace style
} // namespace mbgl
