// excerpts from http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (giantchen at gmail dot com)

#ifndef MUDUO_BASE_THREADPOOL_H
#define MUDUO_BASE_THREADPOOL_H

#include "Condition.h"
#include "Mutex.h"
#include "Thread.h"

#include <functional>
#include <vector>
#include <deque>

namespace muduo
{

class ThreadPool
{
 public:
  typedef std::function<void ()> Task;

  explicit ThreadPool(const std::string& name = std::string());
  ~ThreadPool();

  void start(int numThreads);
  void stop();

  void run(const Task& f);

 private:
  void runInThread();
  Task take();

  MutexLockImpl mutex_;
  Condition cond_;
  std::string name_;
  std::vector<std::unique_ptr<muduo::Thread>> threads_;
  std::deque<Task> queue_;
  bool running_;
};

}

#endif
