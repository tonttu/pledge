#pragma once

#include <cassert>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <variant>

#include "executor.hpp"
#include "traits.hpp"

namespace Pledge {

struct void_type
{};

template <typename T>
class FutureBase;

template <typename T = void>
class Future;

template <>
class Future<void>;

template <typename T = void_type>
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
      m_value.template emplace<1>(std::forward<Y>(y));
    }
    // m_callback can't be assigned after m_value is set, no need to hold mutex
    if (m_callback)
      m_callback();
    m_cond.notify_all();
  }

  void setError(std::exception_ptr error)
  {
    {
      std::unique_lock<std::mutex> lock(m_mutex);
      m_value.template emplace<2>(std::move(error));
    }
    if (m_callback)
      m_callback();
    m_cond.notify_all();
  }

  std::mutex m_mutex;
  std::condition_variable m_cond;
  std::variant<std::monostate, T, std::exception_ptr> m_value;
  Executor* m_executor = nullptr;
  std::function<void()> m_callback;
};

template <typename From, typename To, typename Func>
inline void handleThenImpl(std::shared_ptr<FutureData<From>>& from,
                           std::shared_ptr<FutureData<To>>& to,
                           Func&& f)
{
  if (from->m_value.index() == 1) {
    try {
      using FuncRet = typename Type<Func>::Ret;
      if constexpr (is_specialization_v<FuncRet, Future>) {
        if constexpr (std::is_same_v<From, void_type>) {
          f()
            .then([to](To v) { to->setValue(std::move(v)); })
            .error([to](std::exception_ptr error) { to->setError(std::move(error)); });
        } else {
          f(std::move(std::get<1>(from->m_value)))
            .then([to](To v) { to->setValue(std::move(v)); })
            .error([to](std::exception_ptr error) { to->setError(std::move(error)); });
        }
      } else if constexpr (std::is_same_v<From, void_type>) {
        if constexpr (std::is_same_v<To, void_type>) {
          f();
          to->setValue(void_type{});
        } else {
          to->setValue(f());
        }
      } else {
        if constexpr (std::is_same_v<To, void_type>) {
          f(std::move(std::get<1>(from->m_value)));
          to->setValue(void_type{});
        } else {
          to->setValue(f(std::move(std::get<1>(from->m_value))));
        }
      }
    } catch (...) {
      to->setError(std::current_exception());
    }
  } else {
    assert(from->m_value.index() == 2);
    to->setError(std::move(std::get<2>(from->m_value)));
  }
}

template <typename E, typename T, typename Func>
inline void handleErrorImpl(std::shared_ptr<FutureData<T>>& from,
                            std::shared_ptr<FutureData<T>>& to,
                            Func&& f)
{
  if (from->m_value.index() == 1) {
    to->setValue(std::move(std::get<1>(from->m_value)));
  } else {
    assert(from->m_value.index() == 2);
    using FuncRet = typename Type<Func>::Ret;

    if constexpr (std::is_same_v<E, std::exception_ptr>) {
      try {
        if constexpr (is_specialization_v<FuncRet, Future>) {
          f(std::move(std::get<2>(from->m_value)))
            .then([to](T v) { to->setValue(std::move(v)); })
            .error([to](std::exception_ptr error) { to->setError(std::move(error)); });
        } else if constexpr (std::is_same_v<T, void_type>) {
          f(std::move(std::get<2>(from->m_value)));
          to->setValue(void_type{});
        } else {
          to->setValue(f(std::move(std::get<2>(from->m_value))));
        }
      } catch (...) {
        to->setError(std::current_exception());
      }
    } else {
      using Catch = typename CatchType<E>::Type;
      try {
        std::rethrow_exception(std::get<2>(from->m_value));
      } catch (Catch e) {
        try {
          if constexpr (is_specialization_v<FuncRet, Future>) {
            f(e)
              .then([to](T v) { to->setValue(std::move(v)); })
              .error([to](std::exception_ptr error) { to->setError(std::move(error)); });
          } else if constexpr (std::is_same_v<T, void_type>) {
            f(e);
            to->setValue(void_type{});
          } else {
            to->setValue(f(e));
          }
        } catch (...) {
          to->setError(std::current_exception());
        }
      } catch (...) {
        to->setError(std::get<2>(from->m_value));
      }
    }
  }
}

