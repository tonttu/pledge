#pragma once

#include <mutex>
#include <vector>

#include "Executor.hpp"

namespace Pledge {

// An executor that adds tasks to a queue that needs to be manually executed by
// calling run().
//
// Multithreaded applications could have these in various threads and different
// thread event loops or main loops could call run() on them periodically, so
// you could easily write continuations that jump between relevant threads in
// the application.
class ManualExecutor : public Executor
{
public:
  inline void add(Func func) override
  {
    std::lock_guard<std::mutex> g(m_queueMutex);
    m_queue.push_back(std::move(func));
  }

  inline size_t run()
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
