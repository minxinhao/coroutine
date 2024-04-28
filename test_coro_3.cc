#include <coroutine>
#include <iostream>
#include <thread>

std::thread thread;

struct Awaiter {
  Awaiter() {
    std::cout << "Awaiter()" << std::endl;
  }

  ~Awaiter() {
    std::cout << "~Awaiter()" << std::endl;
  }

  bool await_ready() {
    std::cout << "await ready or not" << std::endl;
    return false;
  }

  void await_resume() {
    std::cout << "await resumed" << std::endl;
  }

  void await_suspend(std::coroutine_handle<> h) {
    std::cout << "await suspended" << std::endl;

    thread = std::thread([h] {
      std::cout << "resume coroutine in another thread" << std::endl;
      h.resume();
      std::this_thread::sleep_for(std::chrono::seconds(1));
    });
  thread.join();
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
  std::cout << "main() exit" << std::endl;
//   thread.join();
}

/*
main() start
get return object
initial suspend, return never
before co_await
Awaiter()
await ready or not
await suspended
main() exit
resume coroutine in another thread
await resumed
~Awaiter()
after co_await
return void
final suspend, return never
*/