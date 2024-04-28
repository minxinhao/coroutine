#include <coroutine>
#include <iostream>
#include <thread>

std::coroutine_handle<> handle;

struct Awaiter {
  bool await_ready() {
    std::cout << "await ready or not" << std::endl;
    return false;
  }

  void await_resume() {
    std::cout << "await resumed" << std::endl;
  }

  void await_suspend(std::coroutine_handle<> h) {
    std::cout << "await suspended" << std::endl;
    handle = h;
  }
};

struct Promise {
  struct promise_type {
    auto get_return_object() noexcept {
      std::cout << "get return object" << std::endl;
      return Promise();
    }

    auto initial_suspend() noexcept {
      std::cout << "initial suspend, return never" << std::endl;
      return std::suspend_never{};
    }

    auto final_suspend() noexcept {
      std::cout << "final suspend, return never" << std::endl;
      return std::suspend_never{};
    }

    void unhandled_exception() {
      std::cout << "unhandle exception" << std::endl;
      std::terminate();
    }

    void return_void() {
      std::cout << "return void" << std::endl;
      return;
    }
  };
};

Promise CoroutineFunc() {
  std::cout << "before co_await" << std::endl;
  co_await Awaiter();
  std::cout << "after co_await" << std::endl;
}

int main() {
  std::cout << "main() start" << std::endl;
  CoroutineFunc();

  std::this_thread::sleep_for(std::chrono::seconds(1));
  std::cout << "resume coroutine after one second" << std::endl;
  handle.resume();

  std::cout << "main() exit" << std::endl;
}