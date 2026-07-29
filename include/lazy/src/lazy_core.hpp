#ifndef LAZY_IMPL_HPP
#define LAZY_IMPL_HPP

#include <vector>
#include "lazy_decls.hpp"
#include <ostream>

namespace lazy{

namespace traits {

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

template<typename Class, typename T>
concept isConvertibleTo = lazyConvertCondition<Class, T>;

template<typename Derived, typename T>
concept isLazyExpr = std::is_base_of_v<lazy::detail::ExprBase<T>, std::decay_t<Derived>>;

template<typename F, typename T>
concept isValidScalar = isConvertibleTo<F, T> || std::is_same_v<std::decay_t<F>, T>;

template<typename F, typename T>
concept isValidType = isLazyExpr<F, T> || isValidScalar<F, T>;

template<typename Derived, typename T>
concept isNode = requires { std::decay_t<Derived>::Nbranches; } && std::is_base_of_v< lazy::detail::Node<std::decay_t<Derived>, T, std::decay_t<Derived>::Nbranches>, std::decay_t<Derived>>;

template<typename Derived, typename T>
concept isAtom =std::is_base_of_v<lazy::detail::Atom<std::decay_t<Derived>, T>, std::decay_t<Derived>>;

template<typename Derived, typename T>
concept isBinOp = requires
    {typename Derived::LhsType; typename Derived::RhsType;} &&
    std::is_base_of_v<lazy::detail::BinaryOperator<Derived, T, typename Derived::LhsType, typename Derived::RhsType>, Derived>;

template<typename Derived, typename T>
concept isAdd = requires { requires isBinOp<Derived, T>; } && std::is_base_of_v<lazy::detail::Add<T, typename Derived::LhsType, typename Derived::RhsType>, Derived>;

template<typename Derived, typename T>
concept isSub = requires { requires isBinOp<Derived, T>; } && std::is_base_of_v<lazy::detail::Sub<T, typename Derived::LhsType, typename Derived::RhsType>, Derived>;

template<typename Derived, typename T>
concept isMul = requires { requires isBinOp<Derived, T>; } && std::is_base_of_v<lazy::detail::Mul<T, typename Derived::LhsType, typename Derived::RhsType>, Derived>;

template<typename Derived, typename T>
concept isDiv = requires { requires isBinOp<Derived, T>; } && std::is_base_of_v<lazy::detail::Div<T, typename Derived::LhsType, typename Derived::RhsType>, Derived>;

template<typename Derived, typename T>
concept isPow = requires { requires isBinOp<Derived, T>; } && std::is_base_of_v<lazy::detail::Pow<T, typename Derived::LhsType, typename Derived::RhsType>, Derived>;

template<typename Arg>
concept isTag = std::is_base_of_v<lazy::tags::Tag, std::decay_t<Arg>>;

template<typename Arg>
concept isBoolTag = std::is_base_of_v<lazy::tags::BOOL_TAG, std::decay_t<Arg>>;

template<typename Derived, typename T>
concept isRef = std::is_base_of_v<lazy::detail::RefType<T>, std::decay_t<Derived>>;

template<typename Derived, typename T>
concept isLazy = std::is_base_of_v<lazy::detail::LazyType<T>, std::decay_t<Derived>>;

template<typename F>
concept isAnyLazyExpr = requires { typename std::decay_t<F>::value_type; } && isLazyExpr<std::decay_t<F>, typename std::decay_t<F>::value_type>;

} // namespace traits


namespace detail{



// ============================================================================
// ExprBase — root of the expression hierarchy
// ============================================================================

/**
 * @brief Non-CRTP root base for all expression types parameterised on value type `T`.
 *
 * Deriving from `ExprBase<T>` opts a type into the `isLazyExpr<D,T>` concept and provides
 * service types used throughout the library:
 *
 * - `value_type` — the underlying arithmetic type (carries `T` into derived classes via
 *   the nested alias).
 * - `expr_storage_t<E>` — determines how sub-expressions are stored inside composite
 *   nodes: raw `T` values are stored as `const T&` (reference to an existing object),
 *   while sub-expression nodes are stored by value (so the entire tree is embedded
 *   inline without heap allocation).
 *   concrete types override this with a more specific pattern.
 *
 * @tparam T  The underlying arithmetic value type (e.g. `double`, `mpfr::mpreal`).
 */
template<typename T>
struct ExprBase{

