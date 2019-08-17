#ifndef MUDUO_BASE_ASYNCLOGGINGDOUBLEBUFFERING_H
#define MUDUO_BASE_ASYNCLOGGINGDOUBLEBUFFERING_H

#include "LogStream.h"

#include <functional>
#include <memory>
#include <vector>


#include "LogFile.h"
#include "Mutex.h"
#include "Thread.h"
#include "Condition.h"
#include "CountDownLatch.h"

namespace muduo
{

    __thread pid_t __cached_index = -1;
    template <class BufferType, class Guard>
    class LoggingBase{
    public:
        using Buffer = BufferType;
        using BufferVector = std::vector<std::unique_ptr<Buffer>>;
        using BufferPtr = std::unique_ptr<Buffer>;
        using StateVec = std::vector<std::atomic_uint32_t>;

        virtual Guard createGuard(size_t i) = 0;
        virtual void writeBuffersToFile(BufferVector& buffers, LogFile& output) = 0;

        LoggingBase(const string& basename, size_t roll_size, size_t partitions, size_t flushInterval, const Thread::ThreadFunc& func):
                running_(false),
                flushInterval_(flushInterval),
                basename_(basename),
                rollSize_(roll_size),
                partitions(partitions),
                thread_(func, "Logging"),
                latch_(1),
                mutex_(),
                cond_(mutex_),
                buffers_(),
                emptyBuffers(),
                buffer_per_thread(partitions)
        {
            int i = 0;
            buffers_.reserve(16);
            buffer_per_thread.resize(partitions);
            for(auto& currentBuffer : buffer_per_thread){
                BufferPtr buffer(new Buffer);
                buffer->bzero();
                currentBuffer = (std::move(buffer));
                // 初始化emptyBuffers
                if(i++ < 6){
                    BufferPtr buffer2(new Buffer);
                    emptyBuffers.push_back(std::move(buffer2));
                }
            }
        }

        void start()
        {
            running_ = true;
            thread_.start();
            latch_.wait();
        }

        void stop()
        {
            running_ = false;
            cond_.notify();
            thread_.join();
        }

        size_t getIndex(){
            if(__cached_index < 0){
                __cached_index = roundRobin.getAndAdd(1);
            }
            return __cached_index;
        }

        // index1 是 0 1 2 .. 7 0 1 2 ... 7
        size_t getIndex1(){
            // 使用roundRoubin的策略进行获取
            return getIndex() % partitions;
        }

        // index2是 [0 0 0 .. 0 ] [1 .. 1 ]... [2 ... 2]
        size_t getIndex2(){
            return (getIndex() / partitions) % partitions;
        }

        void sortBuffers(BufferVector& buffersToWrite){
            // 将Buffer排序,避免pageFault
            size_t threshold = partitions * 2;
            std::sort(buffersToWrite.begin(), buffersToWrite.end(), [](BufferPtr& b1, BufferPtr& b2){return b1->length() > b2->length();});
            if (buffersToWrite.size() > threshold)
            {
                buffersToWrite.resize(threshold);
            }
        }


        void flushBuffers(){
            // 将buffer_per_thread里面的东西写入到buffers里面
            for(size_t i = 0; i < partitions; ++i){
                MutexLockGuard guard(mutex_);
                const Guard& thread_guard = createGuard(i);
                auto& currentBuffer_ = buffer_per_thread[i];

                buffers_.emplace_back(std::move(currentBuffer_));
                if(!emptyBuffers.empty()){
                    currentBuffer_ = std::move(emptyBuffers.back());
                    emptyBuffers.pop_back();
                }else{
                    // 这个地方出现的频率并不低,说明写入的频率比较高
                    printf("new Buffer when timeout \n");
                    currentBuffer_.reset(new Buffer);
                }
            }
        }

        void refillEmpty(BufferVector & buffersToWrite){
            while (!buffersToWrite.empty())
            {
                //assert(!buffersToWrite.empty());
                auto& buffer = buffersToWrite.back();
                buffer->reset();
                emptyBuffers.push_back(std::move(buffer));
                buffersToWrite.pop_back();
            }
            buffersToWrite.clear();
        }


        void threadFunc()
        {
            assert(running_);
            LogFile output(basename_, rollSize_ * 4, false);
            BufferVector buffersToWrite;
            buffersToWrite.reserve(16);
            // 为什么在这里才latch_?,防止刚启动就关闭的情况出现时候,running_来不及执行
            latch_.countDown();
            bool timeout = false;

            while (running_)
            {
                assert(buffersToWrite.empty());
                // 因为需要访问buffers,所以使用notify锁,这个锁的范围不能覆盖到下面的循环,否则会导致死锁(加锁顺序问题)
                {
                    muduo::MutexLockGuard guard(mutex_);
                    if (buffers_.empty()) {
                        timeout = cond_.waitForSeconds(flushInterval_);
                    }
                    if(timeout){
                        flushBuffers();
                    }
                    buffersToWrite.swap(buffers_);
                }

                if(!buffersToWrite.empty()){
                    writeBuffersToFile(buffersToWrite, output);
                    sortBuffers(buffersToWrite);
                    {
                        MutexLockGuard lock2(mutex_);
                        refillEmpty(buffersToWrite);
                    }
                    output.flush();
                }
            }

            // 最后的结尾
            flushBuffers();
            writeBuffersToFile(buffersToWrite, output);
            writeBuffersToFile(buffers_, output);
            output.flush();
        }

