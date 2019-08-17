//
// Created by pengyibo on 2019-06-17.
//

#ifndef UNTITLED_MUTEX_H
#define UNTITLED_MUTEX_H


#include "Thread.h"
#include <assert.h>
#include <pthread.h>

namespace muduo
{

    class MutexLock{
    public:
        virtual void lock() = 0;
        virtual void unlock() = 0;
    };

    class SpinLock: public MutexLock
    {
        public:
            void lock() {
                while (locked.test_and_set()) {}
            }
            void unlock() {
                locked.clear();
            }

    private:
        std::atomic_flag locked = ATOMIC_FLAG_INIT ;
    };


    class MutexLockImpl:public MutexLock
    {
    public:
        MutexLockImpl()
                : holder_(0)
        {
            pthread_mutex_init(&mutex_, NULL);
        }

        ~MutexLockImpl()
        {
            assert(holder_ == 0);
            pthread_mutex_destroy(&mutex_);
        }

        bool isLockedByThisThread()
        {
            // 用来检查是不是被当前线程持有
            return holder_ == CurrentThread::tid();
        }

        void assertLocked()
        {
            assert(isLockedByThisThread());
        }

        // internal usage

        void lock()
        {
            pthread_mutex_lock(&mutex_);
            holder_ = CurrentThread::tid();
        }

        void unlock()
        {
            holder_ = 0;
            pthread_mutex_unlock(&mutex_);
        }

        pthread_mutex_t* getPthreadMutex() /* non-const */
        {
            return &mutex_;
        }

    private:

        pthread_mutex_t mutex_;
        pid_t holder_;
    };

    class MutexLockGuard
    {
    public:
        explicit MutexLockGuard(MutexLock& mutex) : mutex_(mutex)
        {
            mutex_.lock();
        }

        ~MutexLockGuard()
        {
            mutex_.unlock();
        }

    private:

        MutexLock& mutex_;
    };

}

// Prevent misuse like:
// MutexLockGuard(mutex_);
// A tempory object doesn't hold the lock for long!
#define MutexLockGuard(x) error "Missing guard object name"

#endif //UNTITLED_MUTEX_H
