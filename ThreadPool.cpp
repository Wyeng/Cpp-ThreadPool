#include "ThreadPool.h"
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <unistd.h>
const int ThreadPool::NUMBER;
// 构造函数
ThreadPool::ThreadPool(int min, int max)
    : taskQ(nullptr),
    managerID(),
    minNum(min),
    maxNum(max),
    busyNum(0),
    liveNum(0),
    exitNum(0),
    shutdown(false),
    managerStarted(false)
{
    // 判断边界条件
    if (min <= 0 || max < min)
    {
        throw std::invalid_argument("Invalid thread count");
    }
    // 申请内存
    taskQ = new TaskQueue;

    if (pthread_mutex_init(&mutexPool, nullptr) != 0)
    {
        delete taskQ;
        throw std::runtime_error("mutex init failed");
    }

    if (pthread_cond_init(&notEmpty, nullptr) != 0)
    {
        pthread_mutex_destroy(&mutexPool);
        delete taskQ;
        throw std::runtime_error("condition init failed");
    }

    try
    {
        // 提前分配内存大小
        threadIDs.reserve(maxNum);

        // 按照最小线程数大小创建初始工作线程
        for (int i = 0; i < minNum; ++i)
        {
            pthread_t tid;

            if (pthread_create(&tid, nullptr, worker, this) != 0)
            {
                throw std::runtime_error("worker create failed");
            }

            ++liveNum;
            threadIDs.push_back(tid);
        }

        // 创建管理线程
        if (pthread_create(&managerID, nullptr, manager, this) != 0)
        {
            throw std::runtime_error("manager create failed");
        }
        // 初始化管理者线程开始标志
        managerStarted = true;
    }
    catch (...)
    {
        // 异常处理
        pthread_mutex_lock(&mutexPool);
        shutdown = true;
        pthread_cond_broadcast(&notEmpty);
        pthread_mutex_unlock(&mutexPool);

        for (pthread_t tid : threadIDs)
        {
            pthread_join(tid, nullptr);
        }

        pthread_cond_destroy(&notEmpty);
        pthread_mutex_destroy(&mutexPool);
        delete taskQ;

        throw;
    }
}

// 析构函数
ThreadPool::~ThreadPool()
{
    pthread_mutex_lock(&mutexPool);

    shutdown = true;
    exitNum = 0;

    // 唤醒阻塞的工作线程
    pthread_cond_broadcast(&notEmpty);

    pthread_mutex_unlock(&mutexPool);

    // 等待管理线程退出
    if (managerStarted)
    {
        pthread_join(managerID, nullptr);
    }

    // 等待全部工作线程退出
    for (pthread_t tid : threadIDs)
    {
        std::cout << "thread " << std::to_string(pthread_self()) << " is ending" << std::endl;
        pthread_join(tid, nullptr);
    }

    // 清除申请的内存
    delete taskQ;

    pthread_cond_destroy(&notEmpty);
    pthread_mutex_destroy(&mutexPool);
}

// 添加任务
void ThreadPool::addTask(Task task)
{
    pthread_mutex_lock(&mutexPool);

    if (shutdown)
    {
        pthread_mutex_unlock(&mutexPool);
        return;
    }

    taskQ->addTask(task);

    pthread_cond_signal(&notEmpty);

    pthread_mutex_unlock(&mutexPool);
}

// 获取忙碌线程数量
int ThreadPool::getBusyNum()
{
    pthread_mutex_lock(&mutexPool);

    int num = busyNum;

    pthread_mutex_unlock(&mutexPool);

    return num;
}

// 获取存活线程数量
int ThreadPool::getAliveNum()
{
    pthread_mutex_lock(&mutexPool);

    int num = liveNum;

    pthread_mutex_unlock(&mutexPool);

    return num;
}

// 工作线程
void* ThreadPool::worker(void* arg)
{
    ThreadPool* pool = static_cast<ThreadPool*>(arg);

    while (true)
    {
        pthread_mutex_lock(&pool->mutexPool);

        while (pool->taskQ->taskNumber() == 0 && !pool->shutdown)
        {
            pthread_cond_wait(&pool->notEmpty, &pool->mutexPool);

            // 动态缩容
            if (!pool->shutdown && pool->exitNum > 0 && pool->liveNum > pool->minNum)
            {
                --pool->exitNum;
                --pool->liveNum;

                pthread_mutex_unlock(&pool->mutexPool);
                return nullptr;
            }
        }

        // 平滑关闭：队列清空后退出
        if (pool->shutdown && pool->taskQ->taskNumber() == 0)
        {
            --pool->liveNum;

            pthread_mutex_unlock(&pool->mutexPool);
            return nullptr;
        }

        Task task = pool->taskQ->takeTask();

        ++pool->busyNum;

        pthread_mutex_unlock(&pool->mutexPool);

        // 执行任务
        if (task.function != nullptr)
        {
            task.function(task.arg);
        }

        // 当前测试任务的参数是 int*
        delete static_cast<int*>(task.arg);

        pthread_mutex_lock(&pool->mutexPool);
        // 执行完任务后更新busyNum
        --pool->busyNum;

        pthread_mutex_unlock(&pool->mutexPool);
    }

    return nullptr;
}

// 管理线程
void* ThreadPool::manager(void* arg)
{
    ThreadPool* pool = static_cast<ThreadPool*>(arg);

    while (true)
    {
        // 每隔3秒检测一次
        sleep(3);

        pthread_mutex_lock(&pool->mutexPool);

        if (pool->shutdown)
        {
            pthread_mutex_unlock(&pool->mutexPool);
            break;
        }

        int queueSize = static_cast<int>(pool->taskQ->taskNumber());

        int live = pool->liveNum;
        int busy = pool->busyNum;

        // 动态扩容
        if (queueSize > live && live < pool->maxNum)
        {
            int count = std::min(NUMBER,pool->maxNum - live);

            for (int i = 0; i < count; ++i)
            {
                pthread_t tid;

                // 预留空间由构造函数 reserve 保证
                if (pthread_create(&tid, nullptr, worker, pool) == 0)
                {
                    ++pool->liveNum;
                    pool->threadIDs.push_back(tid);
                }
            }
        }

        // 动态缩容
        else if (busy * 2 < live && live > pool->minNum)
        {
            int count = std::min(NUMBER,live - pool->minNum);
            // 避免重复累积退出请求
            if (pool->exitNum == 0)
            {
                pool->exitNum = count;

                for (int i = 0; i < count; ++i)
                {
                    // 唤醒阻塞的工作线程
                    pthread_cond_signal(&pool->notEmpty);
                }
            }
        }

        pthread_mutex_unlock(&pool->mutexPool);
    }

    return nullptr;
}