    using value_type = T;

    template<typename E>
    struct expr_storage {
        // Default: store by const reference
        using type = const T&;
    };

    template<traits::isLazyExpr<T> E>
    struct expr_storage<E> {
        // For sub-expressions: store by value
        using type = std::decay_t<E>;
    };

    template<typename E>
    using expr_storage_t = typename expr_storage<E>::type;

};


// ============================================================================
// Expr — CRTP intermediate base
// ============================================================================

/**
 * @brief CRTP intermediate base that enriches `ExprBase<T>` with static membership flags
 *        and a compile-time `matches_pattern` check.
 *
 * Every concrete expression type inherits from `Expr<Derived, T>` (transitively via
 * `Atom` or `Node`) and overrides the relevant `static constexpr bool isXxx` flag to
 * `true`.  These flags are used in `BinaryOpRules` / `UnaryOpRules` to branch at
 * compile time without virtual dispatch.
 *
 * @tparam Derived The most-derived type (CRTP).
 * @tparam T       The underlying arithmetic value type.
 */
template<typename Derived, typename T>
struct Expr : public ExprBase<T> {

    using Base = ExprBase<T>;
    using value_type = T;

    static constexpr bool isAtom  = false;
    static constexpr bool isNode = false;
    static constexpr bool isBinaryOperator = false;
    static constexpr bool isUnary = false;
    static constexpr bool isAdd = false;
    static constexpr bool isMul = false;
    static constexpr bool isDiv = false;
    static constexpr bool isSub = false;
    static constexpr bool isPow = false;
    static constexpr bool isNeg = false;
    static constexpr bool isLazy = false;
    static constexpr bool isRef = false;

};

// ============================================================================
// Atom — leaf expression nodes (already evaluated)
// ============================================================================

/**
 * @brief CRTP base for leaf nodes that hold a value directly accessible via `value()`.
 *
 * Atoms never need temporary storage for evaluation.  The `value()` call simply returns
 * a reference to the stored or referenced `T` (or compatible) value.  Atoms also
 * provide an `operator T()` implicit conversion for ergonomic use in regular code.
 *
 * Concrete atom types:
 * - `RefType<T>` — holds a `const T&` (non-owning reference into an existing `LazyType`).
 * - `LazyType<T>` — owns a `T` value; the primary user-facing variable type.
 * - `OtherType<T,Type>` — wraps a scalar of a *different* type `Type` (e.g. `int` in a
 *   `double`-typed expression).
 *
 * @tparam Derived The most-derived type (CRTP).
 * @tparam T       The underlying arithmetic value type.
 */
template<typename Derived, typename T>
struct Atom : public Expr<Derived, T>{

    using Base = Expr<Derived, T>;
    using value_type = T;
    static constexpr bool isAtom = true;

    LAZY_FORCE_INLINE const auto& value() const{
        return LAZY_THIS->value();
    }

    operator T() const { return value(); }

};

// ============================================================================
// Node — unevaluated composite expression nodes
// ============================================================================

/**
 * @brief CRTP base for composite (unevaluated) expression nodes.
 *
 * A `Node` holds `Branches` sub-expressions and computes `T` only when `eval()` is
 * called.  The CRTP `eval()` call dispatches to `Derived::eval(T& out)` which
 * typically delegates to the relevant `eval_rule` in `BinaryOpRules` or
 * `UnaryOpRules`.
 *
 * The `operator T()` implicit conversion creates a temporary `T` and calls `eval()`
 * on it — use with care for types (like `mpfr::mpreal`) where default construction
 * is expensive or produces an indeterminate value.
 *
 * @tparam Derived   The most-derived type (CRTP).
 * @tparam T         The underlying arithmetic value type.
 * @tparam Branches  Number of child sub-expressions (1 for unary, 2 for binary, etc.).
 */
template<typename Derived, typename T, size_t Branches>
struct Node : public Expr<Derived, T>{

    using Base = Expr<Derived, T>;
    static constexpr bool isNode = true;
    static constexpr size_t Nbranches = Branches;

    inline static thread_local T tmp{};

    LAZY_FORCE_INLINE T& eval(T& out) const{
        return LAZY_THIS->eval(out);
    }

