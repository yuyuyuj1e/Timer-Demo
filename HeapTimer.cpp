/** 
 * @author: yuyuyuj1e 807152541@qq.com
 * @github: https://github.com/yuyuyuj1e
 * @csdn: https://blog.csdn.net/yuyuyuj1e
 * @date: 2023-04-03 16:44:31
 * @last_edit_time: 2023-04-04 16:10:03
 * @file_path: /Timer-Demo/HeapTimer.cpp
 * @description: 基于最小堆实现的定时器
 */

#include <iostream>
#include <vector>
#include <sys/epoll.h>
#include <chrono>
#include <set>
#include <functional>
#include <memory>

using namespace std;
using namespace chrono;


bool g_flag = true;


struct NodeBase {
    time_t expire;  // 过期时间
    int64_t id;  // 全局唯一 id
};


struct TimerNode : public NodeBase {
    // 回调函数
    using Callback = std::function<void(const TimerNode &node)>;  
    Callback func;

    int repeat = 1;  // 重复次数
    time_t msec;  // 执行时间

    TimerNode(time_t msec, time_t exp, int64_t id, Callback func, int repeat) : func(func), repeat(repeat), msec(msec) {
        this->expire = exp;
        this->id = id;
    }

    TimerNode(const TimerNode &node) : func(node.func), repeat(node.repeat), msec(node.msec) {
        this->expire = node.expire;
        this->id = node.id;
    }

    TimerNode& operator=(const TimerNode &node) {
        if (this != &node) {
            msec = node.msec;
            expire = node.expire;
            id = node.id;
            func = node.func;
            repeat = node.repeat;
        }
        return *this;
    }
};


/** 
 * @description: 定时器类
 */
class HeapTimer {
private:
    vector<TimerNode> timer;
    static int64_t g_id;
    static int64_t getID() {
        return g_id++;
    }

    void siftUp(int start);
    void siftDown(int start, int end);

public:
    static time_t getTick();
    int64_t addTimer(time_t msec, TimerNode::Callback func, int repeat = 1);
    bool delTimer(int64_t timer_id);
    bool checkTimer();
    time_t timeToSleep();

	friend std::ostream& operator<<(std::ostream &out, HeapTimer &timer);
};


int64_t HeapTimer::g_id = 0;


/** 
 * @description: 向上调整小顶堆，用于插入
 * @param {int} start: 插入节点
 */
void HeapTimer::siftUp(int start) {
    int son = start, parent = (son - 1) / 2;
 
	while (son > 0) {  // 条件二
		if (timer[parent].expire < timer[son].expire) {  // 子节点大于父节点，条件一
			break;
		}
		else {
			// 交换父子节点
			TimerNode temp = timer[son];
			timer[son] = timer[parent];
			timer[parent] = temp;

			// 获取下一轮父子节点下标
			son = parent;  // 子节点(本节点)新下标
			parent = (son - 1) / 2;  // 父节点(下次循环的父节点)的新下标
		}
	}
}

/** 
 * @description: 向下调整小顶堆，用于删除
 * @param {int} start: 删除节点
 * @param {int} end: 结束节点
 */
void HeapTimer::siftDown(int start, int end) {
    int parent = start;
	int son = 2 * parent + 1;  // lson: i * 2 + 1，rson: i * 2 + 2
 
	while (son <= end) {
		// 让 son 指向更小的子节点
		if (son < end && timer[son + 1].expire < timer[son].expire) {
			son++;
		}
 
		// 两个子节点都比父节点的大
		if (timer[son].expire > timer[parent].expire) {
			break;  
		}
		else {
			// 交换节点
			TimerNode temp = timer[parent];
			timer[parent] = timer[son];
			timer[son] = temp;

			// 更新下标
			parent = son;  // 父节点(本节点)的新下标
			son = 2 * son + 1;  // 子节点(下次循环的子节点)的新下标
		}
	}
}

time_t HeapTimer::getTick()
{
    auto sec = time_point_cast<milliseconds>(steady_clock::now());
    auto tmp = duration_cast<milliseconds>(sec.time_since_epoch());
    return tmp.count();
}

/** 
 * @description: 添加定时任务
 * @param {time_t} msec: 定时时间
 * @param {Callback} func: 回调函数
 * @param {int64_t} 定时器 id
 */
