# C++ ThreadPool - 基于 pthread 的动态线程池

## 1. 项目简介

本项目使用 C++ 和 POSIX Threads（pthread）实现了一个支持动态扩容和缩容的线程池。

线程池通过任务队列统一管理待执行任务，使用多个工作线程并发执行任务，并通过管理者线程动态调整工作线程数量，提高线程资源的利用率。

项目主要用于学习和实践 Linux 多线程编程、线程同步、互斥锁、条件变量以及线程池的基本实现原理。

## 2. 项目功能

- **任务队列管理**：使用 `std::queue` 存储待执行任务，支持任务添加和取出。
- **多线程执行**：使用 `pthread_create()` 创建多个工作线程，并发执行任务。
- **动态扩容**：当任务数量较多、工作线程不足时，自动增加工作线程。
- **动态缩容**：当工作线程负载较低时，自动减少工作线程数量。
- **线程同步**：使用 `pthread_mutex_t` 和 `pthread_cond_t` 保证多线程环境下的数据安全。
- **线程状态管理**：记录线程池中的存活线程数量、忙碌线程数量和待销毁线程数量。
- **安全关闭**：关闭线程池时唤醒工作线程，等待管理线程和工作线程退出，并回收线程资源。
- **平滑关闭**：停止接收新任务后，等待任务队列中的剩余任务执行完成，再退出工作线程。

## 3. 技术栈

| 技术 | 说明 |
|---|---|
| C++ | 面向对象编程 |
| pthread | Linux POSIX 多线程编程 |
| std::queue | 任务队列管理 |
| std::vector | 保存工作线程 ID |
| pthread_mutex_t | 互斥锁 |
| pthread_cond_t | 条件变量 |
| CMake / g++ | 项目构建 |

## 4. 项目结构

```text
C++-ThreadPool/
│
├── main.cpp          # 主函数，线程池测试程序
│
├── ThreadPool.h      # 线程池类声明
├── ThreadPool.cpp    # 线程池核心实现
│
├── TaskQueue.h       # 任务队列类声明及 Task 结构体
├── TaskQueue.cpp     # 任务队列具体实现
│
└── README.md         # 项目说明文档
```

## 5. 核心设计

### 5.1 Task 任务结构体

使用函数指针和 `void*` 参数封装任务：

```cpp
using callback = void (*)(void* arg);

struct Task
{
    callback function;
    void* arg;
};
```

其中：

- `function`：指向具体的任务执行函数。
- `arg`：任务函数所需要的参数。

### 5.2 TaskQueue 任务队列

使用 `std::queue<Task>` 管理任务。

主要接口：

```cpp
void addTask(Task task);
void addTask(callback f, void* arg);
Task takeTask();
size_t taskNumber();
```

通过互斥锁保护任务队列，保证多个线程同时访问队列时的数据安全。

### 5.3 ThreadPool 线程池

线程池主要维护以下数据：

| 成员变量 | 作用 |
|---|---|
| taskQ | 任务队列 |
| managerID | 管理者线程 ID |
| threadIDs | 工作线程 ID 集合 |
| minNum | 最小工作线程数量 |
| maxNum | 最大工作线程数量 |
| busyNum | 当前忙碌线程数量 |
| liveNum | 当前存活线程数量 |
| exitNum | 待销毁线程数量 |
| mutexPool | 线程池互斥锁 |
| notEmpty | 任务队列非空条件变量 |
| shutdown | 线程池关闭标志 |

### 5.4 工作线程 worker()

工作线程主要负责：

1. 获取线程池互斥锁。
2. 判断任务队列是否为空。
3. 如果没有任务，则通过 `pthread_cond_wait()` 阻塞等待。
4. 从任务队列中取出任务。
5. 更新忙碌线程数量。
6. 解锁并执行任务。
7. 任务执行完成后释放当前测试任务的参数。
8. 更新忙碌线程数量，继续等待或执行下一个任务。

### 5.5 管理线程 manager()

管理线程每隔 3 秒检查一次线程池状态。

**动态扩容：**

当满足以下条件时增加工作线程：

- 当前任务队列中的任务数量大于存活线程数量。
- 当前存活线程数量小于最大线程数量。

每次最多增加 `NUMBER` 个线程。

**动态缩容：**

当满足以下条件时减少工作线程：

- 忙碌线程数量的两倍小于存活线程数量。
- 当前存活线程数量大于最小线程数量。

每次最多减少 `NUMBER` 个线程。

其中：

```cpp
static const int NUMBER = 2;
```

表示每次动态调整的线程数量上限。

### 5.6 线程池安全关闭

线程池析构时执行以下操作：

1. 设置 `shutdown` 关闭标志。
2. 使用 `pthread_cond_broadcast()` 唤醒所有等待中的工作线程。
3. 使用 `pthread_join()` 等待管理线程退出。
4. 使用 `pthread_join()` 等待所有工作线程退出。
5. 释放任务队列及其他资源。
6. 销毁互斥锁和条件变量。

采用平滑关闭策略，保证已经进入任务队列的任务能够执行完成。

## 6. 编译与运行

### 环境要求

- Linux（Ubuntu）
- GCC / G++
- POSIX Threads（pthread）
- C++11 或更高版本

### 使用 g++ 编译

在项目根目录下打开终端，执行：

```bash
g++ -std=c++11 main.cpp ThreadPool.cpp TaskQueue.cpp -pthread -o ThreadPool
```

### 运行程序

```bash
./ThreadPool
```

## 7. 测试示例

在 `main.cpp` 中创建线程池：

```cpp
ThreadPool pool(3, 10);
```

其中：

- 最小工作线程数量：3。
- 最大工作线程数量：10。

向线程池添加 100 个任务：

```cpp
for (int i = 0; i < 100; i++)
{
    int* num = new int(i + 100);
    pool.addTask(Task(taskFunc, num));
}
```

每个任务执行时输出当前线程 ID 和任务参数，并休眠 1 秒。

通过观察程序运行输出，可以验证：

- 多个工作线程是否能够并发执行任务。
- 线程池是否能够动态增加工作线程。
- 线程池是否能够动态减少工作线程。
- 任务是否能够正常执行。
- 线程池关闭时是否能够正常回收线程资源。

## 8. 学习收获

通过本项目，主要学习和实践了以下内容：

1. Linux 下 pthread 多线程编程。
2. `pthread_create()` 创建线程。
3. `pthread_join()` 等待线程结束及回收线程资源。
4. `pthread_mutex_lock()` 和 `pthread_mutex_unlock()` 实现互斥访问。
5. `pthread_cond_wait()`、`pthread_cond_signal()` 和 `pthread_cond_broadcast()` 实现线程间同步。
6. 使用任务队列实现生产者与消费者之间的任务传递。
7. 线程池动态扩容与缩容的基本实现思路。
8. 多线程环境下的资源管理和线程安全问题。

## 9. 后续改进方向

- 使用智能指针优化任务参数的内存管理。
- 支持不同类型的任务参数和任务返回值。
- 增加任务执行异常处理机制。
- 增加任务队列容量限制。
- 完善线程池关闭和任务提交失败时的资源管理。
- 使用 C++11 标准库进一步优化线程池实现。

---

**项目定位：** Linux C++ 多线程编程实践项目。

**开发目的：** 深入理解 pthread、多线程同步机制、任务队列以及动态线程池的实现原理。
