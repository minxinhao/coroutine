#include "dbg.h"
#include <atomic>
#include <cassert>
#include <coroutine>
#include <iostream>
#include <random>
#include <sstream>
#include <thread>
#include <tuple>
#include <vector>

//
// 斐波那契的计算和输出使用协程进行调度
// 每个FibonacciGenerator接受一个vector，其中是要计算的斐波那契数<1,3,5,9,7,100,22,...>
// 对于每个斐波那契数n，计算过程计算保存F(1)~F(n)，并保存在一个res vector中。
// 打印过程则分别打印F(1)~F(n)的结果。这样使得对每个n，其计算和打印都有一定耗时。
// 在一个线程中，使用1～m个协程来完成给定的vector中所有数的计算和打印
//
// 每个FibonacciGenerator接受一段子vector输入，制定要计算的斐比那契数
// 然后提供一个co_await cal_and_prin()接口,逐个计算和打印每个数的计算结果
// cal_and_print在被调用时计算结果，并写入到一个指定区域
// 一个额外的打印线程会读取改地址中的计算结果并打印。打印结束后会写入一个状态为表示该结果已经被打印。
// 协程调度器在调度时会读取到该状态，并唤醒对应协程。

template <typename... Args> std::string get_string(Args &&...args)
{
    // return (std::to_string(args) + ...);
    // return ((std::to_string(args) + " ") + ...);
    std::stringstream ss;
    ((ss << args << " "), ...);
    return ss.str();
}

struct CalPromise
{
    struct promise_type
    {
        CalPromise get_return_object() noexcept
        {
            std::cout << "get return object" << std::endl;
            return CalPromise{};
        }

        auto initial_suspend() noexcept
        {
            std::cout << "initial suspend, return never" << std::endl;
            return std::suspend_never{};
        }

        auto final_suspend() noexcept
        {
            std::cout << "final suspend, return never" << std::endl;
            return std::suspend_never{};
        }

        void unhandled_exception()
        {
            std::cout << "unhandle exception" << std::endl;
            std::terminate();
        }

        void return_void()
        {
            std::cout << "return void" << std::endl;
            return;
        }
    };
};

using PassPrinBuffer = std::vector<std::atomic_uintptr_t>;
using Buffer = std::vector<int>;
using CoroHandle = std::vector<std::coroutine_handle<>>;

struct PrintAwaiter
{
    CoroHandle &handle;
    PassPrinBuffer &pass_buffer;
    int num_coro;
    int coro_id;
    PrintAwaiter(CoroHandle &h, PassPrinBuffer &p, int n, int id) : handle(h), pass_buffer(p), num_coro(n), coro_id(id)
    {
    }

    bool await_ready()
    {
        return false;
    }

    void await_resume()
    {
        dbg("resume");
    }

    void await_suspend(std::coroutine_handle<> h)
    {
        handle[coro_id] = h;
        // 在handle中查找可以被唤醒的协程
        // 也就是pass_buffer中对应buffer为nullptr的部分
        auto find_free_coro = [&]() -> std::tuple<bool, int> {
            int free_coro_id = (coro_id + 1) % num_coro;
            bool free_coro = false;
            for (int cnt = 0; cnt < num_coro; cnt++)
            {
                if (pass_buffer[free_coro_id].load() == 0)
                {
                    free_coro = true;
                    break;
                }
                free_coro_id = (free_coro_id + 1) % num_coro;
            }
            return std::make_tuple(free_coro, free_coro_id);
        };

        int free_coro_id;
        while (true)
        {
            auto [has_free_coro, coro_id] = find_free_coro();
            free_coro_id = coro_id;
            if (has_free_coro)
                break;
        }
        if (handle[free_coro_id] == std::coroutine_handle<>(nullptr))
        {
            // 这个协程还没有启动，直接返回，返回到上层调用者进行启动
            auto info = get_string(free_coro_id, " hasn't been started. So return to main.");
            dbg(info);
            return;
        }
        auto info = get_string("coro", coro_id, "resume", "free_coro_id", free_coro_id);
        dbg(info);
        handle[free_coro_id].resume();
    }
};

