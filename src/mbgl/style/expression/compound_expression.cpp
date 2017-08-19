#include <mbgl/style/expression/compound_expression.hpp>
#include <mbgl/tile/geometry_tile_data.hpp>

namespace mbgl {
namespace style {
namespace expression {


template <typename T>
std::decay_t<T> get(const Value& value);

template <> Value get<Value const&>(const Value& value) {
    return value;
}
template <typename T> std::decay_t<T> get(const Value& value) {
    return value.get<std::decay_t<T>>();
}


template <class, class Enable = void>
struct Signature;

// Signature from a zoom- or property-dependent evaluation function:
// (const EvaluationParameters&, T1, T2, ...) => Result<U>,
// where T1, T2, etc. are the types of successfully-evaluated subexpressions.
template <class R, class... Params>
struct Signature<R (const EvaluationParameters&, Params...)> : SignatureBase {
    using Args = std::array<std::unique_ptr<Expression>, sizeof...(Params)>;
    
    Signature(R (*evaluate_)(const EvaluationParameters&, Params...)) :
        SignatureBase(
            valueTypeToExpressionType<std::decay_t<typename R::Value>>(),
            std::vector<type::Type> {valueTypeToExpressionType<std::decay_t<Params>>()...}
        ),
        evaluate(evaluate_)
    {}
    
    std::unique_ptr<Expression> makeExpression(const std::string& name,
                                               std::vector<std::unique_ptr<Expression>> args) const override {
        typename Signature::Args argsArray;
        std::copy_n(std::make_move_iterator(args.begin()), sizeof...(Params), argsArray.begin());
        return std::make_unique<CompoundExpression<Signature>>(name, *this, std::move(argsArray));
    }
    
    EvaluationResult apply(const EvaluationParameters& evaluationParameters, const Args& args) const {
        return applyImpl(evaluationParameters, args, std::index_sequence_for<Params...>{});
    }
    
private:
    template <std::size_t ...I>
    EvaluationResult applyImpl(const EvaluationParameters& evaluationParameters, const Args& args, std::index_sequence<I...>) const {
        const std::vector<EvaluationResult>& evaluated = {std::get<I>(args)->evaluate(evaluationParameters)...};
        for (const auto& arg : evaluated) {
            if(!arg) return arg.error();
        }
        // TODO: assert correct runtime type of each arg value
        const R& value = evaluate(evaluationParameters, get<Params>(*(evaluated.at(I)))...);
        if (!value) return value.error();
        return *value;
    }
    
    R (*evaluate)(const EvaluationParameters&, Params...);
};

// Signature from varargs evaluation function: (Varargs<T>) => Result<U>,
// where T is the type of each successfully-evaluated subexpression (Varargs<T> being
// an alias for vector<T>).
template <class R, typename T>
struct Signature<R (const Varargs<T>&)> : SignatureBase {
    using Args = std::vector<std::unique_ptr<Expression>>;
    
    Signature(R (*evaluate_)(const Varargs<T>&)) :
        SignatureBase(
            valueTypeToExpressionType<std::decay_t<typename R::Value>>(),
            VarargsType { valueTypeToExpressionType<T>() }
        ),
        evaluate(evaluate_)
    {}
    
    std::unique_ptr<Expression> makeExpression(const std::string& name,
                                               std::vector<std::unique_ptr<Expression>> args) const override  {
        return std::make_unique<CompoundExpression<Signature>>(name, *this, std::move(args));
    };
    
    EvaluationResult apply(const EvaluationParameters& evaluationParameters, const Args& args) const {
        Varargs<T> evaluated;
        for (const auto& arg : args) {
            const auto& evaluatedArg = arg->evaluate<T>(evaluationParameters);
            if(!evaluatedArg) return evaluatedArg.error();
            evaluated.push_back(*evaluatedArg);
        }
        const R& value = evaluate(evaluated);
        if (!value) return value.error();
        return *value;
    }
    
