#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "Executor.hpp"

namespace Pledge {

class ThreadPoolExecutor : public Executor
{
public:
  inline void add(Func func) override
  {
    {
      std::lock_guard<std::mutex> g(m_queueMutex);
      m_queue.push(std::move(func));
    }
    m_queueCond.notify_one();
  }

  inline ThreadPoolExecutor(size_t threadCount = 8)
  {
    m_threads.reserve(threadCount);
    for (size_t i = 0; i < threadCount; ++i)
      m_threads.emplace_back(std::bind(&ThreadPoolExecutor::exec, this));
  }

  inline ~ThreadPoolExecutor()
  {
    {
      std::unique_lock<std::mutex> lock(m_queueMutex);
      m_running = false;
    }
    m_queueCond.notify_all();

    for (std::thread& t : m_threads)
      t.join();
  }

private:
  inline void exec()
  {
    for (;;) {
      Func func;
      {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        while (m_running && m_queue.empty())
          m_queueCond.wait(lock);

        if (m_queue.empty())
          break;

        func = std::move(m_queue.front());
        m_queue.pop();
      }
      func();
    }
  }

private:
  std::vector<std::thread> m_threads;
  std::queue<Func> m_queue;
  std::mutex m_queueMutex;
  std::condition_variable m_queueCond;
  bool m_running = true;
};

}
