#include <atomic>
#include <chrono>
#include <sstream>
#include <thread>

#include "ManualExecutor.hpp"
#include "Promise.hpp"
#include "ThreadPoolExecutor.hpp"

Pledge::ThreadPoolExecutor pool{ 8 };

std::string s_prev;

template <typename T>
void check(T v, const char* test, const char* file, int line)
{
  std::stringstream ss;
  ss << v;
  s_prev = ss.str();

  if (v)
    return;
  fprintf(stderr, "%s:%d: check failed: %s\n", file, line, test);
}

template <typename E, typename A>
void checkEqual(const E& expected,
                const A& actual,
                const char* expectedStr,
                const char* actualStr,
                const char* file,
                int line)
{
  std::stringstream e, a;
  e << expected;
  a << actual;

  s_prev = e.str();

  if (e.str() == a.str())
    return;

  fprintf(stderr,
          "%s:%d: check failed, expected (%s): %s, actual (%s): %s\n",
          file,
          line,
          expectedStr,
          e.str().c_str(),
          actualStr,
          a.str().c_str());
}

template <typename E>
void checkPrev(const E& expected, const char* expectedStr, const char* file, int line)
{
  std::stringstream e;
  e << expected;

  if (s_prev == e.str())
    return;

  fprintf(stderr,
          "%s:%d: check (%s) with value %s not executed (prev: %s)\n",
          file,
          line,
          expectedStr,
          e.str().c_str(),
          s_prev.c_str());
}

#define CHECK(test) check((test), #test, __FILE__, __LINE__)
#define CHECK_EQUAL(expected, actual)                                                              \
  checkEqual((expected), (actual), #expected, #actual, __FILE__, __LINE__)
#define CHECK_PREV(expected) checkPrev((expected), #expected, __FILE__, __LINE__)

int main()
{
  using namespace Pledge;
  {
    Promise<int> promise{ 42 };
    CHECK_EQUAL(42, promise.future().get());
  }
  {
    Promise<int> promise{ 43 };
    promise.future().then([](int v) { CHECK_EQUAL(43, v); });
    CHECK_PREV(43);
  }
  {
    Promise<int> promise;
    promise.future().then([](int v) { CHECK_EQUAL(44, v); });
    promise.setValue(44);
    CHECK_PREV(44);
  }
  {
    Promise<int> promise;
    promise.future().then([](int v) { return v + 1; }).then([](int v) { CHECK_EQUAL(45, v); });
    promise.setValue(44);
    CHECK_PREV(45);
  }

  {
    Promise<int> promise{ 46 };
    promise.future(&pool).then([](int v) { CHECK_EQUAL(46, v); }).get();
    CHECK_PREV(46);
  }
  {
    Promise<int> promise;
    auto future = promise.future(&pool).then([](int v) { CHECK_EQUAL(47, v); });
    promise.setValue(47);
    std::move(future).get();
    CHECK_PREV(47);
  }

  {
    Promise<> promise;
    auto future = promise.future(&pool).then([] { CHECK(true); });
    promise.setValue();
    std::move(future).get();
    CHECK_PREV(true);
  }

  {
    Promise<> promise;
    auto future =
      promise.future(&pool).then([] { return true; }).then([](bool) { return std::string("yay"); });
    promise.setValue();
    CHECK_EQUAL("yay", std::move(future).get());
  }

  ManualExecutor main;
  {
    Promise<int> promise;
    std::atomic<int> a{ 0 }, b{ 0 };
    promise.future(&pool)
      .then([&a](int v) {
        a = v;
        return v + 1;
      })
      .via(&main)
      .then([&b](int v) { b = v; });
    promise.setValue(48);
    while (a != 48)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    CHECK_EQUAL(0, b);
    CHECK_EQUAL(1, main.run());
    CHECK_EQUAL(49, b);
  }

  {
    Promise<int> promise;
    promise.future().error([](const std::exception& error) {
      CHECK_EQUAL("failure", error.what());
      return 0;
    });
    promise.setError(std::runtime_error("failure"));
    CHECK_PREV("failure");
  }

  {
    Promise<int> promise;
    auto f = promise.future()
               .then([](int) {
                 // Not called
                 CHECK(false);
                 return 123;
               })
               .error([](const std::runtime_error&) {
                 // Not called
                 CHECK(false);
                 return 12345;
               })
               .error([](const std::logic_error& error) {
                 CHECK_EQUAL("nope", error.what());
                 return 1234;
               })
               .then([](int v) { return v + 1; });
    CHECK(!f.isReady());
    promise.setError(std::invalid_argument("nope"));
    CHECK(f.isReady());
    CHECK_EQUAL(1235, std::move(f).get());
  }

  {
    Promise<int> promise;
    promise.set([]() -> int { throw "Nah"; });
    promise.future().error([](const char* msg) {
      CHECK_EQUAL("Nah", std::string(msg));
      return 42;
    });
    CHECK_PREV("Nah");
  }

  {
    Promise<int> promise;
    promise.future().then([](int v) { throw v + 1; }).error([](int v) {
      CHECK_EQUAL(100, v);
      return 0;
    });
    promise.setValue(99);
    CHECK_PREV(100);
  }

  static_assert(is_specialization_v<Future<int>, Future>);
  static_assert(std::is_same_v<FutureType<int>, Future<int>>);
  static_assert(std::is_same_v<FutureType<void>, Future<>>);
  static_assert(std::is_same_v<FutureType<Future<int>>, Future<int>>);
  static_assert(std::is_same_v<FutureType<Future<>>, Future<>>);

  {
    Promise<int> promise{ 100 };
    int v = promise.future(&pool)
              .then([](int v) {
                Promise<int> promise2;
                auto future2 = promise2.future(&pool).then([](int v) { return v + 1; });
                promise2.setValue(v + 1);
                return future2;
              })
              .get();
    CHECK_EQUAL(102, v);
  }

  {
    Promise<int> promise;
    auto future = promise.future().error([](const char* error) {
      Promise<int> promise2;
      auto future2 = promise2.future(&pool).then([](int v) { return v + 1; });
      promise2.setValue(std::stoi(error));
      return future2;
    });
    promise.set([]() -> int { throw "102"; });
    CHECK_EQUAL(103, std::move(future).get());
  }

  {
    Promise<std::unique_ptr<int>> promise;
    auto future = promise.future(&pool)
                    .then([](std::unique_ptr<int> p) {
                      ++*p;
                      return p;
                    })
                    .then([](std::unique_ptr<int> p) {
                      ++*p;
                      return p;
                    });
    promise.setValue(std::make_unique<int>(1));
    std::unique_ptr<int> i = std::move(future).get();
    CHECK_EQUAL(3, *i);
  }

  return 0;
}
