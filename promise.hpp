#pragma once

#include "future.hpp"

namespace Pledge {

template <typename T = void>
class Promise
{
public:
  Promise()
    : m_data(std::make_shared<FutureData<T>>())
  {}

  template <typename Y>
  Promise(Y&& t)
    : m_data(std::make_shared<FutureData<T>>(std::forward<Y>(t)))
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

  // private:
  std::shared_ptr<FutureData<T>> m_data;
};

}