    R (*evaluate)(const Varargs<T>&);
};

// Signature from "pure" evaluation function: (T1, T2, ...) => Result<U>,
// where T1, T2, etc. are the types of successfully-evaluated subexpressions.
template <class R, class... Params>
struct Signature<R (Params...)> : SignatureBase {
    using Args = std::array<std::unique_ptr<Expression>, sizeof...(Params)>;
    
    Signature(R (*evaluate_)(Params...)) :
        SignatureBase(
            valueTypeToExpressionType<std::decay_t<typename R::Value>>(),
            std::vector<type::Type> {valueTypeToExpressionType<std::decay_t<Params>>()...}
        ),
        evaluate(evaluate_)
    {}
    
    EvaluationResult apply(const EvaluationParameters& evaluationParameters, const Args& args) const {
        return applyImpl(evaluationParameters, args, std::index_sequence_for<Params...>{});
    }
    
    std::unique_ptr<Expression> makeExpression(const std::string& name,
                                               std::vector<std::unique_ptr<Expression>> args) const override {
        typename Signature::Args argsArray;
        std::copy_n(std::make_move_iterator(args.begin()), sizeof...(Params), argsArray.begin());
        return std::make_unique<CompoundExpression<Signature>>(name, *this, std::move(argsArray));
    }

