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
compile-time expression trees. Operations are never computed until you ask for them —
and when they are, the library uses the most efficient path available via customisable
evaluation rules.

```cpp
using namespace lazy;
using T = mpfr::mpreal;

LazyType<T> x = 5.0, y = 7.345, z = 3.14;

// This builds a type-encoded expression tree — no arithmetic yet
auto expr = x * x + y * y + z * z;

// Evaluate on demand
T result;
expr.eval(result);

// Or implicitly
T result2 = expr;  // same thing
```

---

## 🚀 Key Features

| Feature | Description |
|---|---|
| 🌳 **Expression trees** | Arithmetic builds a statically-typed tree; computation is deferred |
| 🔌 **Pluggable rules** | Specialise `CustomBinaryRules<T>` / `CustomUnaryRules<T>` for type-specific kernels |
| 🧵 **Thread-safe temporaries** | Per-thread scratch allocation via `RuleTree` — no heap, no locks on hot paths |
| 🔢 **MPFR ready** | Include `mpfrLazy.hpp` for full `mpfr::mpreal` support with fused operations |
| 🏎️ **Zero overhead** | All paths are `LAZY_FORCE_INLINE`; the compiler sees flat arithmetic after optimisation |
| 📐 **Pattern rewriting** | Intercept `a*b + c` and replace with `mpfr_fma` automatically |

---

## 📁 File Structure

```
include/
├── patterns.hpp      — Compile-time symbolic pattern types (no dependencies)
├── lazy.hpp          — Core expression-template engine
└── mpfrLazy.hpp      — mpfr::mpreal specialisations (requires libmpfr + libgmp)
```

---

## 📦 Installation

**lazy** is header-only. Copy the `include/` directory or use `make install`:

```sh
make install                    # installs to /usr/local/include/lazy/
make install PREFIX=~/.local    # or a custom prefix
```

Then include:

```cpp
#include <lazy/lazy.hpp>          // core library
#include <lazy/mpfrLazy.hpp>      // + MPFR specialisations
```

---

## 🔧 Usage

### Declaring variables

```cpp
using namespace lazy;

LazyType<double> x = 3.0;   // owning variable
LazyType<double> y = 4.0;
```

### Building expressions

```cpp
auto expr = x * x + y * y;  // returns Add<double, Mul<...>, Mul<...>> — no work done
```

Scalars of other types are promoted automatically:

```cpp
auto expr2 = x * 2 + 1.5;   // 2 → OtherType<double,int>, 1.5 → OtherType<double,double>
```

> [!WARNING]
> Expression nodes hold **`const&` references** to their operands — they do not copy values.
> If you store an expression in an `auto` variable and the referenced `LazyType` variables
> are later modified or go out of scope, evaluating the stored expression will read stale or
> dangling references and produce **undefined behaviour**.
>
> **Prefer evaluating immediately** rather than storing expressions long-term:
> ```cpp
> // ✅ Safe — evaluated before x or y can change
> double result = x * x + y * y;
>
> // ⚠️  Risky — expr holds references; must be evaluated before x/y change
> auto expr = x * x + y * y;
> x = 99.0;          // x changed!
> double r = expr;   // can to lead to unexpected results or UB, segfaults, etc.
> ```

### Evaluating

```cpp
double result;
expr.eval(result);   // writes into result

double result2 = expr;  // implicit conversion (creates a temporary T)
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

## ⚙️ Custom Evaluation Rules

Override how operations are computed for your type using the specialisation macros:

```cpp
SPECIALIZE_OPERATIONS(MyFloat) {
    using T = MyFloat;
    using Base = BinaryOpRules<CustomBinaryRules<T>, T>;
    using Base::evaluate;
    using Base::eval_rule;

    // Override + for T op T
    EVALUATE_OPER(T, a, b, T, PLUS, T) {
        out = fast_add(a, b);   // your optimised kernel
    }

    // Override + for T op double
    EVALUATE_OPER(T, a, b, T, PLUS, double) {
        out = fast_add_double(a, b);
    }
};

SPECIALIZE_FUNCTIONS(MyFloat) {
    using T = MyFloat;
    using Base = UnaryOpRules<CustomUnaryRules<T>, T>;
    using Base::evaluate;

    EVALUATE_FUNC(T, a, SQRT, T) { out = fast_sqrt(a); }
    EVALUATE_FUNC(T, a, ABS,  T) { out = fast_abs(a);  }
};
```

---

## 🎯 Pattern Matching & Fused Operations


Use `OVERRIDE_OPER` inside a `SPECIALIZE_OPERATIONS` block:

```cpp
SPECIALIZE_OPERATIONS(MyFloat) {
    using T    = MyFloat;
    using Base = BinaryOpRules<CustomBinaryRules<T>, T>;
    using Base::evaluate; using Base::eval_rule;

    using Mul_T_T = Multiplication<T, T>;

    // Intercept a*b + c  →  fused multiply-add
    OVERRIDE_OPER(T, a, b, Mul_T_T, PLUS, T) {
        out = fma(a.lhs.value(), a.rhs.value(), b.value());
    }
};
```

The built-in `mpfrLazy.hpp` uses this to map:

| Expression pattern | MPFR function   | Notes |
|---|---|---|
| `a + (b + c)` | `mpfr_sum` | 3-operand compensated sum |
| `a*b + c` | `mpfr_fma` | Fused multiply-add |
| `a*b - c` | `mpfr_fms` | Fused multiply-subtract |
| `c - a*b` | `mpfr_fms` + neg | — |
| `a*b + c*d` | `mpfr_fmma` | MPFR ≥ 4.0 |
| `a*b - c*d` | `mpfr_fmms` | MPFR ≥ 4.0 |

---

## 🔢 MPFR Support

Include `mpfrLazy.hpp` for arbitrary-precision arithmetic via `mpfr::mpreal`:

```cpp
#include <lazy/mpfrLazy.hpp>

