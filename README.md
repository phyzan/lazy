<div align="center">

# ⚡ lazy

**A zero-overhead C++20 expression-template library for lazy arithmetic evaluation**

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue?logo=cplusplus&logoColor=white)](https://en.cppreference.com/w/cpp/20)
[![License: MIT](https://img.shields.io/badge/License-MIT-green?logo=opensourceinitiative&logoColor=white)](LICENSE)
[![MPFR](https://img.shields.io/badge/MPFR-optional-orange?logo=gnu&logoColor=white)](https://www.mpfr.org/)
[![Header-only](https://img.shields.io/badge/header--only-✓-brightgreen)](#installation)

---

*Write arithmetic naturally. Pay nothing at runtime.*

</div>

---

## ✨ Overview

**lazy** is a header-only C++20 library that turns ordinary arithmetic expressions into
compile-time expression trees. This allows users to write arithmetic naturally using standard math operators, while the library automatically
determines where temporary values are required for each operation. All intermediate temporaries are allocated once per thread at initialization,
and reused for all subsequent evaluations of the same expression shape.
This allows for avoiding unnecessary heap allocations and deallocations, that would otherwise require coding mathematical expressions using a non-portable, more verbose and less natural syntax.

The library already supports `mpfr::mpreal` for arbitrary-precision arithmetic, and can be extended to support any other type that can be optimized with custom evaluation rules.

Using mpfr::mpreal as an example, the following code demonstrates how this library uses portable code to optimize mathematical expressions when a non-builtin type is used, which uses heap allocation internally.

Compile with
```
g++ -std=c++20 -O3 -DNDEBUG -Iinclude test.cpp -o test -lmpfr -lgmp
```
to see how all template substitutions yield the same result. One can simply choose ```lazy::LazyType<mpfr::mpreal>``` instead of ```mpfr::mpreal``` on a templated function, and all mathematical operations, including comparisons (e.g. <, >, ==, etc.) and compound assignment (e.g. +=, -=, *=, /=) will compile and work as expected:

```cpp
#include <lazy/apps/mpfrLazy.hpp> // This already includes <mpreal.h> and <lazy/lazy.hpp>

using std::abs; // To enable ADL for abs() in MyFunc, since we are using std::abs for built-in types and lazy::abs for LazyType.

template<typename T>
T MyFunc(const T& x, const T& y, const T& z){
    // Just a random example of a mathematical expression that uses comparisons, compound assignment, and mathematical functions.
    if (x+y < pow(y, z)) {
        T result = -x / (y + abs(z));
        result += 4;
        return result;
    } else {
        return sqrt(x*x + y*y + z*z);
    }
}

template<typename T>
void perform_example(const char* type_name){
    T x = 5;
    T y = 7.345;
    T z = 3.14;

    T result = MyFunc<T>(x, y, z);

    std::cout << "Result: " << result << " using " << type_name << std::endl;
}

int main(){

    // Using double presision
    perform_example<double>("double");
    
    // Using mpfr::mpreal for arbitrary precision
    perform_example<mpfr::mpreal>("mpfr::mpreal");

    // Using lazy evaluation with mpfr::mpreal
    using lazy_mpreal = lazy::LazyType<mpfr::mpreal>;
    perform_example<lazy_mpreal>("lazy::LazyType<mpfr::mpreal>");

    return 0;
}
```


---




## 🚀 Key Features

| Feature | Description |
|---|---|
| 🌳 **Expression trees** | Arithmetic builds a statically-typed tree; computation is deferred |
| 🔌 **Pluggable rules** | Specialise `CustomBinaryRules<T>` / `CustomUnaryRules<T>` for type-specific kernels |
| 🧵 **Thread-safe temporaries** | Per-thread scratch allocation via `RuleTree`|
| 🔢 **MPFR ready** | Include `<lazy/apps/mpfrLazy.hpp>` for full `mpfr::mpreal` support with fused operations |
| 🏎️ **Zero overhead** | All paths are `LAZY_FORCE_INLINE`; the compiler sees flat arithmetic after optimisation |

---


# WARNING
> Expression nodes hold **`const&` references** to their operands — they do not copy values.
> If you store an expression in an `auto` variable and the referenced `LazyType` variables
> are later modified or go out of scope, evaluating the stored expression will read stale or
> dangling references and produce **undefined behaviour**.
>
> **Prefer evaluating immediately** rather than storing expressions long-term:
> ```cpp
> // ✅ Safe — evaluated before x or y can change
> mpfr::mpreal result = x * x + y * y;
>
> // ⚠️  Risky — expr holds references; must be evaluated before x/y change
> auto expr = x * x + y * y;
> x = 99.0;          // x changed!
> mpfr::mpreal r = expr;   // can to lead to unexpected results or UB, segfaults, etc.
> ```

### Evaluating

```cpp
mpfr::mpreal result;
expr.eval(result);   // writes into result

mpfr::mpreal result2 = expr;  // implicit conversion (creates a temporary T)
```

### Compound assignment

```cpp
x += y * y;    // evaluates y*y and adds to x.value_ in-place
x *= 2;
```

### Comparisons

Comparison operators return lazy `Comparison` nodes that convert to `bool`:

```cpp
if (x * x + y * y > 20.0) { /* ... */ }

auto cmp = x > y;      // Gt<double,...> node
bool result = cmp;     // evaluates lazily
```

---

## ⚙️ Customizing lazy evaluation for other types

Firstly, the types that you want a custom type to interact with must be specified, so that for other types, the mathematical operators will not be overloaded. For example, for mpfr::mpreal, as it is an arbitrary precision type, we want to allow lazy evaluation with types such as double, float, int, etc. For this purpose, the struct ```lazy::traits::ValidTypes<T>``` must be specialized.

This must be overriden for each type that the user requires lazy evaluation for.
It represents the types that can be implicitly converted to T. This explicit list is required to avoid converting from types that are built for different behavior but have implicit conversion operators to T, and converting them to T would result in a loss of the original behavior. So we prioritize the mathematical operator overloads of such other types over the implicit conversion to T. For example, if T is mpfr::mpreal, and there is a type Foo that has an implicit conversion operator to mpfr::mpreal, but Foo has its own operator overloads for +, -, *, /, etc., then we do not want to convert Foo to mpfr::mpreal and use the mpfr::mpreal operator overloads.

In order to specialize the ```ValidType``` struct and allow a type to interact with e.g. int, float, long and size_t for lazy evaluation (allowing them to be implicitly converted to the type), the following code can be used:

```cpp

template<>
struct lazy::traits::ValidTypes<MyType> {
    using type = lazy::traits::TypeList<double, int, float, long, size_t>;
};
```

Otherwise, if the allowed types follow a general pattern, it might be more convenient to specialize the ```lazy::traits::lazyConvertCondition``` bool:

```cpp
template<typename F>
constexpr bool lazy::traits::lazyConvertCondition<F, MyType> = std::is_arithmetic_v<F>;
```

For types that are are supposed to fully mimic arithmetic types, the macro ```LAZY_DECLARE_NUMERIC_TYPE(MyType)``` can be used, which will automatically specialize the lazyConvertCondition bool, and also specialize the std::numeric_limits for LazyType<MyType> (as long as std::numeric_limits<MyType> is already specialized).



### Define custom kernels for mathematical operations.

Some kernels are required so that all operations can be evaluated, but you can also provide optimized kernels for specific combinations of operands (e.g. a+b*c, if your type has a fused multiply-add optimized operation). The library chooses the specialized kernel (if it matches an algebraic expression) at compile-time, otherwise it falls back to the default implementation.

```cpp
template <>
struct CustomUnaryRules<MyType>
    : public UnaryOpRules<CustomUnaryRules<MyType>, MyType>{

    using T = MyType;
    using Base = UnaryOpRules<CustomUnaryRules<T>, T>;
    using Base::eval_rule;
    using Base::evaluate;

    // neg
    LAZY_FORCE_INLINE static void evaluate(NEG, T& out, const T& a){
        mpfr_neg(out.mpfr_ptr(), a.mpfr_srcptr(), mpfr::mpreal::get_default_rnd());
    }

    // abs
    LAZY_FORCE_INLINE static void evaluate(ABS, T& out, const T& a){
        mpfr_abs(out.mpfr_ptr(), a.mpfr_srcptr(), mpfr::mpreal::get_default_rnd());
    }

    // sqrt
    LAZY_FORCE_INLINE static void evaluate(SQRT, T& out, const T& a){
        mpfr_sqrt(out.mpfr_ptr(), a.mpfr_srcptr(), mpfr::mpreal::get_default_rnd());
    }

    // Define for all other mathematical functions that take a single argument intended to be used, and that a corresponding tag is defined in lazy_decls.hpp
};


template <>
struct CustomBinaryRules<MyType>
    : public BinaryOpRules<CustomBinaryRules<MyType>, MyType>{
    using T = MyType;
    using Base = BinaryOpRules<CustomBinaryRules<T>, T>;
    using Base::evaluate;
    using Base::eval_rule;

    // Define MyType + MyType
    LAZY_FORCE_INLINE static void evaluate(PLUS, T& out, const T& a, const T& b) {
        make_addition(out, a, b);  // call your type's addition kernel
    }

    // Define MyType + double
    LAZY_FORCE_INLINE static void evaluate(PLUS, T& out, const T& a, const double& b) {
        make_addition(out, a, b);
    }

    // Define all other operations (PLUS, MUL, DIV, MINUS, POW, MIN, MAX)
};
```


## 🧵 Thread Safety

Each unique sub-expression branch of an algebraic tree gets its own **thread-local** scratch array via `RuleTree`. This means:

- ✅ Multiple threads can evaluate expressions of the **same type** concurrently
- ✅ The global `Rules<T>::aux_ptrs` registry is protected by a `shared_mutex`
- ⚠️ A single `LazyType<T>` variable must not be written from multiple threads
  simultaneously (it is not atomic)
