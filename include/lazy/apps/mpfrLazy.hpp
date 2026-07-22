#ifndef MPFR_LAZY_HPP
#define MPFR_LAZY_HPP

/**
 * @file mpfrLazy.hpp
 * @brief `lazy` library specialisations for `mpfr::mpreal` (arbitrary-precision arithmetic).
 *
 * This header wires the lazy expression-template library (`lazy.hpp`) to the MPFR C
 * library via the `mpfr::mpreal` C++ wrapper.  It provides:
 *
 * - `std::numeric_limits` specialisation for `LazyType<mpfr::mpreal>` via
 *   `LAZY_DECLARE_NUMERIC_TYPE`.
 * - `LAZY_SPECIALIZE_FUNCTIONS(mpfr::mpreal)` — type-specific `evaluate` overloads for
 *   `NEG`, `ABS`, and `SQRT` using the raw MPFR C API for maximum performance.
 * - `LAZY_SPECIALIZE_OPERATIONS(mpfr::mpreal)` — type-specific `evaluate` overloads for
 *   `+`, `-`, `*`, `/`, `pow`, `max`, `min` covering all combinations of
 *   `mpfr::mpreal` with `double`, `int`, `float`, `long`, and `size_t`.
 * - Fused-arithmetic `LAZY_OVERRIDE_OPER` specialisations that intercept structural
 *   patterns (e.g. `a + b*c`) and replace them with a single MPFR fused call
 *   (`mpfr_fma`, `mpfr_fms`, `mpfr_fmma`, `mpfr_fmms`) for higher accuracy and
 *   fewer rounding steps.
 * - `set_default_mpreal_prec(prec)` — change the default MPFR precision globally and
 *   resize all registered scratch buffers.
 * - `isfinite(LazyType<mpfr::mpreal>)` — overload matching `std::isfinite` for lazy
 *   mpreal variables.
 *
 * ### Fused operations (requires MPFR >= 4.0)
 * When `MPFR_VERSION >= MPFR_VERSION_NUM(4,0,0)`, the additional fused
 * multiply-multiply-add (`mpfr_fmma`) and subtract (`mpfr_fmms`) overrides are
 * compiled in.
 *
 * ### Precision management
 * All thread-local scratch `mpfr::mpreal` objects allocated by `RuleTree` are
 * registered in `Rules<mpfr::mpreal>::aux_ptrs`.  `set_default_mpreal_prec` iterates
 * over that registry and calls `set_prec` on every scratch variable so that precision
 * changes are reflected throughout the evaluation pipeline.
 *
 * @note Include this header *instead of* `lazy.hpp` when working with
 *       `mpfr::mpreal`. It includes `lazy.hpp` internally.
 */

#include "../lazy.hpp"
#include <mpreal.h>

/// Plug `LazyType<mpfr::mpreal>` into `std::numeric_limits` so that generic
/// numerical code (e.g. ODE solvers querying `epsilon`) works transparently.

