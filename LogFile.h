//
// Created by pengyibo on 2019-06-17.
//

#ifndef UNTITLED_LOGFILE_H
#define UNTITLED_LOGFILE_H

#include <vector>
#include <string>
#include <memory>
#include "Mutex.h"


class T;

namespace muduo
{

    using std::string;

    // 这个类会封装文件相关的操作
    class LogFile
    {
    public:
        LogFile(const string& basename,
                size_t rollSize,
                bool threadSafe = false,
                int flushInterval = 3);
        ~LogFile();
        void appendBatch(const std::vector<T>& buffers);
        void append(const char* logline, int len);
        void flush();

    private:
        void afterAppend();
        void appendv(std::vector<T>::const_iterator, std::vector<T>::const_iterator);
        static string getLogFileName(const string& basename, time_t* now);
        void rollFile();

        const string basename_;
        const size_t rollSize_;
        const int flushInterval_;

        int count_;

        std::unique_ptr<MutexLockImpl> mutex_;
        time_t startOfPeriod_;
        time_t lastRoll_;
        time_t lastFlush_;

        // 使用前置声明,减少头文件依赖
        class File;
        std::unique_ptr<File> file_;

        const static int kCheckTimeRoll_ = 1024;
        const static int kRollPerSeconds_ = 60 * 60 * 24;
    };

}
#endif //UNTITLED_LOGFILE_H
