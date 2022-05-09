#pragma once

#include "Future.hpp"

namespace Pledge {

template <typename T = void>
class Promise
{
public:
  Promise();

  template <typename Y>
  Promise(Y&& t);

  Future<T> future(Executor* executor = nullptr);

  template <typename Y>
  void setValue(Y&& y);

  void setError(std::exception_ptr error);

  template <typename E>
  void setError(E&& e);

  template <typename F>
  void set(F&& f);

  // private:
  std::shared_ptr<FutureDataType<T>> m_data;
};

template <>
class Promise<void>
{
public:
  Promise();

  Promise(void_type t);

  Future<> future(Executor* executor = nullptr);

  void setValue();

  void setError(std::exception_ptr error);

  template <typename E>
  void setError(E&& e);

  // private:
  std::shared_ptr<FutureData<void_type>> m_data;
};

// Create a new future from the result of 'f' executed in the given executor.
template <typename F>
auto via(Executor* executor, F&& f) -> Future<typename Type<F>::Type>;

}

#include "details/PromiseImpl.hpp"
