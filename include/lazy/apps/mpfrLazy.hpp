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
 * All thread-local scratch `mpfr::mpreal` objects allocated automatically are
 * registered in `LazyType<mpfr::mpreal>::workers`.  `set_default_mpreal_prec` iterates
 * over that registry and calls `set_prec` on every scratch variable so that precision
 * changes are reflected throughout the evaluation pipeline.
 *
 * @note Include this header *instead of* `lazy.hpp` when working with
 *       `mpfr::mpreal`. It includes `lazy.hpp` internally.
 */

#include <mpreal.h>
#include "../lazy.hpp"

/// Plug `LazyType<mpfr::mpreal>` into `std::numeric_limits` so that generic
/// numerical code (e.g. ODE solvers querying `epsilon`) works transparently.

#ifndef LAZY_MPFR_RND
#define LAZY_MPFR_RND mpfr::mpreal::get_default_rnd()
#endif

LAZY_DECLARE_NUMERIC_TYPE(mpfr::mpreal);

namespace lazy::detail {


/**
 * @brief Unary function specialisations for `mpfr::mpreal`.
 *
 * Overrides `CustomUnaryEvaluator<mpfr::mpreal>::evaluate` for `NEG`, `ABS`, and `SQRT`
 * using the corresponding raw MPFR C library functions, which avoid any overhead from
 * the `mpfr::mpreal` operator overloads and respect the global rounding mode.
 */
LAZY_SPECIALIZE_FUNCTIONS(mpfr::mpreal){

    using T = mpfr::mpreal;
    using Base = UnaryEvaluator<CustomUnaryEvaluator<T>, T>;
    using Base::eval_rule;
    using Base::evaluate;

    // neg
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::NEG, T &out, const T &a){
        mpfr_neg(out.mpfr_ptr(), a.mpfr_srcptr(), LAZY_MPFR_RND);
    }

    // abs
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::ABS, T &out, const T &a){
        mpfr_abs(out.mpfr_ptr(), a.mpfr_srcptr(), LAZY_MPFR_RND);
    }

    // sqrt
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::SQRT, T &out, const T &a){
        mpfr_sqrt(out.mpfr_ptr(), a.mpfr_srcptr(), LAZY_MPFR_RND);
    }

    // exp
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::EXP, T &out, const T &a){
        mpfr_exp(out.mpfr_ptr(), a.mpfr_srcptr(), LAZY_MPFR_RND);
    }

    // log
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::LOG, T &out, const T &a){
        mpfr_log(out.mpfr_ptr(), a.mpfr_srcptr(), LAZY_MPFR_RND);
    }

    // sin
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::SIN, T &out, const T &a){
        mpfr_sin(out.mpfr_ptr(), a.mpfr_srcptr(), LAZY_MPFR_RND);
    }

    // cos
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::COS, T &out, const T &a){
        mpfr_cos(out.mpfr_ptr(), a.mpfr_srcptr(), LAZY_MPFR_RND);
    }

    // tan
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::TAN, T &out, const T &a){
        mpfr_tan(out.mpfr_ptr(), a.mpfr_srcptr(), LAZY_MPFR_RND);
    }

    // cot
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::COT, T &out, const T &a){
        mpfr_cot(out.mpfr_ptr(), a.mpfr_srcptr(), LAZY_MPFR_RND);
    }

    // sec
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::SEC, T &out, const T &a){
        mpfr_sec(out.mpfr_ptr(), a.mpfr_srcptr(), LAZY_MPFR_RND);
    }

    // csc
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::CSC, T &out, const T &a){
        mpfr_csc(out.mpfr_ptr(), a.mpfr_srcptr(), LAZY_MPFR_RND);
    }

    // asin
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::ASIN, T &out, const T &a){
        mpfr_asin(out.mpfr_ptr(), a.mpfr_srcptr(), LAZY_MPFR_RND);
    }

    // acos
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::ACOS, T &out, const T &a){
        mpfr_acos(out.mpfr_ptr(), a.mpfr_srcptr(), LAZY_MPFR_RND);
    }

    // atan
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::ATAN, T &out, const T &a){
        mpfr_atan(out.mpfr_ptr(), a.mpfr_srcptr(), LAZY_MPFR_RND);
    }

    // sinh
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::SINH, T &out, const T &a){
        mpfr_sinh(out.mpfr_ptr(), a.mpfr_srcptr(), LAZY_MPFR_RND);
    }

    // cosh
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::COSH, T &out, const T &a){
        mpfr_cosh(out.mpfr_ptr(), a.mpfr_srcptr(), LAZY_MPFR_RND);
    }

    // tanh
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::TANH, T &out, const T &a){
        mpfr_tanh(out.mpfr_ptr(), a.mpfr_srcptr(), LAZY_MPFR_RND);
    }

    // erf
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::ERF, T &out, const T &a){
        mpfr_erf(out.mpfr_ptr(), a.mpfr_srcptr(), LAZY_MPFR_RND);
    }

};



