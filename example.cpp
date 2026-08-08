#include <lazy/apps/mpfrLazy.hpp>
#include <chrono>

int main(){

    auto f = [](auto& res, const auto& x, const auto& y, const auto& z){
        res += abs(7 + -(x+10) * (y - 1) / (x * x * x)
             + abs(z * x / (y - 1))
             - x * y * z / (x + z)
             + z * z / (x - z * (y / (x * x)))
             - (-y + z * z) * x / y
             + x / (z + x * y / 4 - 2 * (7 / y))
             - (x * x - y * y) / (x * y - z * z / x)
             + (x + y) * (-(z / x) + y / (x * x)));
    };
    
    using T = mpfr::mpreal;
    T res = 1;
    T x = 5;
    T y = 7.345;
    T z = 3.14;

    int n = 100000;
    auto t_start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < n; ++i){
        f(res, x, y, z);
    }
    auto t_end = std::chrono::high_resolution_clock::now();

    auto time_T = std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count();

    // now with LazyType<T>
    using LT = lazy::LazyType<T>;
    LT res2 = 1;
    LT x2 = 5;
    LT y2 = 7.345;
    LT z2 = 3.14;
    auto t_start2 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < n; ++i){
        f(res2, x2, y2, z2);
    }
    auto t_end2 = std::chrono::high_resolution_clock::now();
    auto time_CT = std::chrono::duration_cast<std::chrono::milliseconds>(t_end2 - t_start2).count();

    std::cout << "Time taken:                   " << time_T << " ms" << std::endl;
    std::cout << "Time taken with LazyType:     " << time_CT << " ms" << std::endl;
    std::cout << "Value with mpreal:            " << res << std::endl; 
    std::cout << "Value with LazyType        :  " << res2 << std::endl;
    std::cout << "Speedup:                      " << (double(time_T) / (double)time_CT) << "x" << std::endl;

    // g++ -O3 -std=c++20 -DNDEBUG -DLAZY_MPFR_RND=MPFR_RNDN -Iinclude example.cpp -o test -lmpfr -lgmp
    // test with clang++

}