    public:
        bool running_;
        const int flushInterval_;
        string basename_;
        size_t rollSize_;
        size_t partitions;
        AtomicInt32 roundRobin;
        muduo::Thread thread_;
        muduo::CountDownLatch latch_;
        muduo::MutexLockImpl mutex_;
        muduo::Condition cond_;
        BufferVector buffers_;
        BufferVector emptyBuffers;
        BufferVector buffer_per_thread;
    };


    class AsyncLoggingDoubleBufferingShards: public LoggingBase<muduo::detail::FixedBuffer<muduo::detail::kLargeBuffer>, MutexLockGuard>
    {

    public:
        AsyncLoggingDoubleBufferingShards(const string& basename, // FIXME: StringPiece
                                          size_t rollSize,
                                          int flushInterval = 1)
                :
                LoggingBase(basename, rollSize, 8, flushInterval, std::bind(&AsyncLoggingDoubleBufferingShards::threadFunc, this)),
                mutexs_(partitions){}


        void append(const char* logline, int len)
        {
            // 随机选择一个mutex进行锁
            size_t index = getIndex1();
            muduo::MutexLockGuard lock(mutexs_[index]);
            BufferPtr& currentBuffer_ = buffer_per_thread[index];

            if (currentBuffer_->avail() > len){
                currentBuffer_->append(logline, len);
            }
            else{
                MutexLockGuard guard(mutex_);
                buffers_.push_back(std::move(currentBuffer_));
                if (!emptyBuffers.empty()){
                    currentBuffer_ = std::move(emptyBuffers.back());
                    emptyBuffers.pop_back();
                }
                else{
                    printf("new Buffer\n");
                    currentBuffer_.reset(new Buffer); // Rarely happens
                }
                currentBuffer_->append(logline, len);
                cond_.notify();
            }
        }

    private:
        MutexLockGuard createGuard(size_t i) override {
            MutexLockGuard guard(mutexs_[i]);
            return guard;
        }

        void writeBuffersToFile(BufferVector& buffers, LogFile& output) override {
            for (auto& buffer : buffers){
                output.append(buffer->data(), buffer->length());
            }
        }


    private:
        std::vector<muduo::SpinLock> mutexs_;
    };



/***************CirCularBuffer版本**************************/

    class States {
    public:
        using State = std::atomic_uint32_t;
        using StateVec = std::vector<State>;

        States(size_t dimension1, size_t dimension2):
        dimension1(dimension1),
        dimension2(dimension2),
        states(dimension1){
            for(StateVec& stateArr : states){
                stateArr = StateVec(dimension2);
            }
        }

        State& get(size_t i, size_t j){
            return states[i][j];
        }

        StateVec& getVec(size_t i){
            return states[i];
        }

        size_t dimension1;
        size_t dimension2;
        std::vector<StateVec> states;

    };

    using CircularBuffer = muduo::detail::CircularBufferTemplate<muduo::detail::kLargeBuffer>;
    class AsyncLoggingDoubleBuffering: public LoggingBase<CircularBuffer, muduo::detail::WriteBarrier>
    {

        using WriteBarrier = muduo::detail::WriteBarrier;
        using ReadBarrier = muduo::detail::ReadBarrier;

    public:
        AsyncLoggingDoubleBuffering(const string& basename, // FIXME: StringPiece
                                    size_t rollSize,
                                    int flushInterval = 3,
                                    size_t partitions = 8)
                                    :
                LoggingBase(basename, rollSize, partitions, flushInterval, std::bind(&AsyncLoggingDoubleBuffering::threadFunc, this)),
                states(LoggingBase::partitions, LoggingBase::partitions){}


        void append(const char* logline, int len)
        {
            bool full;
            size_t remain = 0;
            auto index1 = getIndex1();
            auto index2 = getIndex2();
            BufferPtr& currentBuffer_ = buffer_per_thread[index1];

            {
                ReadBarrier rb(states.get(index1, index2));
                full = !currentBuffer_->append(logline, len, remain);
            }

            // 错开
            if(full || remain <= 8 * 1024 * (index1 + 1)){
                muduo::MutexLockGuard lock(mutex_);
                WriteBarrier wb(states.getVec(index1));

                buffers_.emplace_back(std::move(currentBuffer_));
                if(!emptyBuffers.empty()){
                    currentBuffer_ = std::move(emptyBuffers.back());
                    emptyBuffers.pop_back();
                }else{
                    printf("new buffer\n");
                    currentBuffer_.reset(new Buffer);
                }

                currentBuffer_->append(logline, len, remain);
                cond_.notify();
            }
        }


    private:
         void writeBuffersToFile(BufferVector& buffers, LogFile& output) override {
            for(auto& buffer : buffers){
                writeCircularToFile(buffer, output);
            }
        }

         static void writeCircularToFile(BufferPtr& buffer, LogFile& output)  {
            // 将一个circularBuffer写入到output里面
            size_t len = FETCH_SIZE;
            char* data = nullptr;
            while ((data = buffer->peek(len)) != nullptr){
                output.append(data, len);
                buffer->pop(len);
                len = FETCH_SIZE;
            }

            // 处理最后不足 FETCH_SIZE的部分
            if(len > 0 && len != FETCH_SIZE){
                data = buffer->peek(len);
                assert(data);
                output.append(data, len);
                buffer->pop(len);
            }
        }

        WriteBarrier createGuard(size_t i) override {
            WriteBarrier guard(states.getVec(i));
            return guard;
        }

    private:
        States states;
        static constexpr size_t FETCH_SIZE = 1024 * 1024 * 1;//每次写1M的数据尝试获取
    };
}
#endif  // MUDUO_BASE_ASYNCLOGGINGDOUBLEBUFFERING_H