    operator T() const {
        return LAZY_THIS->eval(tmp);
    }

};

// ============================================================================
// BinaryOperator — stores lhs/rhs and dispatches to eval_rule
// ============================================================================

/**
 * @brief CRTP base for all two-operand expression nodes.
 *
 * Holds the left- and right-hand sub-expressions (by value, embedded inline) and
 * provides a concrete `eval(T& out)` that delegates to
 * `Derived::eval_rule(tag{}, out, make_expr<T>(lhs), make_expr<T>(rhs))`.
 *
 * Constructing a `BinaryOperator<Derived,T,L,R>` requires that both `L` and `R` are
 * constructible from the supplied arguments.  See `make_add`, `make_mul`, etc. for
 * the canonical factory functions that build these nodes.
 *
 * @tparam Derived The most-derived type (e.g. `Add<T,L,R>`).
 * @tparam T       The underlying arithmetic value type.
 * @tparam L       The type of the left-hand sub-expression (stored by value).
 * @tparam R       The type of the right-hand sub-expression (stored by value).
 */
template<typename Derived, typename T, typename L, typename R>
struct BinaryOperator : public Node<Derived, T, 2>{

    using Base = Node<Derived, T, 2>;
    using LhsType = L;
    using RhsType = R;
    static constexpr bool isBinaryOperator = true;
    
    template<typename L2, typename R2>
    requires (std::is_constructible_v<L, L2&&> && std::is_constructible_v<R, R2&&>)
    LAZY_FORCE_INLINE BinaryOperator(L2&& lhs, R2&& rhs) : lhs(std::forward<L2>(lhs)), rhs(std::forward<R2>(rhs)) {}

    LAZY_FORCE_INLINE T& eval(T& out) const{
        Derived::eval_rule(typename Derived::tag{}, out, make_expr<T>(lhs), make_expr<T>(rhs));
        return out;
    }

    L lhs; R rhs;
};


//=============================== Rules ==============================================

/**
 * @brief Global registry of `T*` scratch pointers used by `RuleTree`.
 *
 * Each `RuleTree<T, Branches...>` specialisation registers its thread-local
 * temporary `T` objects into this list so that external code can iterate over all
 * allocated temporaries (e.g. for bulk operations that need to touch all active
 * scratch buffers).
 *
 * @tparam T The arithmetic value type.
 */
template<typename T>
struct Rules{

    inline static thread_local std::vector<T*> aux_workers;

    template<typename Func>
    LAZY_FORCE_INLINE static void for_each_aux(Func&& fn) {
        for (T* p : aux_workers) {fn(*p);};
    }
};


/**
 * @brief Per-(type, operand-pattern) thread-local temporary storage pool.
 *
 * For each unique combination of value type `T` and branch expression types
 * `Branch...`, exactly `sizeof...(Branch)` `T` objects are allocated as
 * thread-local storage.  These serve as scratch buffers for intermediate results
 * during recursive `eval_rule` calls, avoiding any heap allocation on the hot path.
 *
 * The thread-local array is initialised lazily on first use; each element is
 * registered in `Rules<T>::aux_ptrs` so that external code can find all scratch
 * buffers of type `T`.
 *
 * @tparam T        The arithmetic value type.
 * @tparam Branch   The expression types of the sub-expressions for which scratch is
 *                  needed.  Must satisfy `isLazyExpr<Branch, T>` for each element.
 */
template<typename T, traits::isLazyExpr<T>... Branch>
struct RuleTree : public Rules<T>{

    static constexpr size_t Nb = sizeof...(Branch);
    using Base = Rules<T>;
    static_assert(sizeof...(Branch) > 0, "Branches must be greater than 0");

