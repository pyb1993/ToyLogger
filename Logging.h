//
// Created by pengyibo on 2019-06-17.
//

#ifndef UNTITLED_LOGGING_H
#define UNTITLED_LOGGING_H
#include "LogStream.h"
#include "Timestamp.h"
namespace muduo
{

    class Logger
    {
    public:
        Logger(const char* file, int line);
        Logger(const char* file, int line, const char* func);
        Logger(const char* file, int line, bool toAbort);
        ~Logger();

        LogStream& stream() { return impl_.stream_; }
        using OutputFunc = void(*)(const char*, int);
        using FlushFunc = void(*)();
        static void setOutput(OutputFunc);
        static void setFlush(FlushFunc);

    private:

        class Impl
        {
        public:
            Impl(int old_errno, const char* file, int line);
            void formatTime();
            void finish();
            LogStream stream_;
            Timestamp time_;
            int line_;
            const char* fullname_;
            const char* basename_;
        };

        Impl impl_;

    };

// 就是一个 LogStream对象
#define LOG_INFO  (muduo::Logger(__FILE__, __LINE__).stream())



// Taken from glog/logging.h
//
// Check that the input is non NULL.  This very useful in constructor
// initializer lists.

#define CHECK_NOTNULL(val) \
  ::muduo::CheckNotNull(__FILE__, __LINE__, "'" #val "' Must be non NULL", (val))

// A small helper for CHECK_NOTNULL().
    template <typename T>
    T* CheckNotNull(const char *file, int line, const char *names, T* ptr) {
        if (ptr == nullptr) {
            Logger(file, line).stream() << names;
        }
        return ptr;
    }

    template<typename To, typename From>
    inline To implicit_cast(From const &f) {
        return f;
    }

}




#endif //UNTITLED_LOGGING_H
