#ifndef LAZY_CORE_DECLS_HPP
#define LAZY_CORE_DECLS_HPP


#include "patterns/patterns.hpp"
#include "tags.hpp"
#include <tuple>


// ============================================================================
// Utility macros
// ============================================================================


#define LAZY_THIS static_cast<std::conditional_t<std::is_void_v<Derived>, \
    std::remove_reference_t<decltype(*this)>, \
    lazy::detail::copy_const_t<std::remove_reference_t<decltype(*this)>, Derived>>*>(this)

#define LAZY_FORCE_INLINE __attribute__((always_inline)) inline

#define LAZY_DEFINE_ALLOWED_IMPLICIT_CONVERSION(T, ...) \
template<> \
struct lazy::ValidTypes<T> { \
    using type = lazy::detail::TypeList<__VA_ARGS__>; \
};

/**
 * @brief Specialise `std::numeric_limits` for `LazyType<TYPE>` by inheriting from
 *        `std::numeric_limits<TYPE>`.
 *
 * Without this, `std::numeric_limits<LazyType<double>>` is empty (all members zero /
 * false), which can break generic numerical code that queries limits.  Call this macro
 * once per numeric type `TYPE` at namespace scope, typically just below the include of
 * this header.
 *
 * @param TYPE  The underlying arithmetic type (e.g. `double`, `mpfr::mpreal`).
 */
#define LAZY_DECLARE_NUMERIC_TYPE(TYPE) \
namespace std {\
template<>\
class numeric_limits<lazy::detail::LazyType<TYPE>> : public numeric_limits<TYPE>{};\
} \
template<typename F> \
constexpr bool lazy::traits::lazyConvertCondition<F, TYPE> = std::is_arithmetic_v<std::decay_t<F>>;



namespace lazy::detail {


// ============================================================================
// Forward declarations
// ============================================================================

/**
 * @brief Satisfied when `Derived` (after decay) derives from `ExprBase<T>`.
 *
 * This is the root concept for the entire expression-template hierarchy.  Any type
 * that participates in lazy arithmetic must satisfy this concept for the same `T`
 * that its `lazy_value_type` alias names.
 *
 * @tparam Derived The candidate expression type (references and cv-qualifiers are stripped).
 * @tparam T       The underlying arithmetic value type (e.g. `double`, `mpfr::mpreal`).
 */



// ============================================================================
// lazy_value_type trait  —  extracts the value type from an expression
// ============================================================================

/**
 * @brief Primary trait: extract the `lazy_value_type` alias from an expression type `T`.
 *
 * Defaults to `void` when `T` does not have a nested `::lazy_value_type` alias.  The
 * partial specialisation below overrides this for all expression types.
 *
 * @tparam T Candidate type (may or may not be an expression).
 */
template<typename T>
struct value_typeTrait {
    using Type = void;
};

/// @brief Specialisation for types that do expose `::lazy_value_type`.
template<typename T>
requires (requires {typename std::decay_t<T>::lazy_value_type;})
struct value_typeTrait<T> {
    using Type = typename std::decay_t<T>::lazy_value_type;
};

/// @brief Convenience alias: `lazy_value_type<E>` == `value_typeTrait<E>::Type`.
template<typename E>
using lazy_value_type = typename value_typeTrait<E>::Type;


/**
 * @brief Derives the common `lazy_value_type` from a pair of operand types.
 *
 * Rules:
 * - If both `A` and `B` expose a `lazy_value_type` and they are the same, the result is that type.
 * - If only one side has a `lazy_value_type` (the other is `void`, e.g. a raw scalar), that
 *   side's `lazy_value_type` is used.
 * - It is ill-formed for **both** to have `void` `lazy_value_type` (neither would be an expression).
 *
 * @tparam A Left operand type.
 * @tparam B Right operand type.
 */
template<typename A, typename B>
requires ((std::is_same_v<lazy_value_type<A>, lazy_value_type<B>> || std::is_same_v<lazy_value_type<A>, void> || std::is_same_v<lazy_value_type<B>, void>) && !(std::is_same_v<lazy_value_type<A>, void> && std::is_same_v<lazy_value_type<B>, void>))
using value_typeOf = std::conditional_t<std::is_same_v<lazy_value_type<A>, void>, lazy_value_type<B>, lazy_value_type<A>>;




template<typename T> struct ExprBase;
template<typename Derived, typename T> struct Expr;
template<typename Derived, typename T> struct Atom;
template<typename Derived, typename T, typename... branch_t> struct Node;
template<typename T> struct RefType;
template<typename T, typename Type> struct OtherType;
template<typename T> struct LazyType;


template<typename T, typename R>
LAZY_FORCE_INLINE decltype(auto) make_expr(R&& value);

template<typename F>
LAZY_FORCE_INLINE const auto& get_value(F&& value);

template<typename From, typename To>
using copy_const_t = std::conditional_t<std::is_const_v<From>, const To, To>;



template<typename T, lazy::traits::isTag F, typename... Args>
struct TypeGetter{
    using type = void;
};




} // namespace lazy::detail



