namespace Pledge {

namespace Impl {

template <typename T, typename Y>
void setValue(FutureData<T>& data, Y&& y)
{
  {
    std::unique_lock<std::mutex> lock(data.m_mutex);
    data.m_value.template emplace<FutureData<T>::Value>(std::forward<Y>(y));
  }
  // m_callback can't be assigned after m_value is set, no need to hold mutex
  if (data.m_callback)
    data.m_callback();
  data.m_cond.notify_all();
}

template <typename T>
void setError(FutureData<T>& data, std::exception_ptr error)
{
  {
    std::unique_lock<std::mutex> lock(data.m_mutex);
    data.m_value.template emplace<FutureData<T>::Error>(std::move(error));
  }
  if (data.m_callback)
    data.m_callback();
  data.m_cond.notify_all();
}

template <typename From, typename To, typename Func>
inline void handleThenDirect(std::shared_ptr<FutureData<From>>& from,
                             std::shared_ptr<FutureData<To>>& to,
                             Func&& f)
{
  if (from->m_value.index() == FutureData<From>::Value) {
    try {
      using FuncRet = typename Type<Func>::Ret;
      if constexpr (is_specialization_v<FuncRet, Future>) {
        if constexpr (std::is_same_v<From, void_type>) {
          f()
            .then([to](To v) { etValue(*to, std::move(v)); })
            .error([to](std::exception_ptr error) { setError(*to, std::move(error)); });
        } else {
          f(std::move(std::get<FutureData<From>::Value>(from->m_value)))
            .then([to](To v) { setValue(*to, std::move(v)); })
            .error([to](std::exception_ptr error) { setError(*to, std::move(error)); });
        }
      } else if constexpr (std::is_same_v<From, void_type>) {
        if constexpr (std::is_same_v<To, void_type>) {
          f();
          setValue(*to, void_type{});
        } else {
          setValue(*to, f());
        }
      } else {
        if constexpr (std::is_same_v<To, void_type>) {
          f(std::move(std::get<FutureData<From>::Value>(from->m_value)));
          setValue(*to, void_type{});
        } else {
          setValue(*to, f(std::move(std::get<FutureData<From>::Value>(from->m_value))));
        }
      }
    } catch (...) {
      setError(*to, std::current_exception());
    }
  } else {
    assert(from->m_value.index() == 2);
    setError(*to, std::move(std::get<2>(from->m_value)));
  }
}

template <typename E, typename T, typename Func>
inline void handleErrorDirect(std::shared_ptr<FutureData<T>>& from,
                              std::shared_ptr<FutureData<T>>& to,
                              Func&& f)
{
  if (from->m_value.index() == FutureData<T>::Value) {
    setValue(*to, std::move(std::get<FutureData<T>::Value>(from->m_value)));
  } else {
    assert(from->m_value.index() == 2);
    using FuncRet = typename Type<Func>::Ret;

    if constexpr (std::is_same_v<E, std::exception_ptr>) {
      try {
        if constexpr (is_specialization_v<FuncRet, Future>) {
          f(std::move(std::get<2>(from->m_value)))
            .then([to](T v) { setValue(*to, std::move(v)); })
            .error([to](std::exception_ptr error) { setError(*to, std::move(error)); });
        } else if constexpr (std::is_same_v<T, void_type>) {
          f(std::move(std::get<2>(from->m_value)));
          setValue(*to, void_type{});
        } else {
          setValue(*to, f(std::move(std::get<2>(from->m_value))));
        }
      } catch (...) {
        setError(*to, std::current_exception());
      }
    } else {
      using Catch = typename CatchType<E>::Type;
      try {
        std::rethrow_exception(std::get<2>(from->m_value));
      } catch (Catch e) {
        try {
          if constexpr (is_specialization_v<FuncRet, Future>) {
            f(e)
              .then([to](T v) { setValue(*to, std::move(v)); })
              .error([to](std::exception_ptr error) { setError(*to, std::move(error)); });
          } else if constexpr (std::is_same_v<T, void_type>) {
            f(e);
            setValue(*to, void_type{});
          } else {
            setValue(*to, f(e));
          }
        } catch (...) {
          setError(*to, std::current_exception());
        }
      } catch (...) {
        setError(*to, std::get<2>(from->m_value));
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
      handleThenDirect(from, to, std::forward<Func>(f));
    });
  } else {
    handleThenDirect(from, to, std::forward<Func>(f));
  }
}

template <typename E, typename D, typename Func>
inline void handleError(D& from, D& to, Func&& f)
{
  if (from->m_executor) {
    from->m_executor->add([from, to, f]() mutable {
      // TODO: Shouldn't this use std::move instead?
      handleErrorDirect<E>(from, to, std::forward<Func>(f));
    });
  } else {
    handleErrorDirect<E>(from, to, std::forward<Func>(f));
  }
}

} // namespace Impl

template <typename T>
template <typename Y>
FutureData<T>::FutureData(Y&& t)
  : m_value(std::forward<Y>(t))
{}

template <typename T>
Future<T>::Future(std::shared_ptr<FutureDataType<T>> data)
  : m_data(std::move(data))
{}

template <typename T>
template <typename Y>
Future<T>::Future(Y&& t)
  : m_data(std::make_shared<FutureDataType<T>>(std::forward<Y>(t)))
{}

template <typename T>
Future<T>& Future<T>::via(Executor* executor)
{
  // TODO: do we need to lock mutex here?
  m_data->m_executor = executor;
  return *this;
}

template <typename T>
T Future<T>::wait()
{
  std::unique_lock<std::mutex> lock(m_data->m_mutex);
  // TODO: Maybe m_cond is not needed, could use m_data->m_callback instead

  while (m_data->m_value.index() == 0)
    m_data->m_cond.wait(lock);

  if (m_data->m_value.index() == FutureData<T>::Value)
    return std::get<FutureData<T>::Value>(std::move(m_data->m_value));

  std::rethrow_exception(std::get<2>(std::move(m_data->m_value)));
}

template <typename T>
template <typename F>
auto Future<T>::error(F&& f) -> Future<T>
{
  using E = typename Type<F>::Arg;

  std::unique_lock<std::mutex> g(m_data->m_mutex);
  size_t idx = m_data->m_value.index();
  if (idx == 0) {
    std::weak_ptr<FutureDataType<T>> selfWeak(m_data);
    auto next = std::make_shared<FutureDataType<T>>();
    next->m_executor = m_data->m_executor;
    m_data->m_callback = [selfWeak, next, f]() mutable {
      std::shared_ptr<FutureDataType<T>> self(selfWeak);
      Impl::handleError<E>(self, next, std::forward<F>(f));
    };
    return next;
  } else {
    g.unlock();
    auto next = std::make_shared<FutureDataType<T>>();
    next->m_executor = m_data->m_executor;
    Impl::handleError<E>(m_data, next, std::forward<F>(f));
    return next;
  }
}

template <typename T>
template <typename F>
auto Future<T>::then(F&& f) -> FutureType<typename Type<F>::Ret>
{
  using Ret = typename Type<F>::Ret;

  std::unique_lock<std::mutex> g(m_data->m_mutex);
  size_t idx = m_data->m_value.index();
  if (idx == 0) {
    std::weak_ptr<FutureDataType<T>> selfWeak(m_data);
    auto next = std::make_shared<FutureDataType<Ret>>();
    next->m_executor = m_data->m_executor;
    m_data->m_callback = [selfWeak, next, f]() mutable {
      std::shared_ptr<FutureDataType<T>> self(selfWeak);
      Impl::handleThen(self, next, std::forward<F>(f));
    };
    return next;
  } else {
    // m_value can't be reassigned or cleared, so there's no need to keep
    // the mutex locked anymore, which also could lead to deadlock
    // depending on what f does.
    g.unlock();
    auto next = std::make_shared<FutureDataType<Ret>>();
    next->m_executor = m_data->m_executor;
    Impl::handleThen(m_data, next, std::forward<F>(f));
    return next;
  }
}

Future<void>::Future(std::shared_ptr<FutureDataType<void>> data)
  : Base(std::move(data))
{}

Future<>& Future<void>::via(Executor* executor)
{
  // TODO: do we need to lock mutex here?
  m_data->m_executor = executor;
  return *this;
}

}