lazy::set_default_mpreal_prec(256);  // set precision (resizes all scratch buffers)

using namespace lazy;
LazyType<mpfr::mpreal> x = "3.141592653589793238462643383279502884197";
LazyType<mpfr::mpreal> y = 1;

auto expr = sqrt(x * x + y * y);
mpfr::mpreal result = expr;
```

### Precision management

`set_default_mpreal_prec(prec)` changes the MPFR default precision **and** resizes
every thread-local scratch buffer registered in `Rules<mpfr::mpreal>`, so the new
precision is reflected in all intermediate computations immediately.

---

## 🧩 Type Hierarchy

```
ExprBase<T>
└── Expr<Derived, T>
    ├── Atom<Derived, T>        ← leaf nodes (already have a value)
    │   ├── LazyType<T>         ← owning variable (user-facing)
    │   ├── RefType<T>          ← const T& view (auto-created by make_expr)
    │   └── OtherType<T, S>     ← scalar S inside a T-typed tree
    └── Node<Derived, T, N>     ← unevaluated composite nodes
        ├── BinaryOperator<...>
        │   ├── Add, Sub, Mul, Div, Pow
        │   ├── MaxLazy, MinLazy
        │   └── Comparison → Eq, Neq, Gt, Lt, Ge, Le
        └── Unary<...>
            ├── Neg
            ├── Abs
            └── Sqrt
```

---

## 🧵 Thread Safety

Each unique expression shape `(T, L, R)` gets its own **thread-local** scratch
array via `RuleTree<T, L, R>`. This means:

- ✅ Multiple threads can evaluate expressions of the **same type** concurrently
- ✅ The global `Rules<T>::aux_ptrs` registry is protected by a `shared_mutex`
- ⚠️ A single `LazyType<T>` variable must not be written from multiple threads
  simultaneously (it is not atomic)

---

## 📐 Pattern Types (patterns.hpp)

`patterns.hpp` defines a parallel type-level mirror of the expression tree for compile-time structural reasoning. Patterns are zero-size types that compose with normal C++ operators:

```cpp
struct X : lazy::Pattern {};
struct Y : lazy::Pattern {};

using Pat = decltype(X{} * Y{} + X{});
// Pat == Addition<Multiplication<X,Y>, X>

static_assert(lazy::isAnyAddition<Pat>);
```

Operators and functions available at pattern level: `+`, `-`, `*`, `/`, unary `-`,
`pow`, `abs`, `sqrt`, `min`, `max`, `<`, `>`, `==`, `!=`, `<=`, `>=`.

---

## 🏗️ Building & Testing

```sh
# Build the test binary
g++ -std=c++20 -O3 -march=native test.cpp -o test -lmpfr -lgmp

# Run
./test
```

Expected output (times will vary):
```
Time taken:                   142 ms
Time taken with ConcreteType: 91 ms
```

---

## 📋 API Quick Reference

### Macros

| Macro | Purpose |
|---|---|
| `SPECIALIZE_OPERATIONS(T)` | Open a `CustomBinaryRules<T>` specialisation block |
| `SPECIALIZE_FUNCTIONS(T)` | Open a `CustomUnaryRules<T>` specialisation block |
| `EVALUATE_OPER(T,a,b,L,tag,R)` | Declare a typed `evaluate` overload in a binary rules block |
| `EVALUATE_FUNC(T,a,tag,A)` | Declare a typed `evaluate` overload in a unary rules block |
| `OVERRIDE_OPER(T,a,b,LP,tag,RP)` | Intercept a specific expression-pattern in an `eval_rule` overload |
| `DEFINE_UNARY_OP(func,Op,Tag,Pat)` | Declare a new unary node type + free function in one go |
| `LAZY_DEFINE_RELATIONAL_OP(op,Name)` | Declare a relational operator producing a `Comparison` node |
| `DECLARE_LAZY_NUMERIC_TYPE(T)` | Specialise `std::numeric_limits<LazyType<T>>` |

### Key free functions

| Function | Returns |
|---|---|
| `make_expr<T>(v)` | Canonicalise `v` into the appropriate expression atom |
| `make_add<T>(l,r)` / `make_mul` / … | Build a specific binary node |
| `make_neg<T>(a)` | Build a negation node |
| `abs(expr)` | `Abs<T,E>` node |
| `sqrt(expr)` | `Sqrt<T,E>` node |
| `pow(base, exp)` | `Pow<T,L,R>` node |
| `max(l, r)` / `min(l, r)` | `MaxLazy` / `MinLazy` node |
| `set_default_mpreal_prec(p)` | Set MPFR precision and resize all scratch storage |
| `isfinite(LazyType<mpreal>)` | Finite check for lazy mpreal variables |

---
