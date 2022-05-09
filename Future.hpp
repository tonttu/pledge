#pragma once

#include <memory>

#include "Executor.hpp"
#include "details/FutureData.hpp"

namespace Pledge {

template <typename T>
class Future
{
public:
  using ValueType = T;

  Future(std::shared_ptr<FutureDataType<T>> data);

  // Constructs a ready future.
  template <typename Y>
  Future(Y&& t);

  // Changes the future executor and returns *this for chaining.
  Future<T>&& via(Executor* executor) &&;

  // Block the current thread until the future is ready, and either moves out
  // the value or throws the future error. You can move the value out just
  // once, and the future must be rvalue when doing so. You can either add
  // continuations or call this function.
  T get() &&;

  // Returns true if calling get() would return the value immediately.
  bool hasValue() const;
  // Returns true if calling get() would immediately throw the error.
  bool hasError() const;
  // Atomic way to call hasValue() || hasError()
  bool isReady() const;

  // Add a continuation which is called from the current executor once
  // the future has an error.
  template <typename F>
  auto error(F&& f) && -> Future<T>;

  // Add a continuation which is called from the current executor once
  // the future has a value. Returns a future with the same executor.
  template <typename F>
  auto then(F&& f) && -> FutureType<typename Type<F>::Ret>;

protected:
  std::shared_ptr<FutureDataType<T>> m_data;
};

// A special case for a void future. When continuing a void future, then()
// continuations do not take in any arguments. If a continuation callback
// doesn't return a value, the next link becomes a void future.
template <>
class Future<void> : Future<void_type>
{
public:
  using Base = Future<void_type>;

  using Base::error;
  using Base::hasError;
  using Base::hasValue;
  using Base::isReady;
  using Base::then;

  Future(std::shared_ptr<FutureDataType<void>> data);

  void get() && { std::move(*this).Base::get(); }

  Future<>&& via(Executor* executor) &&;
};

}

#include "details/FutureImpl.hpp"
