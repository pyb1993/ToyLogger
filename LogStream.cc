#include "LogStream.h"
#include "LogFile.h"

#include <algorithm>
#include <limits>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

using namespace muduo;
using namespace muduo::detail;

namespace muduo
{

    // 定义FmtMicroSeconds的静态变量
    pthread_once_t FmtMicroSeconds::ponce_ = PTHREAD_ONCE_INIT;   //初始化pthread_once
    char FmtMicroSeconds::strArray[1000][3];
    char FmtMicroSeconds::chr[10] = {'0','1','2','3','4','5','6','7','8','9'};


namespace detail
{

const char digits[] = "9876543210123456789";
const char* zero = digits + 9;

const char digitsHex[] = "0123456789abcdef";


// Efficient Integer to String Conversions, by Matthew Wilson.
template<typename T>
size_t convert(char buf[], T value)
{
  T i = value;
  char* p = buf;

  do
  {
    int lsd = i % 10;
    i /= 10;
    *p++ = zero[lsd];
  } while (i != 0);

  if (value < 0)
  {
    *p++ = '-';
  }
  *p = '\0';
  std::reverse(buf, p);

  return p - buf;
}

size_t convertHex(char buf[], uintptr_t value)
{
  uintptr_t i = value;
  char* p = buf;

  do
  {
    int lsd = i % 16;
    i /= 16;
    *p++ = digitsHex[lsd];
  } while (i != 0);

  *p = '\0';
  std::reverse(buf, p);

  return p - buf;
}

    template class FixedBuffer<kSmallBuffer>;
    template class FixedBuffer<kLargeBuffer>;
}
}

template<int SIZE>
void FixedBuffer<SIZE>::cookieStart()
{
}

template<int SIZE>
void FixedBuffer<SIZE>::cookieEnd()
{
}

/*circular buffer*/





/*
bool CircularBuffer::append(const char *data, size_t len) {
    // 将data压入,成功或者失败
    assert(data != nullptr && len > 0);

    // 这里不能使用 alloc - write来判断剩余容量,原因是那不是一次原子操作,write值获取之后read可能会变化
    // 多写单读的队列,所以这里只有一个读者,但是多个写者
    // 写入只有在前面的都写完的情况下才会进行返回,目的是

    auto idle = idle_count.fetch_sub(len);

    // 注意,这里存在溢出的风险
    if(idle >= 0 && idle >= len){
        auto alloc_start = alloc_count.fetch_add(len);
        auto alloc_end = alloc_start + len;

        auto real_start = alloc_start & capacity_mask;
        auto real_end = alloc_end & capacity_mask;
        if(real_start < real_end){
            //memcpy(static_cast<void*>(buffer + real_start), static_cast<const void *>(data),  len);
        }else{
            size_t first_len = capacity - real_start;
            //memcpy(static_cast<void*>(buffer + real_start), static_cast<const void*>(data),  first_len);
            //memcpy(static_cast<void*>(buffer + 0), static_cast<const void*>(data + first_len),  real_end);
        }

        // todo 这个地方需要优化性能
        while(true){
            // compare_exchange_weak这个操作会改变expected的值,所以每次需要重新设置
            auto tmp = alloc_start;
            if(write_count.compare_exchange_weak(tmp, alloc_end)){
                break;
            }
        }

        return true;
    }else{
        // 空间不够了,push失败,交给外界来处理
        idle_count.fetch_add(len);
        return false;
    }
}
 */






template<typename T>
void LogStream::formatInteger(T v)
{
  if (buffer_.avail() >= kMaxNumericSize)
  {
    size_t len = convert(buffer_.current(), v);
    buffer_.add(len);
  }
}

LogStream& LogStream::operator<<(short v)
{
  *this << static_cast<int>(v);
  return *this;
}

LogStream& LogStream::operator<<(unsigned short v)
{
  *this << static_cast<unsigned int>(v);
  return *this;
}

LogStream& LogStream::operator<<(int v)
{
  formatInteger(v);
  return *this;
}

LogStream& LogStream::operator<<(unsigned int v)
{
  formatInteger(v);
  return *this;
}

LogStream& LogStream::operator<<(long v)
{
  formatInteger(v);
  return *this;
}

LogStream& LogStream::operator<<(unsigned long v)
{
  formatInteger(v);
  return *this;
}

LogStream& LogStream::operator<<(long long v)
{
  formatInteger(v);
  return *this;
}

LogStream& LogStream::operator<<(unsigned long long v)
{
  formatInteger(v);
  return *this;
}

LogStream& LogStream::operator<<(const void* p)
{
  uintptr_t v = reinterpret_cast<uintptr_t>(p);
  if (buffer_.avail() >= kMaxNumericSize)
  {
    char* buf = buffer_.current();
    buf[0] = '0';
    buf[1] = 'x';
    size_t len = convertHex(buf+2, v);
    buffer_.add(len+2);
  }
  return *this;
}

// FIXME: replace this with Grisu3 by Florian Loitsch.
LogStream& LogStream::operator<<(double v)
{
  if (buffer_.avail() >= kMaxNumericSize)
  {
    int len = snprintf(buffer_.current(), kMaxNumericSize, "%.12g", v);
    buffer_.add(len);
  }
  return *this;
}

template<typename T>
Fmt::Fmt(const char* fmt, T val)
{
  length_ = snprintf(buf_, sizeof buf_, fmt, val);
  assert(static_cast<size_t>(length_) < sizeof buf_);
}






// Explicit instantiations

template Fmt::Fmt(const char* fmt, char);

template Fmt::Fmt(const char* fmt, short);
template Fmt::Fmt(const char* fmt, unsigned short);
template Fmt::Fmt(const char* fmt, int);
template Fmt::Fmt(const char* fmt, unsigned int);
template Fmt::Fmt(const char* fmt, long);
template Fmt::Fmt(const char* fmt, unsigned long);
template Fmt::Fmt(const char* fmt, long long);
template Fmt::Fmt(const char* fmt, unsigned long long);

template Fmt::Fmt(const char* fmt, float);
template Fmt::Fmt(const char* fmt, double);