    inline static thread_local T* worker = [](){
        static thread_local std::array<T, Nb> arr;  // Allocate contiguous array
        Base::aux_workers.push_back(arr.data());
        return arr.data();
    }();

};

//============================== Binary Operation rules ==============================

/**
 * @brief Default evaluation policy for binary operations.
 *
 * `BinaryOpRules<Derived, T>` is CRTP-mixed into each concrete binary-operation node
 * (via `CustomBinaryRules<T>`) and provides a two-step evaluation strategy:
 *
 * 1. **`eval_rule(tag, out, a, b)`** — accepts two `isLazyExpr<T>` operands.  Recursively
 *    evaluates any `Node` sub-expression (using thread-local scratch from `RuleTree`)
 *    until both operands are atoms, then calls `evaluate`.
 * 2. **`evaluate(tag, out, a, b)`** — accepts raw values `a`, `b` of any type and
 *    writes the result to `out`.  This is the function to *specialise* when providing
 *    type-specific implementations (e.g. MPFR intrinsics).
 *
 * **Important:** `out`, `a`, and `b` may alias the same memory (the library performs
 * in-place updates of `LazyType<T>` variables).  Implementations of `evaluate` must
 * be safe under aliasing.
 *
 * @tparam Derived The concrete rules class (CRTP, typically `CustomBinaryRules<T>`).
 * @tparam T       The arithmetic value type.
 */
template<typename Derived, typename T>
struct BinaryOpRules{

    /**
    eval_rule takes as input T, while "a" and "b" are Expr types, not raw values like T or int, double etc.
    Override this for more control.

    On the other hand, evaluate takes as input raw values like T or int. These must be instanciated necessarily for core operations like addition, multiplication etc.
    */

    // IMPORTANT: a or b might be the same memory location as out.
    template<lazy::traits::isTag tag, typename L, typename R>
    inline static void evaluate(tag, T& out, const L& a, const R& b);

    template<lazy::traits::isTag tag, lazy::traits::isLazyExpr<T> L, lazy::traits::isLazyExpr<T> R>
    LAZY_FORCE_INLINE static void eval_rule(tag, T& out, const L& a, const R& b){
        // IMPORTANT: out may be the same memory location as a and b
        
        // make sure L and R are NOT LazyType (RefType should be passed intead ALWAYS)
        static_assert(!lazy::traits::isLazy<L, T> && !lazy::traits::isLazy<R, T>, "LazyType should not be passed to eval_rule, use RefType instead");

        // By default, eval_rule simply evaluates the sub-expressions and then calls evaluate with the raw values. Override this if you want to do something different, like short-circuiting for addition or multiplication.
        T* worker = RuleTree<T, L, R>::worker;
        if constexpr (lazy::traits::isNode<L, T>) {
            // no matter what R is, since this operation has not been overriden,
            // we need to evaluate one of the branches (convention: the left one)
            Derived::eval_rule(tag{}, out, make_expr<T>(a.eval(worker[0])), b);
        } else if constexpr (lazy::traits::isNode<R, T>) {
            Derived::evaluate(tag{}, out, a.value(), b.eval(worker[1]));
        } else {
            Derived::evaluate(tag{}, out, a.value(), b.value());
        }
    }
};


/**
 * @brief Default evaluation policy for unary operations.
 *
 * Mirrors `BinaryOpRules` for single-argument operations.  `eval_rule(tag, out, a)`
 * evaluates the argument if it is a `Node` (placing the result in the first slot of
 * `RuleTree<T, Arg>::aux`), then calls `evaluate(tag, out, raw_a)`.
 *
 * Specialise `evaluate` to provide type-specific unary implementations.
 *
 * @tparam Derived The concrete rules class (CRTP, typically `CustomUnaryRules<T>`).
 * @tparam T       The arithmetic value type.
 */
template<typename Derived, typename T>
struct UnaryOpRules{

    template<lazy::traits::isTag tag, typename Arg>
    inline static void evaluate(tag, T& out, const Arg& a);

