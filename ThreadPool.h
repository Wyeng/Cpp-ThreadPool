#include"TaskQueue.h"
class ThreadPool
{
private:
	// 任务队列
	TaskQueue* taskQ;

	pthread_t managerID;	// 管理者线程ID
	pthread_t* threadIDs;	// 工作的线程ID
	int minNum;				// 最小线程数
	int maxNum;				// 最大线程数
	int busyNum;			// 忙的线程数
	int liveNum;			// 存活的线程数
	int exitNum;			// 要销毁的线程数

	pthread_mutex_t mutexPool;	// 锁整个线程池
	pthread_cond_t notEmpty;	// 任务队列是否为空

	bool shutdown;	// 是否销毁线程池

public:
	// 构造函数
	ThreadPool(int min,int max);
	// 析构函数
	~ThreadPool();

	// 给线程池添加任务
	void addTask(Task task);

	// 获取线程池中工作的线程个数
	int getBusyNum();

	// 获取线程池中活着的线程个数
	int getAliveNum();

private:
	// 工作线程的任务函数
	static void* worker(void* arg);
	// 管理者线程任务函数
	static void* manager(void* arg);
	// 单个线程退出
	void threadExit();
};