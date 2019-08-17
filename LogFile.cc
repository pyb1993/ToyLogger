#include "LogFile.h"
#include "util.h"
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <vector>
#include <sys/uio.h>


using namespace muduo;


#ifdef __APPLE__
#define fwrite_unlocked fwrite
#endif





// not thread safe
class LogFile::File
{
 public:
  explicit File(const string& filename)
    : fp_(::fopen(filename.data(), "ae")),
      writtenBytes_(0)
  {
    ::setbuffer(fp_, buffer_, sizeof buffer_);
  }

  ~File()
  {
    ::fclose(fp_);
  }


  void append(const char* logline, const size_t len)
  {
    size_t n = write(logline, len);
    size_t remain = len - n;
    while (remain > 0)
    {
      size_t x = write(logline + n, remain);
      if (x == 0)
      {
        int err = ferror(fp_);
        if (err)
        {
          char buf[128];
          strerror_r(err, buf, sizeof buf); // FIXME: strerror_l
          fprintf(stderr, "LogFile::File::append() failed %s\n", buf);
        }
        break;
      }
      n += x;
      remain = len - n; // remain -= x
    }

    writtenBytes_ += len;
  }

  void appendBatch(std::vector<T>::const_iterator buffer1, std::vector<T>::const_iterator end){
      int i = 0;
      int shouldWrite = 0;
      struct iovec iov[32];
      while(buffer1 != end) {
          iov[i].iov_base = const_cast<char*>(buffer1->str_);
          iov[i].iov_len = buffer1->len_;
          shouldWrite += buffer1->len_;
          buffer1++;
          i++;

      }


      size_t writed = writev(fileno(fp_), iov, i);
      assert(writed == shouldWrite);
      writtenBytes_ += writed;
  }

  void flush()
  {
    ::fflush(fp_);
  }

  size_t writtenBytes() const { return writtenBytes_; }

 private:

  size_t write(const char* logline, size_t len)
  {
    return fwrite_unlocked(logline, 1, len, fp_);
  }

  FILE* fp_;
  char buffer_[64*1024];
  size_t writtenBytes_;
};

LogFile::LogFile(const string& basename,
                 size_t rollSize,
                 bool threadSafe,
                 int flushInterval)
  : basename_(basename),
    rollSize_(rollSize),
    flushInterval_(flushInterval),
    count_(0),
    mutex_(threadSafe ? new MutexLockImpl : NULL),
    startOfPeriod_(0),
    lastRoll_(0),
    lastFlush_(0)
{
  assert(basename.find('/') == string::npos);
  rollFile();
}

LogFile::~LogFile()
{
    flush();
}



void LogFile::flush()
{
  if (mutex_)
  {
    MutexLockGuard lock(*mutex_);
    file_->flush();
  }
  else
  {
    file_->flush();
  }
}


void LogFile::append(const char *logline, int len) {
    file_->append(logline, len);
    afterAppend();
}


void LogFile::appendBatch(const std::vector<T>& buffers)
{
    constexpr size_t batch = 32;
    int len = buffers.size();

    int i = 0;
    for(; i + batch < len; i += batch){
        file_->appendBatch(buffers.begin(), buffers.begin() + batch);
    }

    file_->appendBatch(buffers.begin() + i, buffers.end());

    afterAppend();
}


void LogFile::afterAppend(){
    if (file_->writtenBytes() > rollSize_)
    {
        rollFile();
    }
    else
    {
        if (count_ > kCheckTimeRoll_)
        {
            count_ = 0;
            time_t now = ::time(NULL);
            time_t thisPeriod_ = now / kRollPerSeconds_ * kRollPerSeconds_;
            if (thisPeriod_ != startOfPeriod_)
            {
                rollFile();
            }
            else if (now - lastFlush_ > flushInterval_)
            {
                lastFlush_ = now;
                file_->flush();
            }
        }
        else
        {
            ++count_;
        }
    }
}

void LogFile::rollFile()
{
  time_t now = 0;
  string filename = getLogFileName(basename_, &now);
  time_t start = now / kRollPerSeconds_ * kRollPerSeconds_;

  if (now > lastRoll_)
  {
    lastRoll_ = now;
    lastFlush_ = now;
    startOfPeriod_ = start;
    file_.reset(new File(filename));
  }
}

string LogFile::getLogFileName(const string& basename, time_t* now)
{
  string filename;
  filename.reserve(basename.size() + 32);
  filename = basename;

  char timebuf[32];
  char pidbuf[32];
  struct tm tm;
  *now = time(NULL);
  gmtime_r(now, &tm); // FIXME: localtime_r ?
  strftime(timebuf, sizeof timebuf, ".%Y%m%d-%H%M%S", &tm);
  filename += timebuf;
  snprintf(pidbuf, sizeof pidbuf, ".%d", ::getpid()); // FIXME: ProcessInfo::pid();
  filename += pidbuf;
  filename += ".log";

  return filename;
}

