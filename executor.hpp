#pragma once

#include <functional>

namespace Pledge {

class Executor
{
public:
  using Func = std::function<void()>;

  virtual ~Executor() {}

  virtual void add(Func func) = 0;
};

}