    template<lazy::traits::isTag tag, lazy::traits::isLazyExpr<T> Arg>
    inline static void eval_rule(tag, T& out, const Arg& a){
        if constexpr (lazy::traits::isNode<Arg, T>) {
            T* worker = RuleTree<T, Arg>::worker;
            Derived::evaluate(tag{}, out, a.eval(worker[0]));
        } else {
            Derived::evaluate(tag{}, out, a.value());
        }
    }
};


/**
 * @brief Primary binary-operation rules for type `T`.
 *
 * A default-constructed `CustomBinaryRules<T>` provides no specialised `evaluate`
 * overloads.  Users should provide a full specialisation (via `LAZY_SPECIALIZE_OPERATIONS`)
 * for each numeric type `T` to define how `+`, `-`, `*`, `/`, `pow`, `max`, `min`
 * are computed.
 *
 * When no specialisation exists, the library falls back to the default
 * `BinaryOpRules` which uses the built-in operators on raw `T` values.
 *
 * @tparam T The arithmetic value type to specialise for.
 */
template<typename T>
struct CustomBinaryRules : public BinaryOpRules<CustomBinaryRules<T>, T>{};


/**
 * @brief Primary unary-operation rules for type `T`.
 *
 * Analogous to `CustomBinaryRules`.  Specialise (via `LAZY_SPECIALIZE_FUNCTIONS`) to
 * provide custom `evaluate` overloads for unary functions such as `abs`, `sqrt`,
 * `neg`, etc.
 *
 * @tparam T The arithmetic value type to specialise for.
 */
template<typename T>
struct CustomUnaryRules : public UnaryOpRules<CustomUnaryRules<T>, T>{};



/**
 * @brief CRTP base for single-argument (unary) expression nodes.
 *
 * Stores the single child expression `arg` by value and evaluates via
 * `Derived::eval_rule(tag{}, out, arg)`.  The `UnaryOpRules` default path
 * evaluates `arg` into a scratch slot if it is a `Node`, then calls
 * `Derived::evaluate(tag{}, out, raw_arg)`.
 *
 * @tparam Derived The concrete unary node type (e.g. `Neg<T,Arg>`).
 * @tparam T       The arithmetic value type.
 * @tparam Arg     The type of the single child sub-expression (`isLazyExpr<Arg,T>`).
 */
template<typename Derived, typename T, typename Arg>
struct Unary : public Node<Derived, T, 1>{

    static_assert(traits::isLazyExpr<Arg, T>, "Unary node argument must be an expression");

    using Base = Node<Derived, T, 1>;
    static constexpr bool isUnary = true;

    template<typename Arg2>
    requires( requires {typename std::decay_t<Arg>::value_type;} && traits::isLazyExpr<std::decay_t<Arg>, typename std::decay_t<Arg>::value_type> )
    LAZY_FORCE_INLINE Unary(Arg2&& arg) : arg(std::forward<Arg2>(arg)) {}

    LAZY_FORCE_INLINE T& eval(T& out) const{
        Derived::eval_rule(typename Derived::tag{}, out, arg);
        return out;
    }

    Arg arg;
};


/// @brief Expression node for unary negation: computes `-arg`.
/// @tparam T   The arithmetic value type.
/// @tparam Arg The child sub-expression type.
template<typename T, typename Arg>
struct Neg : public Unary<Neg<T, Arg>, T, Arg>, public CustomUnaryRules<T>{
    using Base = Unary<Neg<T, Arg>, T, Arg>;
    static constexpr bool isNeg = true;
    using tag = lazy::tags::NEG;
};

/**
 * @brief Atom that wraps a `T` value by const reference.
 *
 * `RefType<T>` is a lightweight non-owning view.  It is produced automatically by
 * `make_expr<T>()` when a `LazyType<T>` lvalue is used as an operand, so that the
 * expression tree references the original variable without copying it.  The
 * referenced value must not be modified or destroyed while the node is alive.
 *
 * between a reference and a direct value.
 *
 * @tparam T The arithmetic value type.
 */
template<typename T>
struct RefType : public Atom<RefType<T>, T>{
    using Base = Atom<RefType<T>, T>;


    static constexpr bool isRef = true;

    LAZY_FORCE_INLINE RefType(const T& value) : value_(value) {}

    LAZY_FORCE_INLINE const T& value() const {return value_;}

    Base::template expr_storage_t<T> value_;

};


/**
 * @brief Atom that wraps a value of a *different* scalar type `Type` inside a
 *        `T`-typed expression tree.
 *
 * Used when a raw scalar (e.g. `int` or `double`) is mixed into an expression whose
 * `value_type` is a different type (e.g. `mpfr::mpreal`).  `make_expr<T>()` produces
 * an `OtherType<T, S>` when `S != T` and `S` is not already an expression.  The
 * `value()` accessor returns the stored `Type` value which is then passed to
 * the `evaluate(tag, out, ..., scalar)` overloads in `CustomBinaryRules<T>`.
 *
 * @tparam T    The arithmetic value type of the surrounding expression tree.
 * @tparam Type The actual scalar type of the stored value (e.g. `int`, `double`).
 */
template<typename T, typename Type>
struct OtherType : public Atom<OtherType<T, Type>, T>{