class FibonacciGenerator
{
  public:
    FibonacciGenerator(uint64_t _coro_id, std::vector<int> &_need) : coro_id(_coro_id), needs(_need)
    {
        dbg(coro_id);
    }

    ~FibonacciGenerator()
    {
        auto info = get_string("free", coro_id);
        dbg(info);
    }

    // 尾递归优化
    int cal_feibo(int n, int a = 0, int b = 1)
    {
        if (n == 0)
            return a;
        if (n == 1)
            return b;
        middle_res.push_back(a + b);
        return cal_feibo(n - 1, b, a + b);
    }

    CalPromise cal_and_print(int num_coro, CoroHandle &handle, PassPrinBuffer &pass_buffer)
    {
        for (auto need : needs)
        {
            // 计算并打印f(1)~f(n)
            auto info = get_string("coro", coro_id, "cal", need);
            dbg(info);
            middle_res.clear();
            middle_res.push_back(need);
            cal_feibo(need);

            pass_buffer[coro_id].store((uintptr_t)&middle_res);
            dbg(pass_buffer[coro_id]);
            co_await PrintAwaiter(handle, pass_buffer, num_coro, coro_id);
        }
    }

  private:
    uint64_t coro_id;
    std::vector<int> needs;      // 要求解F(n)的n
    std::vector<int> middle_res; // 对每个n的中间计算结果，也就是F(1)~F(n)
};

void print_thread_loop(int num_coro, std::atomic_bool &exit_flag, PassPrinBuffer &pass_buffer)
{
    while (!exit_flag.load())
    {
        // 扫描PassPrinBuffer中是否有可以打印的内容
        for (int coro_id = 0; coro_id < num_coro; coro_id++)
        {
            uintptr_t ptr = pass_buffer[coro_id].load();
            if (ptr == 0)
                continue;
            Buffer *buffer = reinterpret_cast<Buffer *>(ptr);
            auto info = get_string("coro", coro_id, "cal", buffer->front());
            dbg(info);
            // dbg(*buffer);
            for (auto buf : *buffer)
            {
                dbg(buf);
            }
            pass_buffer[coro_id++].store(0);
        }
    }
}

std::vector<Buffer> generate_needs(int num_coro, int every_coro)
{
    std::vector<Buffer> res(num_coro);
    // 使用默认随机数引擎
    std::random_device rd;
    std::default_random_engine eng(rd());
    // 生成均匀分布的整数随机数
    std::uniform_int_distribution<int> dist(1, 100);

    for (int coro_id = 0; coro_id < num_coro; coro_id++)
    {
        res[coro_id].resize(every_coro);
        for (int i = 0; i < every_coro; i++)
        {
            res[coro_id][i] = dist(eng);
        }
    }
    return res;
}

int main()
{
    dbg("start");
    int num_coro = 4;
    std::vector<Buffer> need = generate_needs(num_coro, 10);
    CoroHandle handle(num_coro);
    PassPrinBuffer pass_buffer(num_coro);
    std::vector<FibonacciGenerator> fibo_coros;
    fibo_coros.reserve(num_coro);
    for (int i = 0; i < num_coro; i++)
    {
        pass_buffer[i].store(0);
        fibo_coros.emplace_back(i, need[i]);
        dbg("");
    }
    {
        dbg("");
    }
    std::atomic_bool exit_flag;
    std::thread p_thread(print_thread_loop, num_coro, std::ref(exit_flag), std::ref(pass_buffer));

    for (int i = 0; i < num_coro; i++)
    {
        fibo_coros[i].cal_and_print(num_coro, handle, pass_buffer);
    }

    exit_flag.store(true);
    p_thread.join();

    return 0;
}
