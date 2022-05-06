#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>

#include "executor.hpp"

namespace Pledge {

template <typename T = void>
class FutureData
{
public:
  FutureData() = default;

  template <typename Y>
  FutureData(Y&& t)
    : m_value(std::forward<Y>(t))
  {}

  template <typename Y>
  void setValue(Y&& y)
  {
    {
      std::unique_lock<std::mutex> lock(m_mutex);
      m_value = std::forward<Y>(y);
    }
    // m_callback can't be assigned after m_value is set, no need to hold mutex
    if (m_callback)
      m_callback();
    m_cond.notify_all();
  }

  std::mutex m_mutex;
  std::condition_variable m_cond;
  std::optional<T> m_value;
  Executor* m_executor = nullptr;
  std::function<void()> m_callback;
};

template <>
class FutureData<void>
{
public:
  // TODO: Change boolean to something else
  FutureData(bool isValueSet = false)
    : m_valueIsSet(isValueSet)
  {}

  void setValue()
  {
    {
      std::unique_lock<std::mutex> lock(m_mutex);
      m_valueIsSet = true;
    }
    if (m_callback)
      m_callback();
    m_cond.notify_all();
  }

  // TODO: base class for these?
  std::mutex m_mutex;
  std::condition_variable m_cond;
  bool m_valueIsSet = false;
  Executor* m_executor = nullptr;
  std::function<void()> m_callback;
};

template <typename T = void>
class Future;

template <typename T>
class FutureBase
{
public:
  FutureBase(std::shared_ptr<FutureData<T>> data)
    : m_data(std::move(data))
  {}

  // TODO: protected
public:
  std::shared_ptr<FutureData<T>> m_data;
};

template <typename T>
class Future : public FutureBase<T>
{
public:
  using Base = FutureBase<T>;

  Future(std::shared_ptr<FutureData<T>> data)
    : Base(std::move(data))
  {}

  template <typename Y>
  Future(Y&& t)
    : Base(std::make_shared<FutureData<T>>(std::forward<Y>(t)))
  {}

  // TODO: Is this a good name? Maybe get() instead?
  T wait()
  {
    std::unique_lock<std::mutex> lock(Base::m_data->m_mutex);
    // TODO: Maybe m_cond is not needed, could use m_data->m_callback instead
    while (!Base::m_data->m_value)
      Base::m_data->m_cond.wait(lock);
    return *std::move(Base::m_data->m_value);
  }

  Future<T>& via(Executor* executor)
  {
    // TODO: do we need to lock mutex here?
    Base::m_data->m_executor = executor;
    return *this;
  }

  // TODO: Add some helper for getting the return type
  template <typename F>
  auto then(F&& f) -> Future<decltype(f(std::declval<T>()))>
  {
    using Ret = decltype(f(std::declval<T>()));

    std::unique_lock<std::mutex> g(Base::m_data->m_mutex);
    if (Base::m_data->m_value) {
      // m_value can't be reassigned or cleared, so there's no need to keep
      // the mutex locked anymore, which also could lead to deadlock
      // depending on what f does.
      g.unlock();
      if (Base::m_data->m_executor) {
        auto next = std::make_shared<FutureData<Ret>>();
        Base::m_data->m_executor->add([next, prev = Base::m_data, f] {
          // TODO: Combine these four almost identical blocks.
          // TODO: Support future return values
          if constexpr (std::is_same_v<Ret, void>) {
            f(*std::move(prev->m_value));
            next->setValue();
          } else {
            next->setValue(f(*std::move(prev->m_value)));
          }
        });
        return Future<Ret>(std::move(next));
      } else {
        if constexpr (std::is_same_v<Ret, void>) {
          f(*std::move(Base::m_data->m_value));
          return Future<void>{ true };
        } else {
          return f(std::move(*Base::m_data->m_value));
        }
      }
    } else {
      std::weak_ptr<FutureData<T>> prevWeak(Base::m_data);
      auto next = std::make_shared<FutureData<Ret>>();
      Base::m_data->m_callback = [prevWeak, next, f] {
        std::shared_ptr<FutureData<T>> prev(prevWeak);
        if (prev->m_executor) {
          prev->m_executor->add([next, prev, f] {
            if constexpr (std::is_same_v<Ret, void>) {
              f(*std::move(prev->m_value));
              next->setValue();
            } else {
              next->setValue(f(*std::move(prev->m_value)));
            }
          });
        } else {
          if constexpr (std::is_same_v<Ret, void>) {
            f(*std::move(prev->m_value));
            next->setValue();
          } else {
            next->setValue(f(*std::move(prev->m_value)));
          }
        }
      };
      return Future<Ret>(std::move(next));
    }
  }
};

template <>
class Future<void> : public FutureBase<void>
{
public:
  using Base = FutureBase<void>;

  Future(std::shared_ptr<FutureData<>> data)
    : Base(std::move(data))
  {}
  Future(bool isValueSet = false)
    : Base(std::make_shared<FutureData<>>(isValueSet))
  {}

  void wait()
  {
    std::unique_lock<std::mutex> lock(m_data->m_mutex);
    while (!m_data->m_valueIsSet)
      m_data->m_cond.wait(lock);
  }

  Future<>& via(Executor* executor)
  {
    // TODO: do we need to lock mutex here?
    m_data->m_executor = executor;
    return *this;
  }

  template <typename F>
  auto then(F&& f) -> Future<decltype(f())>
  {
    using Ret = decltype(f());

    // TODO: implement
  }
};

template <typename F>
auto via(Executor* executor, F&& f) -> Future<decltype(f())>
{
  using Ret = decltype(f());
  return Future<>().via(executor).then(std::forward<F>(f));
}

}