    static_assert(traits::isConvertibleTo<Type, T>, "Type must be convertible to T");

    using Base = Atom<OtherType<T, Type>, T>;
    
    static constexpr bool isLazy = true;

    LAZY_FORCE_INLINE OtherType(const Type& value) : value_(value) {}

    LAZY_FORCE_INLINE const Type& value() const {return value_;}
    
    Type value_;

};


/**
 * @brief Owning lazy variable — the primary user-facing storage type.
 *
 * `LazyType<T>` is the type users declare variables with.  It is an `Atom` so it
 * can be used directly in expressions without evaluation, and it supports:
 *
 * - Construction from raw `T`, from other atoms, and from `Node` expressions
 *   (which evaluates them immediately via `eval()`).
 * - Compound assignment (`+=`, `-=`, `*=`, `/=`) that update `value_` in place
 *   using `CustomBinaryRules<T>`, correctly handling both atom and node operands
 *   without extra temporaries.
 * - Implicit conversion `operator T()` returning `value_` by copy.
 * - Thread-safety for *reading* the scratch pointer registry via `for_each_aux`.
 *
 * **Note:** `make_expr<T>()` converts an lvalue `LazyType<T>` to `RefType<T>` so
 * that expression trees reference the original variable; an rvalue `LazyType<T>` is
 * moved into a new `LazyType<T>` node.
 *
 * @tparam T The arithmetic value type.
 */
template<typename T>
struct LazyType : public detail::Atom<LazyType<T>, T>{
    
    using Base = detail::Atom<LazyType<T>, T>;
    
    static constexpr bool isLazy = true;

    // Main constructors and assignment operators
    LazyType() = default;
    LazyType(const LazyType&) = default;
    LazyType(LazyType&&) = default;
    LazyType& operator=(const LazyType&) = default;
    LazyType& operator=(LazyType&&) = default;
    LazyType& operator=(LazyType& other) {
        return this->operator=(static_cast<const LazyType&>(other));
    }
    LazyType(LazyType& other) : LazyType(static_cast<const LazyType&>(other)) {}  // Prevent variadic from matching lvalue ref
    ~LazyType() = default;

    // Construct from lazy expressions
    template<typename U>
    requires (!traits::isLazyExpr<U, T>)
    LazyType(U&& value) : value_(std::forward<U>(value)) {}

    template<typename U>
    requires (traits::isAtom<U, T> && !::lazy::traits::isLazy<U, T>)
    LazyType(U&& value) : value_(value.value()) {}

    template<typename U>
    requires (traits::isNode<U, T>)
    LazyType(U&& value) {
        value.eval(value_);
    }

    // Construct by forwarding values
    template<typename... U>
    requires (!traits::isLazyExpr<U, T> && ...)
    LazyType(U&&... value) : value_(std::forward<U>(value)...) {}

    // Assignment operators
    template<traits::isNode<T> U>
    LazyType& operator=(U&& other){
        other.eval(value_);
        return *this;
    }

    template<traits::isAtom<T> U>
    LazyType& operator=(U&& other){
        value_ = other.value();
        return *this;
    }

    template<typename U>
    LazyType& operator=(U&& other) requires (!traits::isLazyExpr<U, T>){
        value_ = std::forward<U>(other);
        return *this;
    }

    // Compound assignment operators
    template<typename U>
    LazyType& operator+=(U&& other){
        if constexpr (traits::isAtom<U, T>){
            detail::CustomBinaryRules<T>::evaluate(lazy::tags::PLUS{}, value_, value_, other.value());
        } else if constexpr (traits::isNode<U, T>){
            detail::CustomBinaryRules<T>::eval_rule(lazy::tags::PLUS{}, value_, detail::make_expr<T>(value_), other);
        } else {
            detail::CustomBinaryRules<T>::evaluate(lazy::tags::PLUS{}, value_, value_, other);
        }
        return *this;
    }

