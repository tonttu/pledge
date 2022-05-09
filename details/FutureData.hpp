#pragma once

#include <cassert>
#include <mutex>
#include <variant>

#include "Traits.hpp"

namespace Pledge {

// Future<void> internally uses FutureData<void_type> to make it slightly
// easier to implement void futures.
struct void_type
{};

// Forward declarations

template <typename T = void>
class Future;

template <>
class Future<void>;

template <typename T = void_type>
class FutureData;

// FutureTypeT is to help to choose the correct template parameter for FutureData
// based on the Future template parameter and the returned Future type for then().
//
// Future<int> uses FutureData<int> but
// Future<void> uses FutureData<void_type>.
//
// Also .then([] () -> int {}) returns Future<int> but
//      .then([] () -> Future<int> {}) also returns Future<int>.

template <typename T, typename S = void>
struct FutureTypeT
{
  using FutureValueType = T;
  using DataValueType = T;
};

template <typename T>
struct FutureTypeT<T, typename std::enable_if<is_specialization_v<T, Future>>::type>
{
  using FutureValueType = typename T::ValueType;
  using DataValueType = typename T::ValueType;
};

template <>
struct FutureTypeT<Future<>>
{
  using FutureValueType = void;
  using DataValueType = void_type;
};

template <>
struct FutureTypeT<void>
{
  using FutureValueType = void;
  using DataValueType = void_type;
};

template <typename T>
using FutureDataType = FutureData<typename FutureTypeT<T>::DataValueType>;

template <typename T>
using FutureType = Future<typename FutureTypeT<T>::FutureValueType>;

// This is the shared state between a promise and a future. Each link in a
// continuation chain has its own Future and own FutureData.
template <typename T>
class FutureData
{
public:
  FutureData() = default;

  template <typename Y>
  FutureData(Y&& t);

  // Indexes to m_value
  enum State : size_t
  {
    Waiting = 0,
    Value = 1,
    Error = 2
  };

  std::mutex mutex;
  std::variant<std::monostate, T, std::exception_ptr> value;
  Executor* executor = nullptr;
  std::function<void()> callback;
};

}