namespace lazy::detail {


/**
 * @brief Unary function specialisations for `mpfr::mpreal`.
 *
 * Overrides `CustomUnaryRules<mpfr::mpreal>::evaluate` for `NEG`, `ABS`, and `SQRT`
 * using the corresponding raw MPFR C library functions, which avoid any overhead from
 * the `mpfr::mpreal` operator overloads and respect the global rounding mode.
 */
LAZY_SPECIALIZE_FUNCTIONS(mpfr::mpreal){

    using T = mpfr::mpreal;
    using Base = UnaryOpRules<CustomUnaryRules<T>, T>;
    using Base::eval_rule;
    using Base::evaluate;

    // neg
    LAZY_EVALUATE_FUNC(T, a, NEG, T){
        mpfr_neg(out.mpfr_ptr(), a.mpfr_srcptr(), mpfr::mpreal::get_default_rnd());
    }

    // abs
    LAZY_EVALUATE_FUNC(T, a, ABS, T){
        mpfr_abs(out.mpfr_ptr(), a.mpfr_srcptr(), mpfr::mpreal::get_default_rnd());
    }

    // sqrt
    LAZY_EVALUATE_FUNC(T, a, SQRT, T){
        mpfr_sqrt(out.mpfr_ptr(), a.mpfr_srcptr(), mpfr::mpreal::get_default_rnd());
    }

};



/**
 * @brief Binary operation specialisations for `mpfr::mpreal`.
 *
 * Overrides `CustomBinaryRules<mpfr::mpreal>::evaluate` for all combinations of
 * `mpfr::mpreal` with `double`, `int`, `float`, `long`, and `size_t` for the
 * operations `+`, `-`, `*`, `/`, `pow`, `max`, and `min`.
 *
 * Each overload uses the most efficient MPFR C function for the given operand types
 * (e.g. `mpfr_add_d` when one operand is `double`, `mpfr_add_si` for `int`/`long`).
 *
 * Additionally, structural `LAZY_OVERRIDE_OPER` specialisations replace common sub-expression
 * patterns with MPFR fused operations:
 *
 * | Pattern              | MPFR function  | Condition          |
 * |----------------------|----------------|--------------------|
 * | `a + b*c`            | `mpfr_fma`     | always             |
 * | `a*b + c`            | `mpfr_fma`     | always             |
 * | `a*b - c`            | `mpfr_fms`     | always             |
 * | `c - a*b`            | `mpfr_fms+neg` | always             |
 * | `a*b + c*d`          | `mpfr_fmma`    | MPFR >= 4.0        |
 * | `a*b - c*d`          | `mpfr_fmms`    | MPFR >= 4.0        |
 * | `a + (b + c)`        | `mpfr_sum`     | always (3-sum)     |
 */
LAZY_SPECIALIZE_OPERATIONS(mpfr::mpreal){

    using T = mpfr::mpreal;
    using Base = BinaryOpRules<CustomBinaryRules<T>, T>;
    using Base::evaluate;
    using Base::eval_rule;

    // mpreal with mpreal
    LAZY_EVALUATE_OPER(T, a, b, T, PLUS, T){
        mpfr_add(out.mpfr_ptr(), a.mpfr_srcptr(), b.mpfr_srcptr(), mpfr::mpreal::get_default_rnd());
    }

    // mpreal with double
    LAZY_EVALUATE_OPER(T, a, b, T, PLUS, double){
        mpfr_add_d(out.mpfr_ptr(), a.mpfr_srcptr(), b, mpfr::mpreal::get_default_rnd());
    }

    LAZY_EVALUATE_OPER(T, a, b, double, PLUS, T){
        mpfr_add_d(out.mpfr_ptr(), b.mpfr_srcptr(), a, mpfr::mpreal::get_default_rnd());
    }

    // mpreal with int
    LAZY_EVALUATE_OPER(T, a, b, T, PLUS, int){
        mpfr_add_si(out.mpfr_ptr(), a.mpfr_srcptr(), b, mpfr::mpreal::get_default_rnd());
    }

    LAZY_EVALUATE_OPER(T, a, b, int, PLUS, T){
        mpfr_add_si(out.mpfr_ptr(), b.mpfr_srcptr(), a, mpfr::mpreal::get_default_rnd());
    }

    // mpreal with float
    LAZY_EVALUATE_OPER(T, a, b, T, PLUS, float){
        mpfr_add_d(out.mpfr_ptr(), a.mpfr_srcptr(), b, mpfr::mpreal::get_default_rnd());
    }

    LAZY_EVALUATE_OPER(T, a, b, float, PLUS, T){
        mpfr_add_d(out.mpfr_ptr(), b.mpfr_srcptr(), a, mpfr::mpreal::get_default_rnd());
    }

    // mpreal with long
    LAZY_EVALUATE_OPER(T, a, b, T, PLUS, long){
        mpfr_add_si(out.mpfr_ptr(), a.mpfr_srcptr(), b, mpfr::mpreal::get_default_rnd());
    }

    LAZY_EVALUATE_OPER(T, a, b, long, PLUS, T){
        mpfr_add_si(out.mpfr_ptr(), b.mpfr_srcptr(), a, mpfr::mpreal::get_default_rnd());
    }

    // mpreal with size_t
    LAZY_EVALUATE_OPER(T, a, b, T, PLUS, size_t){
        mpfr_add_ui(out.mpfr_ptr(), a.mpfr_srcptr(), b, mpfr::mpreal::get_default_rnd());
    }

    LAZY_EVALUATE_OPER(T, a, b, size_t, PLUS, T){
        mpfr_add_ui(out.mpfr_ptr(), b.mpfr_srcptr(), a, mpfr::mpreal::get_default_rnd());
    }



    // mpreal with mpreal
    LAZY_EVALUATE_OPER(T, a, b, T, MUL, T){
        mpfr_mul(out.mpfr_ptr(), a.mpfr_srcptr(), b.mpfr_srcptr(), mpfr::mpreal::get_default_rnd());
    }

    // mpreal with double
    LAZY_EVALUATE_OPER(T, a, b, T, MUL, double){
        mpfr_mul_d(out.mpfr_ptr(), a.mpfr_srcptr(), b, mpfr::mpreal::get_default_rnd());
    }

    LAZY_EVALUATE_OPER(T, a, b, double, MUL, T){
        mpfr_mul_d(out.mpfr_ptr(), b.mpfr_srcptr(), a, mpfr::mpreal::get_default_rnd());
    }

    // mpreal with int
    LAZY_EVALUATE_OPER(T, a, b, T, MUL, int){
        mpfr_mul_si(out.mpfr_ptr(), a.mpfr_srcptr(), b, mpfr::mpreal::get_default_rnd());
    }

    LAZY_EVALUATE_OPER(T, a, b, int, MUL, T){
        mpfr_mul_si(out.mpfr_ptr(), b.mpfr_srcptr(), a, mpfr::mpreal::get_default_rnd());
    }

    // mpreal with float
    LAZY_EVALUATE_OPER(T, a, b, T, MUL, float){
        mpfr_mul_d(out.mpfr_ptr(), a.mpfr_srcptr(), b, mpfr::mpreal::get_default_rnd());
    }

    LAZY_EVALUATE_OPER(T, a, b, float, MUL, T){
        mpfr_mul_d(out.mpfr_ptr(), b.mpfr_srcptr(), a, mpfr::mpreal::get_default_rnd());
    }

    // mpreal with long
    LAZY_EVALUATE_OPER(T, a, b, T, MUL, long){
        mpfr_mul_si(out.mpfr_ptr(), a.mpfr_srcptr(), b, mpfr::mpreal::get_default_rnd());
    }

    LAZY_EVALUATE_OPER(T, a, b, long, MUL, T){
        mpfr_mul_si(out.mpfr_ptr(), b.mpfr_srcptr(), a, mpfr::mpreal::get_default_rnd());
    }

    // mpreal with size_t
    LAZY_EVALUATE_OPER(T, a, b, T, MUL, size_t){
        mpfr_mul_ui(out.mpfr_ptr(), a.mpfr_srcptr(), b, mpfr::mpreal::get_default_rnd());
    }

    LAZY_EVALUATE_OPER(T, a, b, size_t, MUL, T){
        mpfr_mul_ui(out.mpfr_ptr(), b.mpfr_srcptr(), a, mpfr::mpreal::get_default_rnd());
    }

    // mpreal with mpreal
    LAZY_EVALUATE_OPER(T, a, b, T, DIV, T){
        mpfr_div(out.mpfr_ptr(), a.mpfr_srcptr(), b.mpfr_srcptr(), mpfr::mpreal::get_default_rnd());
    }

    // mpreal with double
    LAZY_EVALUATE_OPER(T, a, b, T, DIV, double){

        mpfr_div_d(out.mpfr_ptr(), a.mpfr_srcptr(), b, mpfr::mpreal::get_default_rnd());
    }

    LAZY_EVALUATE_OPER(T, a, b, double, DIV, T){
        mpfr_d_div(out.mpfr_ptr(), a, b.mpfr_srcptr(), mpfr::mpreal::get_default_rnd());
    }

    // mpreal with int
    LAZY_EVALUATE_OPER(T, a, b, T, DIV, int){
        mpfr_div_si(out.mpfr_ptr(), a.mpfr_srcptr(), b, mpfr::mpreal::get_default_rnd());
    }

    LAZY_EVALUATE_OPER(T, a, b, int, DIV, T){
        mpfr_set_si(out.mpfr_ptr(), a, mpfr::mpreal::get_default_rnd());
        mpfr_div(out.mpfr_ptr(), out.mpfr_srcptr(), b.mpfr_srcptr(), mpfr::mpreal::get_default_rnd());
    }

    // mpreal with float
    LAZY_EVALUATE_OPER(T, a, b, T, DIV, float){
        mpfr_div_d(out.mpfr_ptr(), a.mpfr_srcptr(), b, mpfr::mpreal::get_default_rnd());
    }

    LAZY_EVALUATE_OPER(T, a, b, float, DIV, T){
        mpfr_d_div(out.mpfr_ptr(), a, b.mpfr_srcptr(), mpfr::mpreal::get_default_rnd());
    }

    // mpreal with long
    LAZY_EVALUATE_OPER(T, a, b, T, DIV, long){
        mpfr_div_si(out.mpfr_ptr(), a.mpfr_srcptr(), b, mpfr::mpreal::get_default_rnd());
    }

    LAZY_EVALUATE_OPER(T, a, b, long, DIV, T){
        mpfr_set_si(out.mpfr_ptr(), a, mpfr::mpreal::get_default_rnd());
        mpfr_div(out.mpfr_ptr(), out.mpfr_srcptr(), b.mpfr_srcptr(), mpfr::mpreal::get_default_rnd());
    }

    // mpreal with size_t
    LAZY_EVALUATE_OPER(T, a, b, T, DIV, size_t){
        mpfr_div_ui(out.mpfr_ptr(), a.mpfr_srcptr(), b, mpfr::mpreal::get_default_rnd());
    }

    LAZY_EVALUATE_OPER(T, a, b, size_t, DIV, T){
        mpfr_set_ui(out.mpfr_ptr(), a, mpfr::mpreal::get_default_rnd());
        mpfr_div(out.mpfr_ptr(), out.mpfr_srcptr(), b.mpfr_srcptr(), mpfr::mpreal::get_default_rnd());
    }


    // mpreal with mpreal
    LAZY_EVALUATE_OPER(T, a, b, T, MINUS, T){
        mpfr_sub(out.mpfr_ptr(), a.mpfr_srcptr(), b.mpfr_srcptr(), mpfr::mpreal::get_default_rnd());
    }

    // mpreal with double
    LAZY_EVALUATE_OPER(T, a, b, T, MINUS, double){
        mpfr_sub_d(out.mpfr_ptr(), a.mpfr_srcptr(), b, mpfr::mpreal::get_default_rnd());
    }

    LAZY_EVALUATE_OPER(T, a, b, double, MINUS, T){
        mpfr_d_sub(out.mpfr_ptr(), a, b.mpfr_srcptr(), mpfr::mpreal::get_default_rnd());
    }

    // mpreal with int
    LAZY_EVALUATE_OPER(T, a, b, T, MINUS, int){
        mpfr_sub_si(out.mpfr_ptr(), a.mpfr_srcptr(), b, mpfr::mpreal::get_default_rnd());
    }

    LAZY_EVALUATE_OPER(T, a, b, int, MINUS, T){
        mpfr_si_sub(out.mpfr_ptr(), a, b.mpfr_srcptr(), mpfr::mpreal::get_default_rnd());
    }

    // mpreal with float
    LAZY_EVALUATE_OPER(T, a, b, T, MINUS, float){
        mpfr_sub_d(out.mpfr_ptr(), a.mpfr_srcptr(), b, mpfr::mpreal::get_default_rnd());
    }

    LAZY_EVALUATE_OPER(T, a, b, float, MINUS, T){
        mpfr_d_sub(out.mpfr_ptr(), a, b.mpfr_srcptr(), mpfr::mpreal::get_default_rnd());
    }

    // mpreal with long
    LAZY_EVALUATE_OPER(T, a, b, T, MINUS, long){
        mpfr_sub_si(out.mpfr_ptr(), a.mpfr_srcptr(), b, mpfr::mpreal::get_default_rnd());
    }

    LAZY_EVALUATE_OPER(T, a, b, long, MINUS, T){
        mpfr_si_sub(out.mpfr_ptr(), a, b.mpfr_srcptr(), mpfr::mpreal::get_default_rnd());
    }

    // mpreal with size_t
    LAZY_EVALUATE_OPER(T, a, b, T, MINUS, size_t){
        mpfr_sub_ui(out.mpfr_ptr(), a.mpfr_srcptr(), b, mpfr::mpreal::get_default_rnd());
    }

    LAZY_EVALUATE_OPER(T, a, b, size_t, MINUS, T){
        mpfr_ui_sub(out.mpfr_ptr(), a, b.mpfr_srcptr(), mpfr::mpreal::get_default_rnd());
    }








    // mpreal with mpreal
    LAZY_EVALUATE_OPER(T, a, b, T, POW, T){
        mpfr_pow(out.mpfr_ptr(), a.mpfr_srcptr(), b.mpfr_srcptr(), mpfr::mpreal::get_default_rnd());
    }

    // mpreal with double
    LAZY_EVALUATE_OPER(T, a, b, T, POW, double){
        mpfr_set_d(out.mpfr_ptr(), b, mpfr::mpreal::get_default_rnd());
        mpfr_pow(out.mpfr_ptr(), a.mpfr_srcptr(), out.mpfr_srcptr(), mpfr::mpreal::get_default_rnd());
    }

    LAZY_EVALUATE_OPER(T, a, b, double, POW, T){
        mpfr_set_d(out.mpfr_ptr(), a, mpfr::mpreal::get_default_rnd());
        mpfr_pow(out.mpfr_ptr(), out.mpfr_srcptr(), b.mpfr_srcptr(), mpfr::mpreal::get_default_rnd());
    }

    // mpreal with int
    LAZY_EVALUATE_OPER(T, a, b, T, POW, int){
        mpfr_pow_si(out.mpfr_ptr(), a.mpfr_srcptr(), b, mpfr::mpreal::get_default_rnd());
    }

    LAZY_EVALUATE_OPER(T, a, b, int, POW, T){
        mpfr_set_si(out.mpfr_ptr(), a, mpfr::mpreal::get_default_rnd());
        mpfr_pow(out.mpfr_ptr(), out.mpfr_srcptr(), b.mpfr_srcptr(), mpfr::mpreal::get_default_rnd());
    }

    // mpreal with float
    LAZY_EVALUATE_OPER(T, a, b, T, POW, float){
        mpfr_set_d(out.mpfr_ptr(), static_cast<double>(b), mpfr::mpreal::get_default_rnd());
        mpfr_pow(out.mpfr_ptr(), a.mpfr_srcptr(), out.mpfr_srcptr(), mpfr::mpreal::get_default_rnd());
    }

    LAZY_EVALUATE_OPER(T, a, b, float, POW, T){
        mpfr_set_d(out.mpfr_ptr(), static_cast<double>(a), mpfr::mpreal::get_default_rnd());
        mpfr_pow(out.mpfr_ptr(), out.mpfr_srcptr(), b.mpfr_srcptr(), mpfr::mpreal::get_default_rnd());
    }

    // mpreal with long
    LAZY_EVALUATE_OPER(T, a, b, T, POW, long){
        mpfr_pow_si(out.mpfr_ptr(), a.mpfr_srcptr(), b, mpfr::mpreal::get_default_rnd());
    }

    LAZY_EVALUATE_OPER(T, a, b, long, POW, T){
        mpfr_set_si(out.mpfr_ptr(), a, mpfr::mpreal::get_default_rnd());
        mpfr_pow(out.mpfr_ptr(), out.mpfr_srcptr(), b.mpfr_srcptr(), mpfr::mpreal::get_default_rnd());
    }

    // mpreal with size_t
    LAZY_EVALUATE_OPER(T, a, b, T, POW, size_t){
        mpfr_pow_ui(out.mpfr_ptr(), a.mpfr_srcptr(), b, mpfr::mpreal::get_default_rnd());
    }

    LAZY_EVALUATE_OPER(T, a, b, size_t, POW, T){
        mpfr_set_ui(out.mpfr_ptr(), a, mpfr::mpreal::get_default_rnd());
        mpfr_pow(out.mpfr_ptr(), out.mpfr_srcptr(), b.mpfr_srcptr(), mpfr::mpreal::get_default_rnd());
    }

    // max
    LAZY_EVALUATE_OPER(T, a, b, T, MAX, T){
        mpfr_max(out.mpfr_ptr(), a.mpfr_srcptr(), b.mpfr_srcptr(), mpfr::mpreal::get_default_rnd());
    }

    // min
    LAZY_EVALUATE_OPER(T, a, b, T, MIN, T){
        mpfr_min(out.mpfr_ptr(), a.mpfr_srcptr(), b.mpfr_srcptr(), mpfr::mpreal::get_default_rnd());
    }




    using Mul_T_T = Multiplication<T, T>;
    using Add_T_T = Addition<T, T>;

    /// 3-operand sum: `a + (b + c)` via `mpfr_sum` for improved accuracy.
    LAZY_OVERRIDE_OPER(T, a, b, T, PLUS, Add_T_T){
        // b is an Add expression with lhs and rhs
        T& a_mut = const_cast<T&>(a.value());
        T& b_lhs_mut = const_cast<T&>(b.lhs.value());
        T& b_rhs_mut = const_cast<T&>(b.rhs.value());
        mpfr_ptr tmp[3] = {a_mut.mpfr_ptr(), b_lhs_mut.mpfr_ptr(), b_rhs_mut.mpfr_ptr()};
        mpfr_sum(out.mpfr_ptr(), tmp, 3, mpfr::mpreal::get_default_rnd());
    }

    /// Fused multiply-add: `a + b*c` via `mpfr_fma`.
    LAZY_OVERRIDE_OPER(T, a, b, T, PLUS, Mul_T_T){
        // b is a Mul expression with lhs and rhs
        mpfr_fma(out.mpfr_ptr(),
                 b.lhs.value().mpfr_srcptr(),
                 b.rhs.value().mpfr_srcptr(),
                 a.value().mpfr_srcptr(),
                 mpfr::mpreal::get_default_rnd());
    }

    /// Fused multiply-add (commuted): `a*b + c` via `mpfr_fma`.
    LAZY_OVERRIDE_OPER(T, a, b, Mul_T_T, PLUS, T){
        mpfr_fma(out.mpfr_ptr(),
                 a.lhs.value().mpfr_srcptr(),
                 a.rhs.value().mpfr_srcptr(),
                 b.value().mpfr_srcptr(),
                 mpfr::mpreal::get_default_rnd());
    }

    /// Fused multiply-subtract: `a*b - c` via `mpfr_fms`.
    LAZY_OVERRIDE_OPER(T, a, b, Mul_T_T, MINUS, T){
        mpfr_fms(out.mpfr_ptr(),
                 a.lhs.value().mpfr_srcptr(),
                 a.rhs.value().mpfr_srcptr(),
                 b.value().mpfr_srcptr(),
                 mpfr::mpreal::get_default_rnd());
    }

    /// Negated fused multiply-subtract: `c - a*b` via `mpfr_fms` followed by negation.
    LAZY_OVERRIDE_OPER(T, a, b, T, MINUS, Mul_T_T){
        mpfr_fms(out.mpfr_ptr(),
                 b.lhs.value().mpfr_srcptr(),
                 b.rhs.value().mpfr_srcptr(),
                 a.value().mpfr_srcptr(),
                 mpfr::mpreal::get_default_rnd());
        mpfr_neg(out.mpfr_ptr(), out.mpfr_srcptr(), mpfr::mpreal::get_default_rnd());
    }

#if MPFR_VERSION >= MPFR_VERSION_NUM(4, 0, 0)
    /// Fused multiply-multiply-add: `a*b + c*d` via `mpfr_fmma` (MPFR >= 4.0).
    LAZY_OVERRIDE_OPER(T, a, b, Mul_T_T, PLUS, Mul_T_T){
        mpfr_fmma(out.mpfr_ptr(),
                  a.lhs.value().mpfr_srcptr(),
                  a.rhs.value().mpfr_srcptr(),
                  b.lhs.value().mpfr_srcptr(),
                  b.rhs.value().mpfr_srcptr(),
                  mpfr::mpreal::get_default_rnd());
    }

    /// Fused multiply-multiply-subtract: `a*b - c*d` via `mpfr_fmms` (MPFR >= 4.0).
    LAZY_OVERRIDE_OPER(T, a, b, Mul_T_T, MINUS, Mul_T_T){
        mpfr_fmms(out.mpfr_ptr(),
                  a.lhs.value().mpfr_srcptr(),
                  a.rhs.value().mpfr_srcptr(),
                  b.lhs.value().mpfr_srcptr(),
                  b.rhs.value().mpfr_srcptr(),
                  mpfr::mpreal::get_default_rnd());
    }
#endif // MPFR_VERSION >= 4.0


};


/**
 * @brief Check whether a `LazyType<mpfr::mpreal>` holds a finite value.
 *
 * Delegates to `mpfr::isfinite` on the underlying `mpfr::mpreal` value.
 * Provided so that generic numerical algorithms using `std::isfinite`-like calls
 * work with lazy mpreal variables without explicit `value()` extraction.
 *
 * @param x  A lazy mpreal variable.
 * @return   `true` if `x.value()` is finite (not ±inf or NaN).
 */
inline bool isfinite(const LazyType<mpfr::mpreal>& x){
    return mpfr::isfinite(x.value());
}


}; // namespace lazy::detail

LAZY_DECLARE_NUMERIC_TYPE(mpfr::mpreal);


namespace lazy {

using lazy::detail::isfinite;

/**
 * @brief Set the global default MPFR precision and resize all scratch buffers.
 *
 * Changes the precision for newly created `mpfr::mpreal` objects **and** resizes
 * every `mpfr::mpreal` scratch buffer registered in `Rules<mpfr::mpreal>::aux_ptrs`
 * (i.e. all thread-local temporaries created by `RuleTree` for `mpfr::mpreal`
 * expressions).  This ensures that subsequent lazy evaluations use the new precision
 * throughout.
 *
 * @param prec  The new MPFR precision in bits (e.g. 256 for quad-like precision).
 */
inline void set_default_mpreal_prec(mpfr_prec_t prec){
    mpfr::mpreal::set_default_prec(prec);
    lazy::LazyType<mpfr::mpreal>::for_each_aux([prec](mpfr::mpreal& key){
        key.set_prec(prec);
    });
    mpfr::mpreal::set_default_prec(prec);
}

} // namespace lazy


#endif // MPFR_LAZY_HPP