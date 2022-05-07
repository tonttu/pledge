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

}
