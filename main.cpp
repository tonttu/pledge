#include <atomic>
#include <chrono>
#include <sstream>
#include <thread>

#include "manual_executor.hpp"
#include "promise.hpp"
#include "thread_pool_executor.hpp"

Pledge::ThreadPoolExecutor pool{ 8 };

std::string s_prev;

void check(bool v, const char* test, const char* file, int line)
{
  s_prev = test;
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
          "%s:%d: check (%s) with value %s not executed\n",
          file,
          line,
          expectedStr,
          e.str().c_str());
}

#define CHECK(check) check((check), #check, __FILE__, __LINE__)
#define CHECK_EQUAL(expected, actual)                                                              \
  checkEqual((expected), (actual), #expected, #actual, __FILE__, __LINE__)
#define CHECK_PREV(expected) checkPrev((expected), #expected, __FILE__, __LINE__)

int main()
{
  using namespace Pledge;
  {
    Promise<int> promise{ 42 };
    CHECK_EQUAL(42, promise.future().wait());
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
    promise.future(&pool).then([](int v) { CHECK_EQUAL(46, v); }).wait();
    CHECK_PREV(46);
  }
  {
    Promise<int> promise;
    auto future = promise.future(&pool).then([](int v) { CHECK_EQUAL(47, v); });
    promise.setValue(47);
    future.wait();
    CHECK_PREV(47);
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

  return 0;
}
