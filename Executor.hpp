#pragma once

#include <functional>

namespace Pledge {

// Executor defines an execution context for tasks. In practise it manages
// when and in which thread then/error callbacks are called.
class Executor
{
public:
  using Func = std::function<void()>;

  virtual ~Executor() {}

  virtual void add(Func func) = 0;
};

}
