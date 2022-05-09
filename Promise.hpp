#pragma once

#include "Future.hpp"

namespace Pledge {

template <typename T = void>
class Promise
{
public:
  Promise()
    : m_data(std::make_shared<FutureDataType<T>>())
  {}

  template <typename Y>
  Promise(Y&& t)
    : m_data(std::make_shared<FutureDataType<T>>(std::forward<Y>(t)))
  {}

  Future<T> future(Executor* executor = nullptr)
  {
    m_data->m_executor = executor;
    return Future<T>(m_data);
  }

  template <typename Y>
  void setValue(Y&& y)
  {
    m_data->setValue(std::forward<Y>(y));
  }

  void setError(std::exception_ptr error) { m_data->setError(std::move(error)); }

  template <typename E>
  void setError(E&& e)
  {
    m_data->setError(std::make_exception_ptr(std::forward<E>(e)));
  }

  template <typename F>
  void set(F&& f)
  {
    try {
      setValue(f());
    } catch (...) {
      setError(std::current_exception());
    }
  }

  // private:
  std::shared_ptr<FutureDataType<T>> m_data;
};

// TODO: PromiseBase?
template <>
class Promise<void>
{
public:
  Promise()
    : m_data(std::make_shared<FutureData<void_type>>())
  {}

  Promise(void_type t)
    : m_data(std::make_shared<FutureData<void_type>>(t))
  {}

  Future<> future(Executor* executor = nullptr)
  {
    m_data->m_executor = executor;
    return Future<>(m_data);
  }

  void setValue() { m_data->setValue(void_type{}); }

  void setError(std::exception_ptr error) { m_data->setError(std::move(error)); }

  template <typename E>
  void setError(E&& e)
  {
    m_data->setError(std::make_exception_ptr(std::forward<E>(e)));
  }

  // private:
  std::shared_ptr<FutureData<void_type>> m_data;
};

template <typename F>
auto via(Executor* executor, F&& f) -> Future<typename Type<F>::Type>
{
  return Promise<>().future(executor).then(std::forward<F>(f));
}

}
