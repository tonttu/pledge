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

  template <typename Y>
  Future(Y&& t);

  Future<T>& via(Executor* executor);

  T get();

  template <typename F>
  auto error(F&& f) -> Future<T>;

  template <typename F>
  auto then(F&& f) -> FutureType<typename Type<F>::Ret>;

  // TODO: private
public:
  std::shared_ptr<FutureDataType<T>> m_data;
};

template <>
class Future<void> : Future<void_type>
{
public:
  using Base = Future<void_type>;

  using Base::error;
  using Base::then;

  Future(std::shared_ptr<FutureDataType<void>> data);

  void get() { Base::get(); }

  Future<>& via(Executor* executor);
};

}

#include "details/FutureImpl.hpp"
