#include "Logging.h"

#include "Thread.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <sstream>

namespace muduo
{

/*
class LoggerImpl
{
 public:
  typedef Logger::LogLevel LogLevel;
  LoggerImpl(LogLevel level, int old_errno, const char* file, int line);
  void finish();

  Timestamp time_;
  LogStream stream_;
  LogLevel level_;
  int line_;
  const char* fullname_;
  const char* basename_;
};
*/

__thread char t_errnobuf[512];
__thread char t_time[32];
__thread time_t t_lastSecond;
__thread Fmt* tidPtr = nullptr;



void defaultOutput(const char* msg, int len)
{
  size_t n = fwrite(msg, 1, len, stdout);
  //FIXME check n
  (void)n;
}

void defaultFlush()
{
  fflush(stdout);
}

Logger::OutputFunc g_output = defaultOutput;
Logger::FlushFunc g_flush = defaultFlush;

}

using namespace muduo;

inline char* getCachedBasename(const char* file)
{
    // 这个应该是thread-safe的,原因是file在运行起来的时候就不会变化了,而且是一个常量
    static __thread char* basename_ = nullptr;
    static __thread char* fullname_ = nullptr;
    static __thread char* path_sep_pos = nullptr;

    if(fullname_ == nullptr){
        fullname_ = const_cast<char*>(file);
        path_sep_pos = strrchr(fullname_, '/');
        basename_ = (path_sep_pos != nullptr) ? path_sep_pos + 1 : fullname_;
    }

    return basename_;
}

Logger::Impl::Impl(int savedErrno, const char* file, int line)
  : time_(Timestamp::now()),
    stream_(),
    line_(line),
    fullname_(file),
    basename_(NULL)
{
  basename_ = getCachedBasename(file);

  formatTime();
  if (tidPtr == nullptr){
      tidPtr = new Fmt("%5d ", CurrentThread::tid());
  }

  assert(tidPtr->length() == 6);
  stream_ << T(tidPtr->data(), 6);
}

void Logger::Impl::formatTime()
{
  int64_t microSecondsSinceEpoch = time_.microSecondsSinceEpoch();
  time_t seconds = static_cast<time_t>(microSecondsSinceEpoch / 1000000);
  int microseconds = static_cast<int>(microSecondsSinceEpoch % 1000000);
  if (seconds != t_lastSecond)
  {
    t_lastSecond = seconds;
    struct tm tm_time;
    ::gmtime_r(&seconds, &tm_time); // FIXME TimeZone::fromUtcTime

    int len = snprintf(t_time, sizeof(t_time), "%4d%02d%02d %02d:%02d:%02d",
        tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
        tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
    assert(len == 17);
  }

  // 这里是计算毫秒,但是在要求较高性能的时候可以忽略,现在考虑如何降低
  FmtMicroSeconds us(microseconds);
  assert(us.length() == 9);
  stream_ << T(t_time, 17) << T(us.data(), 9);
}

void Logger::Impl::finish()
{
  stream_ << " - " << basename_ << ':' << line_ << '\n';
}


Logger::Logger(const char* file, int line, const char* func)
  : impl_(0, file, line)
{
  impl_.stream_ << func << ' ';
}

Logger::Logger(const char* file, int line)
  : impl_(0, file, line)
{
}


Logger::~Logger()
{
  impl_.finish();
  const LogStream::Buffer& buf(stream().buffer());
  g_output(buf.data(), buf.length());
}


// 给g_output设置一个函数,用这个函数在析构的时候输出一点东西
// 因为析构的时候不能正常用自己的buffer和成员了
void Logger::setOutput(OutputFunc out)
{
  g_output = out;
}

void Logger::setFlush(FlushFunc flush)
{
  g_flush = flush;
}