int64_t HeapTimer::addTimer(time_t msec, TimerNode::Callback func, int repeat) {
    time_t expire = HeapTimer::getTick() + msec;
    int64_t timer_id = HeapTimer::getID();
    TimerNode node(msec, expire, timer_id, func, repeat);
    timer.push_back(node);
    siftUp(timer.size() - 1);
    return timer_id;
}



/** 
 * @description: 删除定时器
 * @param {int64_t} timer_id: 需要删除的定时器的 ID
 */
bool HeapTimer::delTimer(int64_t timer_id) {
    for (int i = 0; i < timer.size(); ++i) {
        if (timer[i].id == timer_id) {  // 找到待删除的节点
            // 调整堆
            TimerNode del_timer = timer[i];
            timer[i] = timer[timer.size() - 1];
            siftDown(i, timer.size() - 1);

            // 删除节点
            timer.erase(timer.end() - 1);
        }
    }
    return false;
}

/** 
 * @description: 检测并找到剩余时间最少的定时器
 */
bool HeapTimer::checkTimer() {
    auto iter = timer.begin();  // 找到最小的节点，最小堆堆顶就是最小节点

    // 如果存在定时任务，并且是已经到了触发事件
    if (iter != timer.end() && iter->expire <= HeapTimer::getTick()) {
        iter->func(*iter);  // 执行定时任务
        iter->repeat -= 1;

        if (iter->repeat == 0) {  // 如果重复结束，删除定时器
            delTimer(iter->id);
        }
        else {  // 否则重新定时
            iter->expire += iter->msec;
            siftDown(0, timer.size()- 1);
        }
        return true;
    }
    return false;
}


/** 
 * @description: 检测并找到剩余时间最少的定时器，并获取剩余休眠时间
 */
time_t HeapTimer::timeToSleep() {
    auto iter = timer.begin();
    if (iter == timer.end()) {  // 如果没有定时任务
        return 3000;
    }
    
    time_t diss = iter->expire - HeapTimer::getTick();
    return diss > 0 ? diss : 0;
}


/** 
 * @description: HeapTimer 友元函数，用于输出 HeapTimer 内部属性
 * @param {ostream} &out: 输出流
 * @param {HeapTimer} &timer: HeapTimer 对象
 * @return {*}
 */
std::ostream& operator<<(std::ostream &out, HeapTimer &timer) {
    for (auto iter = timer.timer.begin(); iter != timer.timer.end(); ++iter) {
        out << iter->expire - HeapTimer::getTick() << "/" << iter->id << "  ";
    }
    out << "\n";
    return out;
}


int main() {
    HeapTimer timer;
    timer.addTimer(1000, [](const TimerNode &node) {
        cout << HeapTimer::getTick() << "  node id: " << node.id  << " time: " << 1000 << endl;
    }, 1);
    timer.addTimer(2000, [](const TimerNode &node) {
        cout << HeapTimer::getTick() << "  node id: " << node.id  << " time: " << 2000 << endl;
    }, 2);
    timer.addTimer(3000, [](const TimerNode &node) {
        cout << HeapTimer::getTick() << "  node id: " << node.id  << " time: " << 3000  << endl;
    }, 1);
    timer.addTimer(5000, [](const TimerNode &node) {
        cout << HeapTimer::getTick() << "  node id: " << node.id  << " time: " << 5000  << endl;
    }, 1);

    cout << timer << endl;

    timer.addTimer(500, [](const TimerNode &node) {
        cout << HeapTimer::getTick() << "  node id: " << node.id  << " time: " << 500  << endl;
    }, 2);

    cout << timer << endl;

    timer.delTimer(4);
    cout << timer << endl;

    timer.addTimer(10000, [](const TimerNode &node) {
        g_flag = false;
        cout << HeapTimer::getTick() << "  byebye  node id: " << node.id  << " time: " << 10000  << endl;
    });


    int epfd = epoll_create(1);
    epoll_event ev[64] = {0};

    while (g_flag) {
        // timeout -1 永久阻塞； 0 立即返回； > 0  最多阻塞等待 timeout 毫秒
        // 传递最近触发的定时任务的过期时间
        int n = epoll_wait(epfd, ev, 64, timer.timeToSleep());  // n 被触发的网络事件的个数
        for (int i = 0; i < n; ++i) {
            // 处理网络事件
        }

        // 处理定时时间
        while (timer.checkTimer()) {

        }
    }
    return 0;
}