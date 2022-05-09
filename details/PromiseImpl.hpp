namespace Pledge {

template <typename T>
Promise<T>::Promise()
  : m_data(std::make_shared<FutureDataType<T>>())
{}

template <typename T>
template <typename Y>
Promise<T>::Promise(Y&& t)
  : m_data(std::make_shared<FutureDataType<T>>(std::forward<Y>(t)))
{}

template <typename T>
Future<T> Promise<T>::future(Executor* executor)
{
  m_data->executor = executor;
  return Future<T>(m_data);
}

template <typename T>
template <typename Y>
void Promise<T>::setValue(Y&& y)
{
  Impl::setValue(*m_data, std::forward<Y>(y));
}

template <typename T>
void Promise<T>::setError(std::exception_ptr error)
{
  Impl::setError(*m_data, std::move(error));
}

template <typename T>
template <typename E>
void Promise<T>::setError(E&& e)
{
  Impl::setError(*m_data, std::make_exception_ptr(std::forward<E>(e)));
}

template <typename T>
template <typename F>
void Promise<T>::set(F&& f)
{
  try {
    setValue(f());
  } catch (...) {
    setError(std::current_exception());
  }
}

Promise<void>::Promise()
  : m_data(std::make_shared<FutureData<void_type>>())
{}

Promise<void>::Promise(void_type t)
  : m_data(std::make_shared<FutureData<void_type>>(t))
{}

Future<> Promise<void>::future(Executor* executor)
{
  m_data->executor = executor;
  return Future<>(m_data);
}

void Promise<void>::setValue()
{
  Impl::setValue(*m_data, void_type{});
}

void Promise<void>::setError(std::exception_ptr error)
{
  Impl::setError(*m_data, std::move(error));
}

template <typename E>
void Promise<void>::setError(E&& e)
{
  Impl::setError(*m_data, std::make_exception_ptr(std::forward<E>(e)));
}

template <typename F>
auto via(Executor* executor, F&& f) -> Future<typename Type<F>::Type>
{
  return Promise<>().future(executor).then(std::forward<F>(f));
}

}
