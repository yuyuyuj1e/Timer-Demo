/** 
 * @author: yuyuyuj1e 807152541@qq.com
 * @github: https://github.com/yuyuyuj1e
 * @csdn: https://blog.csdn.net/yuyuyuj1e
 * @date: 2023-03-31 21:34:09
 * @last_edit_time: 2023-04-02 15:25:33
 * @file_path: /Timer-Demo/timer.cpp
 * @description: 定时器配合网络模块使用的 demo
 */

#include <iostream>
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

    TimerNode(time_t exp, int64_t id, Callback func, int repeat) : func(func), repeat(repeat) {
        this->expire = exp;
        this->id = id;
    }
};


/** 
 * @description: 比较器的比较函数，虽然比较的是 NodeBase，但是在 TimerNode 的比较器中依然可以使用
 */
bool operator< (const NodeBase &lhd, const NodeBase &rhd) {
    // return lhd.expire < rhd.expire;
    if (lhd.expire < rhd.expire) return true;
    else if (lhd.expire > rhd.expire) return false;

    return lhd.id < rhd.id;
}


/** 
 * @description: 定时器类
 */
class Timer {
private:
    set<TimerNode, std::less<>> timer;
    static int64_t g_id;
    static int64_t getID() {
        return g_id++;
    }

public:
    static time_t getTick();
    NodeBase addTimer(time_t msec, TimerNode::Callback func, int repeat = 1);
    bool delTimer(NodeBase &node);
    bool checkTimer();
    time_t timeToSleep();
};

int64_t Timer::g_id = 0;

/** 
 * @description: 获取系统启动后到现在的时间，避免修改了系统时间
 */
time_t Timer::getTick() {
    auto sec = time_point_cast<milliseconds>(steady_clock::now());
    auto tmp = duration_cast<milliseconds>(sec.time_since_epoch());
    return tmp.count();
}


/** 
 * @description: 添加定时器
 * @param {time_t} msec: 定时时间
 * @param {Callback} func: 回调函数
 * @param {int} repeat: 重复次数
 */
NodeBase Timer::addTimer(time_t msec, TimerNode::Callback func, int repeat) {
    time_t expire = Timer::getTick() + msec;

    auto element = timer.emplace(expire, Timer::getID(), func, repeat);
    return *element.first;
}

/** 
 * @description: 删除定时器
 * @param {NodeBase} &node: 需要删除的定时器 
 */
bool Timer::delTimer(NodeBase &node) {
    auto iter = timer.find(node);  // 通过 NodeBase 找到 TimerNode 在 C++14 中允许，可以通过等价 key 找到某个节点
    if (iter != timer.end()) {
        timer.erase(iter);
        return true;
    }
    return false;
}


/** 
 * @description: 检测并找到剩余时间最少的定时器，如果该定时器
 */
bool Timer::checkTimer() {
    auto iter = timer.begin();  // 找到最小的节点
    if (iter != timer.end() && iter->expire <= Timer::getTick()) {
        iter->func(*iter);
        if (iter->repeat != 1) {
            addTimer(1000, [](const TimerNode &node) {
                cout << Timer::getTick() << "  node id: " << node.id << endl;
            }, iter->repeat - 1);
        }
        timer.erase(iter);
        return true;
    }
    return false;
}

/** 
 * @description: 检测并找到剩余时间最少的定时器，并获取剩余休眠时间
 */
time_t Timer::timeToSleep() {
    auto iter = timer.begin();
    if (iter == timer.end()) {  // 如果没有定时任务
        return 3000;
    }
    
    time_t diss = iter->expire - Timer::getTick();
    return diss > 0 ? diss : 0;
}

int main() {
    int epfd = epoll_create(1);
    epoll_event ev[64] = {0};

    unique_ptr<Timer> timer = make_unique<Timer>();

    timer->addTimer(1000, [](const TimerNode &node) {
        cout << Timer::getTick() << "  node id: " << node.id << endl;
    }, 3);

    timer->addTimer(1000, [](const TimerNode &node) {
        cout << Timer::getTick() << "  node id: " << node.id << endl;
    });

    auto node = timer->addTimer(3000, [](const TimerNode &node) {
        cout << Timer::getTick() << "  node id: " << node.id << endl;
    });
    timer->delTimer(node);

    timer->addTimer(5000, [](const TimerNode &node) {
        g_flag = false;
        cout << Timer::getTick() << "  byebye  node id: " << node.id << endl;
    });


    while (g_flag) {
        // timeout -1 永久阻塞； 0 立即返回； > 0  最多阻塞等待 timeout 毫秒
        // 传递最近触发的定时任务的过期时间
        int n = epoll_wait(epfd, ev, 64, timer->timeToSleep());  // n 被触发的网络事件的个数
        for (int i = 0; i < n; ++i) {
            // 处理网络事件
        }

        // 处理定时时间
        while (timer->checkTimer()) {

        }
    }

    return 0;
}
