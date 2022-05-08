#pragma

namespace Pledge {

template <typename Functor>
struct Type : Type<decltype(&Functor::operator())>
{};

template <typename R, typename C, typename A>
struct Type<R (C::*)(A) const>
{
  using Ret = R;
  using Class = C;
  using Arg = A;
};

template <typename R, typename C, typename A>
struct Type<R (C::*)(A)>
{
  using Ret = R;
  using Class = C;
  using Arg = A;
};

template <typename R, typename C>
struct Type<R (C::*)() const>
{
  using Ret = R;
  using Class = C;
  using Arg = void;
};

template <typename R, typename C>
struct Type<R (C::*)()>
{
  using Ret = R;
  using Class = C;
  using Arg = void;
};

// is_specialization<std::vector<int>, std::vector>::value == true
// https://stackoverflow.com/a/28796458
template <typename Test, template <typename...> class Ref>
struct is_specialization : std::false_type
{};

template <template <typename...> class Ref, typename... Args>
struct is_specialization<Ref<Args...>, Ref> : std::true_type
{};

template <typename Test, template <typename...> class Ref>
inline constexpr bool is_specialization_v = is_specialization<Test, Ref>::value;

// Normally we catch by reference
template <typename T, typename S = void>
struct CatchType
{
  using Type = T&;
};

// Except pointers, to work around https://github.com/llvm/llvm-project/issues/55340
template <typename T>
struct CatchType<T, typename std::enable_if<std::is_pointer_v<T>>::type>
{
  using Type = T;
};


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

}
