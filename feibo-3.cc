#include "dbg.h"
#include <atomic>
#include <cassert>
#include <coroutine>
#include <iostream>
#include <sstream>
#include <thread>
#include <tuple>
#include <vector>

template <typename... Args> std::string get_string(Args &&...args)
{
    // return (std::to_string(args) + ...);
    // return ((std::to_string(args) + " ") + ...);
    std::stringstream ss;
    ((ss << args << " "), ...);
    return ss.str();
}

using Buffer = std::vector<int>;

class FibonacciGenerator
{
  public:
    FibonacciGenerator()
    {
    }

    FibonacciGenerator(uint64_t _coro_id, std::vector<int> &_need) : coro_id(_coro_id), needs(_need)
    {
        dbg(coro_id);
    }

    ~FibonacciGenerator()
    {
        auto info = get_string("free", coro_id);
        dbg(info);
    }

    FibonacciGenerator(FibonacciGenerator &&other)
    {
        dbg("rvalue");
    }

    FibonacciGenerator(const FibonacciGenerator &other)
    {
        dbg("lvalue");
    }

    FibonacciGenerator &operator=(const FibonacciGenerator &obj)
    {
        dbg("copy");
        return *this;
    }

    FibonacciGenerator &operator=(FibonacciGenerator &&obj)
    {
        dbg("move");
        return *this;
    }

  private:
    uint64_t coro_id;
    std::vector<int> needs;      // 要求解F(n)的n
    std::vector<int> middle_res; // 对每个n的中间计算结果，也就是F(1)~F(n)
};

int main()
{
    dbg("start");
    std::vector<Buffer> need{{11, 21, 31, 5}, {10, 20, 5, 15}};
    int num_coro = 2;
    std::vector<FibonacciGenerator> fibo_coros;
    fibo_coros.reserve(10);
    FibonacciGenerator fibo_coro_0(3, need[0]);
    FibonacciGenerator fibo_coro_1(4, need[1]);
    for (int i = 0; i < num_coro; i++)
    {
        fibo_coros.emplace_back(i, need[i]);
        dbg("");
    }
    {
        dbg("");
    }

    return 0;
}
