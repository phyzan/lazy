<div align="center">

# ⚡ lazy

**A zero-overhead C++20 expression-template library for lazy arithmetic evaluation**

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue?logo=cplusplus&logoColor=white)](https://en.cppreference.com/w/cpp/20)
[![License: MIT](https://img.shields.io/badge/License-MIT-green?logo=opensourceinitiative&logoColor=white)](LICENSE)
[![MPFR](https://img.shields.io/badge/MPFR-optional-orange?logo=gnu&logoColor=white)](https://www.mpfr.org/)
[![Header-only](https://img.shields.io/badge/header--only-✓-brightgreen)](#installation)

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
| 🔌 **Pluggable rules** | Specialise `CustomBinaryEvaluator<T>` / `CustomUnaryEvaluator<T>` for type-specific kernels |
| 🔢 **MPFR ready** | Include `<lazy/apps/mpfrLazy.hpp>` for full `mpfr::mpreal` support with fused operations |
| 🏎️ **Zero overhead** | All paths are `LAZY_FORCE_INLINE`; the compiler sees flat arithmetic after optimisation |

---

## Installation

Clone and initialize submodules (needed for MPFR support via `external/mpreal`):
```bash
git submodule update --init --recursive
```

## Macros

| CMake Option | Macro | Effect |
|--------------|-------|--------|
| `LAZY_MPFR_RND` | `LAZY_MPFR_RND` | Sets the rounding mode for MPFR operations. Set it to `MPFR_RNDN` for most use cases, or do not set it at all (then the default rounding mode is called at each operation). |
| `LAZY_ENABLE_MPREAL` | - | Only used when building the example. Simply links against the `mpfr` and `gmp` libraries. |

**Linking via CMake:**
```cmake
add_subdirectory(path/to/lazy)
target_link_libraries(your_target PRIVATE lazy)  # core lazy/lazy.hpp only
```
This propagates the required C++20 standard and `LAZY_MPFR_RND` (see [Notes for `LazyType<mpfr::mpreal>`](#notes-for-lazytypempfrmpreal) below) automatically.

MPFR support (`lazy/apps/mpfrLazy.hpp`) is opt-in, via the `LAZY_ENABLE_MPREAL` option — plain `add_subdirectory` doesn't require MPFR/GMP to be installed at all, since having the `mpreal` submodule checked out doesn't guarantee they're actually available/linkable:
```bash
cmake -S . -B build -DLAZY_ENABLE_MPREAL=ON -DLAZY_MPFR_RND=MPFR_RNDN
```
```cmake
target_link_libraries(your_target PRIVATE lazy::mpfr)  # adds mpreal.h + links mpfr/gmp
```
`lazy::mpfr` only exists when `LAZY_ENABLE_MPREAL` is `ON`. It only affects the build (include dir, library links) — it isn't a preprocessor macro and has no effect on the headers themselves, so you still need to `#include <lazy/apps/mpfrLazy.hpp>` explicitly wherever you use `mpfr::mpreal`/`LazyType<mpfr::mpreal>` (it isn't pulled in automatically by `<lazy/lazy.hpp>`).

**Without CMake:**
```bash
g++ -std=c++20 -O3 -DNDEBUG -DLAZY_MPFR_RND=MPFR_RNDN -Iinclude -Iexternal/mpreal test.cpp -o test -lmpfr -lgmp
```

**Syntax highlighting (clangd):** configure the build from this directory's own root so `compile_commands.json` ends up directly in `build/`, which clangd discovers automatically:
```bash
cmake -S . -B build
```

**Running the example** (needs `LAZY_ENABLE_MPREAL`, since it uses `mpfr::mpreal`):
```bash
cmake -S . -B build -DLAZY_ENABLE_MPREAL=ON -DLAZY_MPFR_RND=MPFR_RNDN
cmake --build build
./build/example
```

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

Firstly, the types that you want a ```LazyType<YourType>``` to interact with must be specified, so that for other types, the mathematical operators will not be overloaded. For example, for mpfr::mpreal, as it is an arbitrary precision type, we want to allow lazy evaluation with types such as double, float, int, etc. For this purpose, the struct ```lazy::traits::ValidTypes<T>``` must be specialized.

This must be overridden for each type that the user requires lazy evaluation for.
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


The required kernels that must be overriden are all the basic overloads of the `evaluate` function, whose main parameters are the `out` parameter that will hold the result of the operation, and the input operands (raw values that have been declared to be compatible with the custom type), as in the examples below.

**Important**: For `evaluate` overloads where any of the input operands is the same type as `out`, keep in mind that they might be the same memory location, so make sure that any operation that modifies `out`, does so safely and correctly handles potential aliasing with the input operands.
If intermediate evaluations are required, make sure to template-specialize the `constexpr size_t lazy::required_workers` number (default is 0) to be the minimum number that is used in any of the `evaluate` overloads for your custom type. For `mpfr::mpreal`, this is done as follows:

```cpp
template<>
inline constexpr size_t lazy::required_workers<mpfr::mpreal> = 1;
```
because some overloads like this:
```cpp
LAZY_FORCE_INLINE static void evaluate(lazy::tags::DIV, T &out, Pool<T> workers, const int &a, const T &b){
    T& worker = workers.consume();
    mpfr_set_si(worker.mpfr_ptr(), a, LAZY_MPFR_RND);
    mpfr_div(out.mpfr_ptr(), worker.mpfr_srcptr(), b.mpfr_srcptr(), LAZY_MPFR_RND);
}
```
evaluate an intermediate result using a worker from the pool before performing the final operation on the `out` parameter.

Optionally, the `eval_rule` function can be overriden to provide custom evaluation rules for specific operations, and its inputs are the `out` parameter that will hold the result of the operation, and node-like objects representing specific algebraic expressions. In this case, no caution regarding aliasing with the input operands is necessary, as `out` is guaranteed not to alias with any object in any of the input expression trees.

See the [example](include/lazy/apps/mpfrLazy.hpp) for a complete implementation of custom kernels for the `mpfr::mpreal` type.

```cpp
template <>
struct CustomUnaryEvaluator<MyType>
    : public UnaryEvaluator<CustomUnaryEvaluator<MyType>, MyType>{

    using T = MyType;
    using Base = UnaryEvaluator<CustomUnaryEvaluator<T>, T>;
    using Base::eval_rule;
    using Base::evaluate;

    // neg
    LAZY_FORCE_INLINE static void evaluate(NEG, T& out, Pool<T> /**/, const T& a){
        mpfr_neg(out.mpfr_ptr(), a.mpfr_srcptr(), mpfr::mpreal::get_default_rnd());
    }

    // abs
    LAZY_FORCE_INLINE static void evaluate(ABS, T& out, Pool<T> /**/, const T& a){
        mpfr_abs(out.mpfr_ptr(), a.mpfr_srcptr(), mpfr::mpreal::get_default_rnd());
    }

    // sqrt
    LAZY_FORCE_INLINE static void evaluate(SQRT, T& out, Pool<T> /**/, const T& a){
        mpfr_sqrt(out.mpfr_ptr(), a.mpfr_srcptr(), mpfr::mpreal::get_default_rnd());
    }

    // Define for all other mathematical functions that take a single argument intended to be used, and that a corresponding tag is defined in lazy_decls.hpp
};


template <>
struct CustomBinaryEvaluator<MyType>
    : public BinaryEvaluator<CustomBinaryEvaluator<MyType>, MyType>{
    using T = MyType;
    using Base = BinaryEvaluator<CustomBinaryEvaluator<T>, T>;
    using Base::evaluate;
    using Base::eval_rule;

    // Define MyType + MyType
    LAZY_FORCE_INLINE static void evaluate(PLUS, T& out, Pool<T> /**/, const T& a, const T& b) {
        make_addition(out, a, b);  // call your type's addition kernel
    }

    // Define MyType + double
    LAZY_FORCE_INLINE static void evaluate(PLUS, T& out, Pool<T> /**/, const T& a, const double& b) {
        make_addition(out, a, b);
    }

    // Define all other operations (PLUS, MUL, DIV, MINUS, POW, MIN, MAX)
};
```


## 🧵 Thread Safety

Each thread allocates its own scratch space for temporary values, so that multiple threads can evaluate the same expression concurrently. However, a single `LazyType<T>` variable must not be written from multiple threads simultaneously, given that its underlying type `T` is not thread-safe.

### Updating cached workers

That per-thread scratch space is allocated once and reused for every subsequent evaluation — it is **not** reconstructed on each use. If you change some global/static state that affects how `T` behaves (e.g. a global precision, locale, or rounding mode), already-allocated workers keep reflecting whatever state was in effect when they were first created, and will silently go stale unless you update them yourself, via `LazyType<T>::for_each_worker`:

```cpp
template<typename F>
static void LazyType<T>::for_each_worker(F&& fn); // fn is called with a T& for every cached worker
```

For example, `lazy::set_default_mpreal_prec` (in `lazy/apps/mpfrLazy.hpp`) uses this to keep `mpfr::mpreal` workers consistent after changing the global default precision:

```cpp
inline void set_default_mpreal_prec(mpfr_prec_t prec){
    mpfr::mpreal::set_default_prec(prec);
    lazy::LazyType<mpfr::mpreal>::for_each_worker([prec](mpfr::mpreal& key){
        key.set_prec(prec);
    });
}
```

**Note:** `workers` is `thread_local`, so `for_each_worker` only updates the calling thread's own cache. In a multi-threaded program, each thread that uses `LazyType<T>` must call it independently after a relevant global change — there is no way to refresh another thread's workers from outside that thread.


## Notes for `LazyType<mpfr::mpreal>`

Via CMake, this requires linking `lazy::mpfr` instead of `lazy` (see [Installation](#installation)), which only exists when configured with `-DLAZY_ENABLE_MPREAL=ON`.

The `mpfr` library provides functions for each operation (e.g. addition, multiplication, etc.) that take a rounding mode as an argument. The `LazyType<mpfr::mpreal>` specialization uses these functions, and by default, it passes `mpfr::mpreal::get_default_rnd()` in each operation. However, for most use cases, rounding to nearest is sufficient, so hardcoding `MPFR_RNDN` in all lazy operations will increase performance. In order to hardcode the rounding mode, the user can pass the macro `LAZY_MPFR_RND` with the desired rounding mode found in mpfr.h, e.g. `-DLAZY_MPFR_RND=MPFR_RNDN` to the compiler.

**CMake:** when consuming the `lazy` target (directly, or transitively through another target like `xdiff`), `LAZY_MPFR_RND` is controlled by a cache variable of the same name, which defaults to `MPFR_RNDN` — so CMake builds are hardcoded to `MPFR_RNDN` out of the box, unlike the dynamic `get_default_rnd()` default described above for manual compilation. Override it with `cmake -DLAZY_MPFR_RND=MPFR_RNDZ ...` (or `set(LAZY_MPFR_RND MPFR_RNDZ CACHE STRING "" FORCE)` before `add_subdirectory` in a consuming project). Set it to empty (`-DLAZY_MPFR_RND=`) to leave the macro undefined and restore the dynamic `get_default_rnd()` behavior.