    R (*evaluate)(Params...);
private:
    template <std::size_t ...I>
    EvaluationResult applyImpl(const EvaluationParameters& evaluationParameters, const Args& args, std::index_sequence<I...>) const {
        const std::vector<EvaluationResult>& evaluated = {std::get<I>(args)->evaluate(evaluationParameters)...};
        for (const auto& arg : evaluated) {
            if(!arg) return arg.error();
        }
        // TODO: assert correct runtime type of each arg value
        const R& value = evaluate(get<Params>(*(evaluated.at(I)))...);
        if (!value) return value.error();
        return *value;
    }
};

template <class R, class... Params>
struct Signature<R (*)(Params...)>
    : Signature<R (Params...)>
{ using Signature<R (Params...)>::Signature; };

template <class T, class R, class... Params>
struct Signature<R (T::*)(Params...) const>
    : Signature<R (Params...)>
{ using Signature<R (Params...)>::Signature;  };

template <class T, class R, class... Params>
struct Signature<R (T::*)(Params...)>
    : Signature<R (Params...)>
{ using Signature<R (Params...)>::Signature; };

template <class Lambda>
struct Signature<Lambda, std::enable_if_t<std::is_class<Lambda>::value>>
    : Signature<decltype(&Lambda::operator())>
{ using Signature<decltype(&Lambda::operator())>::Signature; };


ParseResult CompoundExpressionRegistry::create(const std::string& name,
                              const Definition& definition,
                              std::vector<std::unique_ptr<Expression>> args,
                              ParsingContext ctx)
{
    std::vector<ParsingError> currentSignatureErrors;

    ParsingContext signatureContext(currentSignatureErrors);
    signatureContext.key = ctx.key;
    
    for (const auto& signature : definition) {
        currentSignatureErrors.clear();
        
        
        if (signature->params.is<std::vector<type::Type>>()) {
            const auto& params = signature->params.get<std::vector<type::Type>>();
            if (params.size() != args.size()) {
                signatureContext.error(
                    "Expected " + std::to_string(params.size()) +
                    " arguments, but found " + std::to_string(args.size()) + " instead."
                );
                continue;
            }

            for (std::size_t i = 0; i < args.size(); i++) {
                const auto& arg = args.at(i);
                checkSubtype(params.at(i), arg->getType(), ParsingContext(signatureContext, i + 1));
            }
        } else if (signature->params.is<VarargsType>()) {
            const auto& paramType = signature->params.get<VarargsType>().type;
            for (std::size_t i = 0; i < args.size(); i++) {
                const auto& arg = args.at(i);
                checkSubtype(paramType, arg->getType(), ParsingContext(signatureContext, i + 1));
            }
        }
        
        if (currentSignatureErrors.size() == 0) {
            return ParseResult(signature->makeExpression(name, std::move(args)));
        }
    }
    
    if (definition.size() == 1) {
        ctx.errors.insert(ctx.errors.end(), currentSignatureErrors.begin(), currentSignatureErrors.end());
    } else {
        std::string signatures;
        for (const auto& signature : definition) {
            signatures += (signatures.size() > 0 ? " | " : "");
            signature->params.match(
                [&](const VarargsType& varargs) {
                    signatures += "(" + toString(varargs.type) + ")";
                },
                [&](const std::vector<type::Type>& params) {
                    signatures += "(";
                    bool first = true;
                    for (const type::Type& param : params) {
                        if (!first) signatures += ", ";
                        signatures += toString(param);
                        first = false;
                    }
                    signatures += ")";
                }
            );
            
        }
        std::string actualTypes;
        for (const auto& arg : args) {
            if (actualTypes.size() > 0) {
                actualTypes += ", ";
            }
            actualTypes += toString(arg->getType());
        }
        ctx.error("Expected arguments of type " + signatures + ", but found (" + actualTypes + ") instead.");
    }
    
    return ParseResult();
}

using Definition = CompoundExpressionRegistry::Definition;

// Helper for creating expression Definigion from one or more lambdas
template <typename ...Evals, typename std::enable_if_t<sizeof...(Evals) != 0, int> = 0>
static std::pair<std::string, Definition> define(std::string name, Evals... evalFunctions) {
    Definition definition;
    expand_pack(definition.push_back(std::make_unique<Signature<Evals>>(evalFunctions)));
    const auto& t0 = definition.at(0)->result;
    for (const auto& signature : definition) {
        assert(t0 == signature->result);
        (void)signature;
        (void)t0;
    }
    return std::pair<std::string, Definition>(name, std::move(definition));
}

template <typename T>
Result<T> assertion(const Value& v) {
    if (!v.is<T>()) {
        return EvaluationError {
            "Expected value to be of type " + toString(valueTypeToExpressionType<T>()) +
            ", but found " + toString(typeOf(v)) + " instead."
        };
    }
    return v.get<T>();
}

std::string stringifyColor(double r, double g, double b, double a) {
    return stringify(r) + ", " +
        stringify(g) + ", " +
        stringify(b) + ", " +
        stringify(a);
}
Result<mbgl::Color> rgba(double r, double g, double b, double a) {
    if (
        r < 0 || r > 255 ||
        g < 0 || g > 255 ||
        b < 0 || b > 255
    ) {
        return EvaluationError {
            "Invalid rgba value [" + stringifyColor(r, g, b, a)  +
            "]: 'r', 'g', and 'b' must be between 0 and 255."
        };
    }
    if (a < 0 || a > 1) {
        return EvaluationError {
            "Invalid rgba value [" + stringifyColor(r, g, b, a)  +
            "]: 'a' must be between 0 and 1."
        };
    }
    return mbgl::Color(r / 255, g / 255, b / 255, a);
}

template <typename T>
Result<bool> equal(const T& lhs, const T& rhs) { return lhs == rhs; }

template <typename T>
Result<bool> notEqual(const T& lhs, const T& rhs) { return lhs != rhs; }


template <typename ...Entries>
std::unordered_map<std::string, CompoundExpressionRegistry::Definition> initializeDefinitions(Entries... entries) {
    std::unordered_map<std::string, CompoundExpressionRegistry::Definition> definitions;
    expand_pack(definitions.insert(std::move(entries)));
    return definitions;
}

std::unordered_map<std::string, Definition> CompoundExpressionRegistry::definitions = initializeDefinitions(
    define("e", []() -> Result<double> { return 2.718281828459045f; }),
    define("pi", []() -> Result<double> { return 3.141592653589793f; }),
    define("ln2", []() -> Result<double> { return 0.6931471805599453; }),

    define("typeof", [](const Value& v) -> Result<std::string> { return toString(typeOf(v)); }),
    define("number", assertion<double>),
    define("string", assertion<std::string>),
    define("boolean", assertion<bool>),
    define("object", assertion<std::unordered_map<std::string, Value>>),
    
    define("to_string", [](const Value& v) -> Result<std::string> { return stringify(v); }),
    define("to_number", [](const Value& v) -> Result<double> {
        optional<double> result = v.match(
            [](const double f) -> optional<double> { return f; },
            [](const std::string& s) -> optional<double> {
                try {
                    return std::stof(s);
                } catch(std::exception) {
                    return optional<double>();
                }
            },
            [](const auto&) { return optional<double>(); }
        );
        if (!result) {
            return EvaluationError {
                "Could not convert " + stringify(v) + " to number."
            };
        }
        return *result;
    }),
    define("to_boolean", [](const Value& v) -> Result<bool> {
        return v.match(
            [&] (double f) { return (bool)f; },
            [&] (const std::string& s) { return s.length() > 0; },
            [&] (bool b) { return b; },
            [&] (const NullValue&) { return false; },
            [&] (const auto&) { return true; }
        );
    }),
    define("to_rgba", [](const mbgl::Color& color) -> Result<std::array<double, 4>> {
        return std::array<double, 4> {{ color.r, color.g, color.b, color.a }};
    }),
    
    define("parse_color", [](const std::string& colorString) -> Result<mbgl::Color> {
        const auto& result = mbgl::Color::parse(colorString);
        if (result) return *result;
        return EvaluationError {
            "Could not parse color from value '" + colorString + "'"
        };
    }),
    
    define("rgba", rgba),
    define("rgb", [](double r, double g, double b) { return rgba(r, g, b, 1.0f); }),
    
    define("zoom", [](const EvaluationParameters& params) -> Result<double> {
        if (!params.zoom) {
            return EvaluationError {
                "The 'zoom' expression is unavailable in the current evaluation context."
            };
        }
        return *(params.zoom);
    }),
    
    define("has", [](const EvaluationParameters& params, const std::string& key) -> Result<bool> {
        if (!params.feature) {
            return EvaluationError {
                "Feature data is unavailable in the current evaluation context."
            };
        }
        
        return params.feature->getValue(key) ? true : false;
    }, [](const std::string& key, const std::unordered_map<std::string, Value>& object) -> Result<bool> {
        return object.find(key) != object.end();
    }),
    
    define("get", [](const EvaluationParameters& params, const std::string& key) -> Result<Value> {
        if (!params.feature) {
            return EvaluationError {
                "Feature data is unavailable in the current evaluation context."
            };
        }

        auto propertyValue = params.feature->getValue(key);
        if (!propertyValue) {
            return EvaluationError {
                "Property '" + key + "' not found in feature.properties"
            };
        }
        return Value(toExpressionValue(*propertyValue));
    }, [](const std::string& key, const std::unordered_map<std::string, Value>& object) -> Result<Value> {
        if (object.find(key) == object.end()) {
            return EvaluationError {
                "Property '" + key + "' not found in object"
            };
        }
        return object.at(key);
    }),
    
    define("length", [](const std::vector<Value>& arr) -> Result<double> {
        return arr.size();
    }, [] (const std::string s) -> Result<double> {
        return s.size();
    }),
    
    define("properties", [](const EvaluationParameters& params) -> Result<std::unordered_map<std::string, Value>> {
        if (!params.feature) {
            return EvaluationError {
                "Feature data is unavailable in the current evaluation context."
            };
        }
        std::unordered_map<std::string, Value> result;
        const auto& properties = params.feature->getProperties();
        for (const auto& entry : properties) {
            result[entry.first] = toExpressionValue(entry.second);
        }
        return result;
    }),
    
    define("geometry_type", [](const EvaluationParameters& params) -> Result<std::string> {
        if (!params.feature) {
            return EvaluationError {
                "Feature data is unavailable in the current evaluation context."
            };
        }
    
        auto type = params.feature->getType();
        if (type == FeatureType::Point) {
            return "Point";
        } else if (type == FeatureType::LineString) {
            return "LineString";
        } else if (type == FeatureType::Polygon) {
            return "Polygon";
        } else {
            return "Unknown";
        }
    }),
    
    define("id", [](const EvaluationParameters& params) -> Result<Value> {
        if (!params.feature) {
            return EvaluationError {
                "Feature data is unavailable in the current evaluation context."
            };
        }
    
        auto id = params.feature->getID();
        if (!id) {
            return EvaluationError {
                "Property 'id' not found in feature"
            };
        }
        return id->match(
            [](const auto& idValue) {
                return toExpressionValue(mbgl::Value(idValue));
            }
        );
    }),
    
    define("+", [](const Varargs<double>& args) -> Result<double> {
        double sum = 0.0f;
        for (auto arg : args) {
            sum += arg;
        }
        return sum;
    }),
    define("-", [](double a, double b) -> Result<double> { return a - b; }),
    define("*", [](const Varargs<double>& args) -> Result<double> {
        double prod = 1.0f;
        for (auto arg : args) {
            prod *= arg;
        }
        return prod;
    }),
    define("/", [](double a, double b) -> Result<double> { return a / b; }),
    define("%", [](double a, double b) -> Result<double> { return fmod(a, b); }),
    define("^", [](double a, double b) -> Result<double> { return pow(a, b); }),
    define("log10", [](double x) -> Result<double> { return log10(x); }),
    define("ln", [](double x) -> Result<double> { return log(x); }),
    define("log2", [](double x) -> Result<double> { return log2(x); }),
    define("sin", [](double x) -> Result<double> { return sin(x); }),
    define("cos", [](double x) -> Result<double> { return cos(x); }),
    define("tan", [](double x) -> Result<double> { return tan(x); }),
    define("asin", [](double x) -> Result<double> { return asin(x); }),
    define("acos", [](double x) -> Result<double> { return acos(x); }),
    define("atan", [](double x) -> Result<double> { return atan(x); }),
    
    define("min", [](const Varargs<double>& args) -> Result<double> {
        double result = std::numeric_limits<double>::infinity();
        for (double arg : args) {
            result = fmin(arg, result);
        }
        return result;
    }),
    define("max", [](const Varargs<double>& args) -> Result<double> {
        double result = -std::numeric_limits<double>::infinity();
        for (double arg : args) {
            result = fmax(arg, result);
        }
        return result;
    }),
    
    define("==", equal<double>, equal<bool>, equal<const std::string&>, equal<NullValue>),
    define("!=", notEqual<double>, notEqual<bool>, notEqual<const std::string&>, notEqual<NullValue>),
    define(">", [](double lhs, double rhs) -> Result<bool> { return lhs > rhs; }),
    define(">=", [](double lhs, double rhs) -> Result<bool> { return lhs >= rhs; }),
    define("<", [](double lhs, double rhs) -> Result<bool> { return lhs < rhs; }),
    define("<=", [](double lhs, double rhs) -> Result<bool> { return lhs <= rhs; }),
    
    define("||", [](const Varargs<bool>& args) -> Result<bool> {
        bool result = false;
        for (auto arg : args) result = result || arg;
        return result;
    }),
    define("&&", [](const Varargs<bool>& args) -> Result<bool> {
        bool result = true;
        for (bool arg : args) result = result && arg;
        return result;
    }),
    define("!", [](bool e) -> Result<bool> { return !e; }),
    
    define("upcase", [](const std::string& input) -> Result<std::string> {
        std::string s = input;
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c){ return std::toupper(c); });
        return s;
    }),
    define("downcase", [](const std::string& input) -> Result<std::string> {
        std::string s = input;
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        return s;
    }),
    define("concat", [](const Varargs<std::string>& args) -> Result<std::string> {
        std::string s;
        for (const std::string& arg : args) {
            s += arg;
        }
        return s;
    })
);

} // namespace expression
} // namespace style
} // namespace mbgl
