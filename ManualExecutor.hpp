#pragma once

#include <mutex>
#include <vector>

#include "Executor.hpp"

namespace Pledge {

class ManualExecutor : public Executor
{
public:
  void add(Func func) override
  {
    std::lock_guard<std::mutex> g(m_queueMutex);
    m_queue.push_back(std::move(func));
  }

  size_t run()
  {
    std::vector<Func> todo;
    {
      std::unique_lock<std::mutex> lock(m_queueMutex);
      std::swap(todo, m_queue);
    }
    for (Func& f : todo)
      f();
    return todo.size();
  }

private:
  std::vector<Func> m_queue;
  std::mutex m_queueMutex;
};

}