    template<typename U>
    LazyType& operator*=(U&& other){
        if constexpr (traits::isAtom<U, T>){
            detail::CustomBinaryRules<T>::evaluate(lazy::tags::MUL{}, value_, value_, other.value());
        } else if constexpr (traits::isNode<U, T>){
            detail::CustomBinaryRules<T>::eval_rule(lazy::tags::MUL{}, value_, detail::make_expr<T>(value_), other);
        } else {
            detail::CustomBinaryRules<T>::evaluate(lazy::tags::MUL{}, value_, value_, other);
        }
        return *this;
    }

    template<typename U>
    LazyType& operator-=(U&& other){
        if constexpr (traits::isAtom<U, T>){
            detail::CustomBinaryRules<T>::evaluate(lazy::tags::MINUS{}, value_, value_, other.value());
        } else if constexpr (traits::isNode<U, T>){
            detail::CustomBinaryRules<T>::eval_rule(lazy::tags::MINUS{}, value_, detail::make_expr<T>(value_), other);
        } else {
            detail::CustomBinaryRules<T>::evaluate(lazy::tags::MINUS{}, value_, value_, other);
        }
        return *this;
    }

    template<typename U>
    LazyType& operator/=(U&& other){
        if constexpr (traits::isAtom<U, T>){
            detail::CustomBinaryRules<T>::evaluate(lazy::tags::DIV{}, value_, value_, other.value());
        } else if constexpr (traits::isNode<U, T>){
            detail::CustomBinaryRules<T>::eval_rule(lazy::tags::DIV{}, value_, detail::make_expr<T>(value_), other);
        } else {
            detail::CustomBinaryRules<T>::evaluate(lazy::tags::DIV{}, value_, value_, other);
        }
        return *this;
    }

    operator T() const { return value_; }

    template<typename F>
    inline static void for_each_aux(F&& fn) {
        detail::Rules<T>::for_each_aux(std::forward<F>(fn));
    }

    LAZY_FORCE_INLINE const T& value() const {return value_;}

    LAZY_FORCE_INLINE T& value() {return value_;}
private:
    T value_;

};


/**
 * @brief Canonicalise any value into an expression node of value type `T`.
 *
 * This function is the central entry-point used by all operator overloads and
 * `BinaryOperator::eval()` to convert heterogeneous operands to a uniform expression
 * type.  The conversion rules are:
 *
 * | Input type                  | Output type                              |
 * |-----------------------------|------------------------------------------|
 * | `LazyType<T>` lvalue        | `RefType<T>(value.value())`              |
 * | `LazyType<T>` rvalue        | `LazyType<T>(std::move(value))`          |
 * | Any other `isLazyExpr<T>` type  | Forwarded as-is (no copy)                |
 * | Exactly `T` lvalue          | `RefType<T>(value)` (reference to `T`)   |
 * | Exactly `T` rvalue          | `LazyType<T>(value)` (takes ownership)   |
 * | Any other scalar `S`        | `OtherType<T, S>(value)`                 |
 *
 * @tparam T     The target arithmetic value type for the expression tree.
 * @tparam R     The type of the incoming value (deduced).
 * @param  value The value to wrap.  For lvalue expressions, passed by lvalue ref;
 *               for rvalue nodes/scalars, passed by rvalue ref.
 * @return       An expression node of the appropriate type (see table).
 */
template<typename T, typename R>
LAZY_FORCE_INLINE decltype(auto) make_expr(R&& value){

    static_assert(lazy::traits::isValidType<R, T>, "Invalid type for make_expr");

    if constexpr (lazy::traits::isLazy<R, T> && std::is_lvalue_reference_v<R>) {
        // static_assert(std::is_lvalue_reference_v<R>, "LazyType rvalues are not allowed; bind to a variable first.");
        return RefType<T>(value.value());
    } else if constexpr (lazy::traits::isLazy<R, T>) {
        return LazyType<T>(std::move(value));
    } else if constexpr (lazy::traits::isLazyExpr<std::decay_t<R>, T>) {
        return std::forward<R>(value);
    } else if constexpr (std::is_same_v<T, std::decay_t<R>> && std::is_lvalue_reference_v<R>) {
        return RefType<T>(value);
    } else if constexpr (std::is_same_v<T, std::decay_t<R>>) {
        return LazyType<T>(std::forward<R>(value));
    } else {
        return OtherType<T, std::decay_t<R>>(std::forward<R>(value));
    }
}


} // namespace detail


using detail::LazyType;

} // namespace lazy

#endif // LAZY_IMPL_HPP