/**
 * @brief Binary operation specialisations for `mpfr::mpreal`.
 *
 * Overrides `CustomBinaryEvaluator<mpfr::mpreal>::evaluate` for all combinations of
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
    using Base = BinaryEvaluator<CustomBinaryEvaluator<T>, T>;
    using Base::evaluate;
    using Base::eval_rule;

    // mpreal with mpreal
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::PLUS, T &out, const T &a, const T &b){
        mpfr_add(out.mpfr_ptr(), a.mpfr_srcptr(), b.mpfr_srcptr(), LAZY_MPFR_RND);
    }

    // mpreal with double
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::PLUS, T &out, const T &a, const double &b){
        mpfr_add_d(out.mpfr_ptr(), a.mpfr_srcptr(), b, LAZY_MPFR_RND);
    }

    LAZY_FORCE_INLINE static void evaluate(lazy::tags::PLUS, T &out, const double &a, const T &b){
        mpfr_add_d(out.mpfr_ptr(), b.mpfr_srcptr(), a, LAZY_MPFR_RND);
    }

    // mpreal with int
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::PLUS, T &out, const T &a, const int &b){
        mpfr_add_si(out.mpfr_ptr(), a.mpfr_srcptr(), b, LAZY_MPFR_RND);
    }

    LAZY_FORCE_INLINE static void evaluate(lazy::tags::PLUS, T &out, const int &a, const T &b){
        mpfr_add_si(out.mpfr_ptr(), b.mpfr_srcptr(), a, LAZY_MPFR_RND);
    }

    // mpreal with float
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::PLUS, T &out, const T &a, const float &b){
        mpfr_add_d(out.mpfr_ptr(), a.mpfr_srcptr(), b, LAZY_MPFR_RND);
    }

    LAZY_FORCE_INLINE static void evaluate(lazy::tags::PLUS, T &out, const float &a, const T &b){
        mpfr_add_d(out.mpfr_ptr(), b.mpfr_srcptr(), a, LAZY_MPFR_RND);
    }

    // mpreal with long
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::PLUS, T &out, const T &a, const long &b){
        mpfr_add_si(out.mpfr_ptr(), a.mpfr_srcptr(), b, LAZY_MPFR_RND);
    }

    LAZY_FORCE_INLINE static void evaluate(lazy::tags::PLUS, T &out, const long &a, const T &b){
        mpfr_add_si(out.mpfr_ptr(), b.mpfr_srcptr(), a, LAZY_MPFR_RND);
    }

    // mpreal with size_t
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::PLUS, T &out, const T &a, const size_t &b){
        mpfr_add_ui(out.mpfr_ptr(), a.mpfr_srcptr(), b, LAZY_MPFR_RND);
    }

    LAZY_FORCE_INLINE static void evaluate(lazy::tags::PLUS, T &out, const size_t &a, const T &b){
        mpfr_add_ui(out.mpfr_ptr(), b.mpfr_srcptr(), a, LAZY_MPFR_RND);
    }






    // mpreal with mpreal
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::MINUS, T &out, const T &a, const T &b){
        mpfr_sub(out.mpfr_ptr(), a.mpfr_srcptr(), b.mpfr_srcptr(), LAZY_MPFR_RND);
    }

    // mpreal with double
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::MINUS, T &out, const T &a, const double &b){
        mpfr_sub_d(out.mpfr_ptr(), a.mpfr_srcptr(), b, LAZY_MPFR_RND);
    }

    LAZY_FORCE_INLINE static void evaluate(lazy::tags::MINUS, T &out, const double &a, const T &b){
        mpfr_d_sub(out.mpfr_ptr(), a, b.mpfr_srcptr(), LAZY_MPFR_RND);
    }

    // mpreal with int
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::MINUS, T &out, const T &a, const int &b){
        mpfr_sub_si(out.mpfr_ptr(), a.mpfr_srcptr(), b, LAZY_MPFR_RND);
    }

    LAZY_FORCE_INLINE static void evaluate(lazy::tags::MINUS, T &out, const int &a, const T &b){
        mpfr_si_sub(out.mpfr_ptr(), a, b.mpfr_srcptr(), LAZY_MPFR_RND);
    }

    // mpreal with float
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::MINUS, T &out, const T &a, const float &b){
        mpfr_sub_d(out.mpfr_ptr(), a.mpfr_srcptr(), b, LAZY_MPFR_RND);
    }

    LAZY_FORCE_INLINE static void evaluate(lazy::tags::MINUS, T &out, const float &a, const T &b){
        mpfr_d_sub(out.mpfr_ptr(), a, b.mpfr_srcptr(), LAZY_MPFR_RND);
    }

    // mpreal with long
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::MINUS, T &out, const T &a, const long &b){
        mpfr_sub_si(out.mpfr_ptr(), a.mpfr_srcptr(), b, LAZY_MPFR_RND);
    }

    LAZY_FORCE_INLINE static void evaluate(lazy::tags::MINUS, T &out, const long &a, const T &b){
        mpfr_si_sub(out.mpfr_ptr(), a, b.mpfr_srcptr(), LAZY_MPFR_RND);
    }

    // mpreal with size_t
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::MINUS, T &out, const T &a, const size_t &b){
        mpfr_sub_ui(out.mpfr_ptr(), a.mpfr_srcptr(), b, LAZY_MPFR_RND);
    }

    LAZY_FORCE_INLINE static void evaluate(lazy::tags::MINUS, T &out, const size_t &a, const T &b){
        mpfr_ui_sub(out.mpfr_ptr(), a, b.mpfr_srcptr(), LAZY_MPFR_RND);
    }







    // mpreal with mpreal
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::MUL, T &out, const T &a, const T &b){
        mpfr_mul(out.mpfr_ptr(), a.mpfr_srcptr(), b.mpfr_srcptr(), LAZY_MPFR_RND);
    }

    // mpreal with double
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::MUL, T &out, const T &a, const double &b){
        mpfr_mul_d(out.mpfr_ptr(), a.mpfr_srcptr(), b, LAZY_MPFR_RND);
    }

    LAZY_FORCE_INLINE static void evaluate(lazy::tags::MUL, T &out, const double &a, const T &b){
        mpfr_mul_d(out.mpfr_ptr(), b.mpfr_srcptr(), a, LAZY_MPFR_RND);
    }

    // mpreal with int
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::MUL, T &out, const T &a, const int &b){
        mpfr_mul_si(out.mpfr_ptr(), a.mpfr_srcptr(), b, LAZY_MPFR_RND);
    }

    LAZY_FORCE_INLINE static void evaluate(lazy::tags::MUL, T &out, const int &a, const T &b){
        mpfr_mul_si(out.mpfr_ptr(), b.mpfr_srcptr(), a, LAZY_MPFR_RND);
    }

    // mpreal with float
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::MUL, T &out, const T &a, const float &b){
        mpfr_mul_d(out.mpfr_ptr(), a.mpfr_srcptr(), b, LAZY_MPFR_RND);
    }

    LAZY_FORCE_INLINE static void evaluate(lazy::tags::MUL, T &out, const float &a, const T &b){
        mpfr_mul_d(out.mpfr_ptr(), b.mpfr_srcptr(), a, LAZY_MPFR_RND);
    }

    // mpreal with long
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::MUL, T &out, const T &a, const long &b){
        mpfr_mul_si(out.mpfr_ptr(), a.mpfr_srcptr(), b, LAZY_MPFR_RND);
    }

    LAZY_FORCE_INLINE static void evaluate(lazy::tags::MUL, T &out, const long &a, const T &b){
        mpfr_mul_si(out.mpfr_ptr(), b.mpfr_srcptr(), a, LAZY_MPFR_RND);
    }

    // mpreal with size_t
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::MUL, T &out, const T &a, const size_t &b){
        mpfr_mul_ui(out.mpfr_ptr(), a.mpfr_srcptr(), b, LAZY_MPFR_RND);
    }

    LAZY_FORCE_INLINE static void evaluate(lazy::tags::MUL, T &out, const size_t &a, const T &b){
        mpfr_mul_ui(out.mpfr_ptr(), b.mpfr_srcptr(), a, LAZY_MPFR_RND);
    }








    // mpreal with mpreal
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::DIV, T &out, const T &a, const T &b){
        mpfr_div(out.mpfr_ptr(), a.mpfr_srcptr(), b.mpfr_srcptr(), LAZY_MPFR_RND);
    }

    // mpreal with double
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::DIV, T &out, const T &a, const double &b){

        mpfr_div_d(out.mpfr_ptr(), a.mpfr_srcptr(), b, LAZY_MPFR_RND);
    }

    LAZY_FORCE_INLINE static void evaluate(lazy::tags::DIV, T &out, const double &a, const T &b){
        mpfr_d_div(out.mpfr_ptr(), a, b.mpfr_srcptr(), LAZY_MPFR_RND);
    }

    // mpreal with int
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::DIV, T &out, const T &a, const int &b){
        mpfr_div_si(out.mpfr_ptr(), a.mpfr_srcptr(), b, LAZY_MPFR_RND);
    }

    LAZY_FORCE_INLINE static void evaluate(lazy::tags::DIV, T &out, const int &a, const T &b){
        mpfr_set_si(out.mpfr_ptr(), a, LAZY_MPFR_RND);
        mpfr_div(out.mpfr_ptr(), out.mpfr_srcptr(), b.mpfr_srcptr(), LAZY_MPFR_RND);
    }

    // mpreal with float
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::DIV, T &out, const T &a, const float &b){
        mpfr_div_d(out.mpfr_ptr(), a.mpfr_srcptr(), b, LAZY_MPFR_RND);
    }

    LAZY_FORCE_INLINE static void evaluate(lazy::tags::DIV, T &out, const float &a, const T &b){
        mpfr_d_div(out.mpfr_ptr(), a, b.mpfr_srcptr(), LAZY_MPFR_RND);
    }

    // mpreal with long
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::DIV, T &out, const T &a, const long &b){
        mpfr_div_si(out.mpfr_ptr(), a.mpfr_srcptr(), b, LAZY_MPFR_RND);
    }

    LAZY_FORCE_INLINE static void evaluate(lazy::tags::DIV, T &out, const long &a, const T &b){
        mpfr_set_si(out.mpfr_ptr(), a, LAZY_MPFR_RND);
        mpfr_div(out.mpfr_ptr(), out.mpfr_srcptr(), b.mpfr_srcptr(), LAZY_MPFR_RND);
    }

    // mpreal with size_t
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::DIV, T &out, const T &a, const size_t &b){
        mpfr_div_ui(out.mpfr_ptr(), a.mpfr_srcptr(), b, LAZY_MPFR_RND);
    }

    LAZY_FORCE_INLINE static void evaluate(lazy::tags::DIV, T &out, const size_t &a, const T &b){
        mpfr_set_ui(out.mpfr_ptr(), a, LAZY_MPFR_RND);
        mpfr_div(out.mpfr_ptr(), out.mpfr_srcptr(), b.mpfr_srcptr(), LAZY_MPFR_RND);
    }






    // mpreal with mpreal
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::POW, T &out, const T &a, const T &b){
        mpfr_pow(out.mpfr_ptr(), a.mpfr_srcptr(), b.mpfr_srcptr(), LAZY_MPFR_RND);
    }

    // mpreal with double
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::POW, T &out, const T &a, const double &b){
        mpfr_set_d(out.mpfr_ptr(), b, LAZY_MPFR_RND);
        mpfr_pow(out.mpfr_ptr(), a.mpfr_srcptr(), out.mpfr_srcptr(), LAZY_MPFR_RND);
    }

    LAZY_FORCE_INLINE static void evaluate(lazy::tags::POW, T &out, const double &a, const T &b){
        mpfr_set_d(out.mpfr_ptr(), a, LAZY_MPFR_RND);
        mpfr_pow(out.mpfr_ptr(), out.mpfr_srcptr(), b.mpfr_srcptr(), LAZY_MPFR_RND);
    }

    // mpreal with int
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::POW, T &out, const T &a, const int &b){
        mpfr_pow_si(out.mpfr_ptr(), a.mpfr_srcptr(), b, LAZY_MPFR_RND);
    }

    LAZY_FORCE_INLINE static void evaluate(lazy::tags::POW, T &out, const int &a, const T &b){
        mpfr_set_si(out.mpfr_ptr(), a, LAZY_MPFR_RND);
        mpfr_pow(out.mpfr_ptr(), out.mpfr_srcptr(), b.mpfr_srcptr(), LAZY_MPFR_RND);
    }

    // mpreal with float
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::POW, T &out, const T &a, const float &b){
        mpfr_set_d(out.mpfr_ptr(), static_cast<double>(b), LAZY_MPFR_RND);
        mpfr_pow(out.mpfr_ptr(), a.mpfr_srcptr(), out.mpfr_srcptr(), LAZY_MPFR_RND);
    }

    LAZY_FORCE_INLINE static void evaluate(lazy::tags::POW, T &out, const float &a, const T &b){
        mpfr_set_d(out.mpfr_ptr(), static_cast<double>(a), LAZY_MPFR_RND);
        mpfr_pow(out.mpfr_ptr(), out.mpfr_srcptr(), b.mpfr_srcptr(), LAZY_MPFR_RND);
    }

    // mpreal with long
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::POW, T &out, const T &a, const long &b){
        mpfr_pow_si(out.mpfr_ptr(), a.mpfr_srcptr(), b, LAZY_MPFR_RND);
    }

    LAZY_FORCE_INLINE static void evaluate(lazy::tags::POW, T &out, const long &a, const T &b){
        mpfr_set_si(out.mpfr_ptr(), a, LAZY_MPFR_RND);
        mpfr_pow(out.mpfr_ptr(), out.mpfr_srcptr(), b.mpfr_srcptr(), LAZY_MPFR_RND);
    }

    // mpreal with size_t
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::POW, T &out, const T &a, const size_t &b){
        mpfr_pow_ui(out.mpfr_ptr(), a.mpfr_srcptr(), b, LAZY_MPFR_RND);
    }

    LAZY_FORCE_INLINE static void evaluate(lazy::tags::POW, T &out, const size_t &a, const T &b){
        mpfr_set_ui(out.mpfr_ptr(), a, LAZY_MPFR_RND);
        mpfr_pow(out.mpfr_ptr(), out.mpfr_srcptr(), b.mpfr_srcptr(), LAZY_MPFR_RND);
    }

    // min
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::MIN, T &out, const T &a, const T &b){
        mpfr_min(out.mpfr_ptr(), a.mpfr_srcptr(), b.mpfr_srcptr(), LAZY_MPFR_RND);
    }

    // max
    LAZY_FORCE_INLINE static void evaluate(lazy::tags::MAX, T &out, const T &a, const T &b){
        mpfr_max(out.mpfr_ptr(), a.mpfr_srcptr(), b.mpfr_srcptr(), LAZY_MPFR_RND);
    }




    using Mul_T_T = lazy::patterns::Multiplication<T, T>;
    using Add_T_T = lazy::patterns::Addition<T, T>;

    /// 3-operand sum: `a + (b + c)` via `mpfr_sum` for improved accuracy.
    LAZY_OVERRIDE_OPER(T, a, b, T, lazy::tags::PLUS, Add_T_T){
        // b is an Add expression with lhs and rhs
        T& a_mut = const_cast<T&>(a.value());
        T& b_lhs_mut = const_cast<T&>(b.get<0>().value());
        T& b_rhs_mut = const_cast<T&>(b.get<1>().value());
        mpfr_ptr tmp[3] = {a_mut.mpfr_ptr(), b_lhs_mut.mpfr_ptr(), b_rhs_mut.mpfr_ptr()};
        mpfr_sum(out.mpfr_ptr(), tmp, 3, LAZY_MPFR_RND);
    }

    /// Fused multiply-add: `a + b*c` via `mpfr_fma`.
    LAZY_OVERRIDE_OPER(T, a, b, T, lazy::tags::PLUS, Mul_T_T){
        // b is a Mul expression with lhs and rhs
        mpfr_fma(out.mpfr_ptr(),
                 b.get<0>().value().mpfr_srcptr(),
                 b.get<1>().value().mpfr_srcptr(),
                 a.value().mpfr_srcptr(),
                 LAZY_MPFR_RND);
    }

    /// Fused multiply-add (commuted): `a*b + c` via `mpfr_fma`.
    LAZY_OVERRIDE_OPER(T, a, b, Mul_T_T, lazy::tags::PLUS, T){
        mpfr_fma(out.mpfr_ptr(),
                 a.get<0>().value().mpfr_srcptr(),
                 a.get<1>().value().mpfr_srcptr(),
                 b.value().mpfr_srcptr(),
                 LAZY_MPFR_RND);
    }

    /// Fused multiply-subtract: `a*b - c` via `mpfr_fms`.
    LAZY_OVERRIDE_OPER(T, a, b, Mul_T_T, lazy::tags::MINUS, T){
        mpfr_fms(out.mpfr_ptr(),
                 a.get<0>().value().mpfr_srcptr(),
                 a.get<1>().value().mpfr_srcptr(),
                 b.value().mpfr_srcptr(),
                 LAZY_MPFR_RND);
    }

    /// Negated fused multiply-subtract: `c - a*b` via `mpfr_fms` followed by negation.
    LAZY_OVERRIDE_OPER(T, a, b, T, lazy::tags::MINUS, Mul_T_T){
        mpfr_fms(out.mpfr_ptr(),
                 b.get<0>().value().mpfr_srcptr(),
                 b.get<1>().value().mpfr_srcptr(),
                 a.value().mpfr_srcptr(),
                 LAZY_MPFR_RND);
        mpfr_neg(out.mpfr_ptr(), out.mpfr_srcptr(), LAZY_MPFR_RND);
    }

#if MPFR_VERSION >= MPFR_VERSION_NUM(4, 0, 0)
    /// Fused multiply-multiply-add: `a*b + c*d` via `mpfr_fmma` (MPFR >= 4.0).
    LAZY_OVERRIDE_OPER(T, a, b, Mul_T_T, lazy::tags::PLUS, Mul_T_T){
        mpfr_fmma(out.mpfr_ptr(),
                  a.get<0>().value().mpfr_srcptr(),
                  a.get<1>().value().mpfr_srcptr(),
                  b.get<0>().value().mpfr_srcptr(),
                  b.get<1>().value().mpfr_srcptr(),
                  LAZY_MPFR_RND);
    }

    /// Fused multiply-multiply-subtract: `a*b - c*d` via `mpfr_fmms` (MPFR >= 4.0).
    LAZY_OVERRIDE_OPER(T, a, b, Mul_T_T, lazy::tags::MINUS, Mul_T_T){
        mpfr_fmms(out.mpfr_ptr(),
                  a.get<0>().value().mpfr_srcptr(),
                  a.get<1>().value().mpfr_srcptr(),
                  b.get<0>().value().mpfr_srcptr(),
                  b.get<1>().value().mpfr_srcptr(),
                  LAZY_MPFR_RND);
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


namespace lazy {


using lazy::detail::isfinite;

/**
 * @brief Set the global default MPFR precision and resize all scratch buffers.
 *
 * Changes the precision for newly created `mpfr::mpreal` objects **and** resizes
 * every `mpfr::mpreal` scratch buffer registered in `LazyType<mpfr::mpreal>::workers`
 * (i.e. all thread-local temporaries created automatically for `mpfr::mpreal`
 * expressions).  This ensures that subsequent lazy evaluations use the new precision
 * throughout.
 *
 * @param prec  The new MPFR precision in bits (e.g. 256 for quad-like precision).
 */
inline void set_default_mpreal_prec(mpfr_prec_t prec){
    mpfr::mpreal::set_default_prec(prec);
    lazy::LazyType<mpfr::mpreal>::for_each_worker([prec](mpfr::mpreal& key){
        key.set_prec(prec);
    });
    mpfr::mpreal::set_default_prec(prec);
}

} // namespace lazy


#endif // MPFR_LAZY_HPP