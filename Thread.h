// excerpts from http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (giantchen at gmail dot com)

#ifndef MUDUO_BASE_THREAD_H
#define MUDUO_BASE_THREAD_H

#include "Atomic.h"

#include <functional>
#include <pthread.h>
#include <string>

namespace muduo
{

class Thread
{
 public:
  using ThreadFunc = std::function<void ()>;

  explicit Thread(const ThreadFunc&, const std::string& name = std::string());
  ~Thread();

  void start();
  void join();

  bool started() const { return started_; }
  // pthread_t pthreadId() const { return pthreadId_; }
  pid_t tid() const { return *tid_; }
  const std::string& name() const { return name_; }

  static int numCreated() { return numCreated_.get(); }

 private:
  bool        started_;
  bool        joined_;
  pthread_t   pthreadId_;
  std::shared_ptr<pid_t> tid_;
  ThreadFunc  func_;
  std::string name_;

  static AtomicInt32 numCreated_;
};

namespace CurrentThread
{
  pid_t tid();
  const char* name();
  bool isMainThread();
}

}

#endif