namespace lazy::traits {

// ============================================================================
// Concepts
// ============================================================================

//=============================================================================

template<typename... F>
struct TypeList {};

template<typename T>
struct ValidTypes{
    using type = lazy::traits::TypeList<>;
};

template<typename T, typename List>
struct type_list_contains;

template<typename T, typename... Ts>
struct type_list_contains<T, lazy::traits::TypeList<Ts...>> : std::bool_constant<(std::is_same_v<T, Ts> || ...)> {};

template<typename Class, typename T>
constexpr bool lazyConvertCondition = lazy::traits::type_list_contains<std::decay_t<Class>, typename ValidTypes<T>::type>::value;

//=============================================================================


// Helpers for identifying a Node expression
template<typename F, typename T, typename BranchTuple>
struct NodeBaseCheck;

template<typename F, typename T, typename... Branches>
struct NodeBaseCheck<F, T, std::tuple<Branches...>> {
    static constexpr bool value = std::is_base_of_v<lazy::detail::Node<F, T, Branches...>, F>;
};

template<typename F>
struct HelperNodeIndentifier{
    static constexpr bool value = false;
};

template<typename F>
requires (requires {typename std::decay_t<F>::branch_t; typename std::decay_t<F>::lazy_value_type;})
struct HelperNodeIndentifier<F>{
    using D = std::decay_t<F>;
    static constexpr bool value = NodeBaseCheck<D, typename D::lazy_value_type, typename D::branch_t>::value;
};

template<typename Class, typename T>
concept isConvertibleTo = lazyConvertCondition<Class, T>;

template<typename Derived, typename T>
concept isLazyExpr = std::is_base_of_v<lazy::detail::ExprBase<T>, std::decay_t<Derived>>;

template<typename F, typename T>
concept isValidScalar = isConvertibleTo<F, T> || std::is_same_v<std::decay_t<F>, T>;

template<typename F, typename T>
concept isValidType = isLazyExpr<F, T> || isValidScalar<F, T>;

template<typename Derived, typename T>
concept isNode = HelperNodeIndentifier<std::decay_t<Derived>>::value;

template<typename Derived, typename T>
concept isAtom =std::is_base_of_v<lazy::detail::Atom<std::decay_t<Derived>, T>, std::decay_t<Derived>>;

template<typename Derived, typename T>
concept isRef = std::is_base_of_v<lazy::detail::RefType<T>, std::decay_t<Derived>>;

template<typename Derived, typename T>
concept isLazy = std::is_base_of_v<lazy::detail::LazyType<T>, std::decay_t<Derived>>;

template<typename F>
concept isAnyLazyExpr = requires { typename std::decay_t<F>::lazy_value_type; } && isLazyExpr<std::decay_t<F>, typename std::decay_t<F>::lazy_value_type>;

} // namespace lazy::traits



#endif // LAZY_CORE_DECLS_HPP