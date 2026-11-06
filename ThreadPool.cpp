#include "ThreadPool.h"
#include<iostream>
#include<string.h>
#include<string>
ThreadPool::ThreadPool(int min, int max)
{

	do
	{
		// 实例化任务队列
		taskQ = new TaskQueue;
		if (taskQ == nullptr)
		{
			std::cout << "new taskQ fail..." << std::endl;
			break;
		}

		// 实例化工作线程号
		threadIDs = new pthread_t[max];
		if (threadIDs == nullptr)
		{
			std::cout << "new threadIDs fail..." << std::endl;
			break;
		}
		memset(threadIDs, 0, sizeof(pthread_t) * max);

		minNum = min;
		maxNum = max;
		busyNum = 0;
		liveNum = min;
		exitNum = 0;

		// 判断锁和条件变量是否初始化成功
		if (pthread_mutex_init(&mutexPool, NULL) != 0 ||
			pthread_cond_init(&notEmpty, NULL) != 0)
		{
			std::cout << "mutex or condition init fail..." << std::endl;
			break;
		}

		shutdown = false;

		// 创建管理者线程
		pthread_create(&managerID, NULL, manager, this);
		// 创建工作者线程
		for (int i = 0;i < min;i++)
		{
			pthread_create(&threadIDs[i], NULL, worker, this);
		}
		return;
	} while (0);

	// 释放资源
	if (threadIDs) delete[] threadIDs;
	if (taskQ) delete taskQ;
}

ThreadPool::~ThreadPool()
{
}

void ThreadPool::addTask(Task task)
{
}

int ThreadPool::getBusyNum()
{
	return 0;
}

int ThreadPool::getAliveNum()
{
	return 0;
}

void* ThreadPool::worker(void* arg)
{
	ThreadPool* pool = static_cast<ThreadPool*>(arg);
	while (true)
	{
		pthread_mutex_lock(&pool->mutexPool);
		// 判断任务队列是否为空
		while (pool->taskQ->taskNumber() == 0 && !pool->shutdown)
		{
			// 阻塞工作线程
			pthread_cond_wait(&pool->notEmpty, &pool->mutexPool);

			// 判断是不是要销毁线程
			if (pool->exitNum > 0)
			{
				pool->exitNum--;
				if (pool->liveNum > pool->minNum)
				{
					pool->liveNum--;
					// 解锁
					pthread_mutex_unlock(&pool->mutexPool);
					pool->threadExit();
				}
			}
		}
		// 判断线程池是否关闭了
		if (pool->shutdown)
		{
			pthread_mutex_unlock(&pool->mutexPool);
			pool->threadExit();
		}

		// 从任务队列中取出一个任务
		Task task = pool->taskQ->takeTask();

		pool->busyNum++;
		// 解锁
		pthread_mutex_unlock(&pool->mutexPool);
		std::cout << "thread " <<  std::to_string(pthread_self()) << " start working..." << std::endl;

		//执行任务
		task.function(task.arg);
		delete task.arg;
		task.arg = nullptr;

		std::cout << "thread " << std::to_string(pthread_self()) << " end working..." << std::endl;

		pthread_mutex_lock(&pool->mutexPool);
		pool->busyNum--;
		pthread_mutex_unlock(&pool->mutexPool);
	}
	return NULL;
}

void* ThreadPool::manager(void* arg)
{
	return nullptr;
}

void ThreadPool::threadExit()
{
	pthread_t tid = pthread_self();
	for (int i = 0;i < maxNum;i++)
	{
		if (threadIDs[i] == tid)
		{
			threadIDs[i] = 0;
			std::cout << "threadExit() called, " << std::to_string(tid) << " exiting..." << std::endl;
			break;
		}
	}
	// 线程退出
	pthread_exit(NULL);
}