// Called when from has ready value or an error, and now we are expected to
// call the continuation function f in the 'from' executor. The result of f
// is then assigned to 'to'. If 'f' returns a future instead, a new then/error
// continuations are added to the future which then assign the value to 'to'.
template <typename From, typename To, typename Func>
inline void handleThen(std::shared_ptr<FutureData<From>>& from,
                       std::shared_ptr<FutureData<To>>& to,
                       Func&& f)
{
  if (from->m_executor) {
    from->m_executor->add([from, to, f]() mutable {
      // TODO: Shouldn't this use std::move instead?
      handleThenImpl(from, to, std::forward<Func>(f));
    });
  } else {
    handleThenImpl(from, to, std::forward<Func>(f));
  }
}

template <typename E, typename D, typename Func>
inline void handleError(D& from, D& to, Func&& f)
{
  if (from->m_executor) {
    from->m_executor->add([from, to, f]() mutable {
      // TODO: Shouldn't this use std::move instead?
      handleErrorImpl<E>(from, to, std::forward<Func>(f));
    });
  } else {
    handleErrorImpl<E>(from, to, std::forward<Func>(f));
  }
}

template <typename T>
class FutureBase
{
public:
  using ValueType = T;

  FutureBase(std::shared_ptr<FutureDataType<T>> data)
    : m_data(std::move(data))
  {}

  // TODO: Is this a good name? Maybe get() instead?
  T wait()
  {
    std::unique_lock<std::mutex> lock(m_data->m_mutex);
    // TODO: Maybe m_cond is not needed, could use m_data->m_callback instead

    while (m_data->m_value.index() == 0)
      m_data->m_cond.wait(lock);

    if (m_data->m_value.index() == 1)
      return std::get<1>(std::move(m_data->m_value));

    std::rethrow_exception(std::get<2>(std::move(m_data->m_value)));
  }

  // TODO: protected
public:
  std::shared_ptr<FutureDataType<T>> m_data;
};

template <typename T>
class Future : public FutureBase<T>
{
public:
  using Base = FutureBase<T>;

  Future(std::shared_ptr<FutureDataType<T>> data)
    : Base(std::move(data))
  {}

  template <typename Y>
  Future(Y&& t)
    : Base(std::make_shared<FutureDataType<T>>(std::forward<Y>(t)))
  {}

  Future<T>& via(Executor* executor)
  {
    // TODO: do we need to lock mutex here?
    Base::m_data->m_executor = executor;
    return *this;
  }

  template <typename F>
  auto error(F&& f) -> Future<T>
  {
    using E = typename Type<F>::Arg;

    std::unique_lock<std::mutex> g(Base::m_data->m_mutex);
    size_t idx = Base::m_data->m_value.index();
    if (idx == 0) {
      std::weak_ptr<FutureDataType<T>> selfWeak(Base::m_data);
      auto next = std::make_shared<FutureDataType<T>>();
      next->m_executor = Base::m_data->m_executor;
      Base::m_data->m_callback = [selfWeak, next, f]() mutable {
        std::shared_ptr<FutureDataType<T>> self(selfWeak);
        handleError<E>(self, next, std::forward<F>(f));
      };
      return next;
    } else {
      g.unlock();
      auto next = std::make_shared<FutureDataType<T>>();
      next->m_executor = Base::m_data->m_executor;
      handleError<E>(Base::m_data, next, std::forward<F>(f));
      return next;
    }
  }

  template <typename F>
  auto then(F&& f) -> FutureType<typename Type<F>::Ret>
  {
    using Ret = typename Type<F>::Ret;

    std::unique_lock<std::mutex> g(Base::m_data->m_mutex);
    size_t idx = Base::m_data->m_value.index();
    if (idx == 0) {
      std::weak_ptr<FutureDataType<T>> selfWeak(Base::m_data);
      auto next = std::make_shared<FutureDataType<Ret>>();
      next->m_executor = Base::m_data->m_executor;
      Base::m_data->m_callback = [selfWeak, next, f]() mutable {
        std::shared_ptr<FutureDataType<T>> self(selfWeak);
        handleThen(self, next, std::forward<F>(f));
      };
      return next;
    } else {
      // m_value can't be reassigned or cleared, so there's no need to keep
      // the mutex locked anymore, which also could lead to deadlock
      // depending on what f does.
      g.unlock();
      auto next = std::make_shared<FutureDataType<Ret>>();
      next->m_executor = Base::m_data->m_executor;
      handleThen(Base::m_data, next, std::forward<F>(f));
      return next;
    }
  }
};

template <>
class Future<void> : Future<void_type>
{
public:
  using Base = Future<void_type>;

  using Base::error;
  using Base::then;

  Future(std::shared_ptr<FutureDataType<void>> data)
    : Base(std::move(data))
  {}

  void wait() { Base::wait(); }

  Future<>& via(Executor* executor)
  {
    // TODO: do we need to lock mutex here?
    m_data->m_executor = executor;
    return *this;
  }
};

}
