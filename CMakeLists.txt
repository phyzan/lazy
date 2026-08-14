cmake_minimum_required(VERSION 3.14)

project(lazy)

add_library(lazy INTERFACE)

target_compile_features(lazy INTERFACE cxx_std_20)

target_include_directories(lazy
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set(LAZY_MPFR_RND "MPFR_RNDN" CACHE STRING
    "Rounding mode passed as LAZY_MPFR_RND to consumers of the lazy target (see include/lazy/apps/mpfrLazy.hpp). Leave empty to fall back to mpfr::mpreal::get_default_rnd() at runtime.")

if(LAZY_MPFR_RND)
    target_compile_definitions(lazy
        INTERFACE
            LAZY_MPFR_RND=${LAZY_MPFR_RND}
    )
endif()

# ----------- Optional MPFR support (lazy/apps/mpfrLazy.hpp) ---------------
# Kept separate from the core `lazy` target since most consumers never touch
# MPFR: link against `lazy::mpfr` instead of `lazy` to opt in to mpreal's
# include dir plus the mpfr/gmp system libraries. Gated by an explicit option
# rather than just checking whether external/mpreal exists: having the
# mpreal.h submodule checked out doesn't mean MPFR/GMP are actually installed
# and linkable, since the submodule may just be present from a blanket
# `git submodule update --init --recursive`.
option(LAZY_ENABLE_MPREAL "Enable MPFR support: defines the lazy::mpfr target (mpreal.h include dir, mpfr/gmp links). Requires the mpreal submodule and system MPFR/GMP. Does not affect the headers themselves — include lazy/apps/mpfrLazy.hpp explicitly to use it." OFF)

if(LAZY_ENABLE_MPREAL)
    if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/mpreal/mpreal.h)
        message(FATAL_ERROR "LAZY_ENABLE_MPREAL requires the 'mpreal' submodule. Run: git submodule update --init --recursive")
    endif()

    add_library(mpreal INTERFACE)
    target_include_directories(mpreal
        INTERFACE
            ${CMAKE_CURRENT_SOURCE_DIR}/external/mpreal
    )

    add_library(lazy_mpfr INTERFACE)
    add_library(lazy::mpfr ALIAS lazy_mpfr)

    target_link_libraries(lazy_mpfr
        INTERFACE
            lazy
            mpreal
            mpfr
            gmp
    )
endif()

# ----------- Dev-only targets (example) ---------------
# Only built when lazy is configured directly (not when pulled in via
# add_subdirectory/FetchContent by another project, e.g. xdiff), so
# consumers who only want the interface never build or even see this target.

if((DEFINED PROJECT_IS_TOP_LEVEL AND PROJECT_IS_TOP_LEVEL) OR
   (NOT DEFINED PROJECT_IS_TOP_LEVEL AND CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR))
    set(LAZY_BUILD_EXAMPLE_DEFAULT ON)
else()
    set(LAZY_BUILD_EXAMPLE_DEFAULT OFF)
endif()
option(LAZY_BUILD_EXAMPLE "Build the lazy example executable" ${LAZY_BUILD_EXAMPLE_DEFAULT})

if(LAZY_BUILD_EXAMPLE)
    if(LAZY_ENABLE_MPREAL)
        set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

        add_executable(example example.cpp)

        target_link_libraries(example
            PRIVATE
                lazy::mpfr
        )

        target_compile_options(example
            PRIVATE
                -O3
                -DNDEBUG
        )
    else()
        message(STATUS "Skipping the lazy example: it uses mpfr::mpreal, so it needs -DLAZY_ENABLE_MPREAL=ON")
    endif()
endif()
