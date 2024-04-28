#include "dbg.h"
#include <cassert>
#include <coroutine>
#include <iostream>
#include <sstream>
#include <vector>

template <typename... Args>
std::string get_string(Args &&...args)
{
    std::stringstream ss;
    ((ss << args << " "), ...);
    return ss.str();
}

struct CalPromise
{
    std::coroutine_handle<> m_continuation;
};

using PassPrinBuffer = std::vector<std::vector<int> *>;

class FibonacciGenerator
{
  public:
    FibonacciGenerator(uint64_t _coro_id, std::vector<int> &_need, PassPrinBuffer::iterator _iter)
        : coro_id(_coro_id), needs(_need), pass_buf(_iter)
    {
        dbg(coro_id);
    }

    ~FibonacciGenerator()
    {
        dbg("free", coro_id);
    }

    int cal_feibo(int n, int a = 0, int b = 1)
    {
        if (n == 0)
            return a;
        if (n == 1)
            return b;
        middle_res.push_back(a + b);
        return cal_feibo(n - 1, b, a + b);
    }

    CalPromise cal_and_print()
    {
        for (auto need : needs)
        {
            auto info = get_string("cal", need);
            dbg(info);
            middle_res.clear();
            cal_feibo(need);
            int cnt = 1;
            for (auto mid_res : middle_res)
            {
                auto info = get_string(cnt++, mid_res);
                dbg(info);
            }
            *pass_buf = &middle_res;
            co_await print_buffer();
        }
        co_return CalPromise{std::noop_coroutine()};
    }

  private:
    uint64_t coro_id;
    std::vector<int> needs;
    std::vector<int> middle_res;
    PassPrinBuffer::iterator pass_buf;
};

std::coroutine_handle<> g_continuation;

void print_thread_loop()
{
    while (true)
    {
        // 这里编写打印线程的逻辑
        std::this_thread::sleep_for(std::chrono::seconds(1)); // 假设每秒钟打印一次
        if (g_continuation)
        {
            g_continuation.resume();
        }
    }
}

std::coroutine<void> print_buffer()
{
    co_await std::suspend_always{};
    // 这里编写从 pass_buf 中读取中间结果并打印的逻辑
    auto middle_res = **pass_buf;
    dbg("Printing buffer:");
    for (auto mid_res : middle_res)
    {
        dbg(mid_res);
    }
    g_continuation = co_await std::suspend_always{};
}

int main()
{
    dbg("start");
    std::vector<int> need{10, 20, 5, 15};
    int num_coro = 1;
    PassPrinBuffer pass_buffer(num_coro);
    auto begin = pass_buffer.begin();
    FibonacciGenerator fibeo_0(0, need, begin);
    auto promise = fibeo_0.cal_and_print();
    g_continuation = promise.m_continuation;
    std::thread print_thread(print_thread_loop);
    print_thread.detach();
    return 0;
}
