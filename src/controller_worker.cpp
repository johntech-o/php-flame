#include "coroutine.h"
#include "controller_worker.h"
#include "controller.h"

namespace flame {
    
    controller_worker::controller_worker() {

    }

    void controller_worker::initialize(const php::array &options) {

    }

    void controller_worker::await_signal() {
        signal_->async_wait([this] (const boost::system::error_code &error, int sig) {
            // 主进程与子进程 signal 处理之间有干扰
            if (error) return; 
            if (gcontroller->worker_size == 0) signal_.reset(); // 单进程模式
            else await_signal(); // 多进程模式的工作进程需要持续监听
            
            if(sig == SIGTERM || sig == SIGINT) { // 工作进程仅处理退出信号
                boost::asio::post(gcontroller->context_x, [this] () {
                    gcontroller->status |= controller::controller_status::STATUS_SHUTDOWN;
                    auto ft = gcontroller->cbmap->equal_range("quit"); // 需要在协程调用退出回调
                    for (auto i = ft.first; i != ft.second; ++i) coroutine::start(i->second);
                });
            }
            else if(sig == SIGUSR1) {
                if(gcontroller->status & controller::controller_status::STATUS_CLOSECONN) 
                    gcontroller->status &= ~controller::controller_status::STATUS_CLOSECONN;
                else
                    gcontroller->status |= controller::controller_status::STATUS_CLOSECONN;
            }
        });
    }

    void controller_worker::run() {
        // 工作线程的使用是随机的, 需要保持其一直存在
        auto work = boost::asio::make_work_guard(gcontroller->context_y);
        // 1. 停止信号调用退出通知
        signal_.reset(new boost::asio::signal_set(gcontroller->context_y, SIGINT, SIGTERM));
        signal_->add(SIGUSR1);
        signal_->add(SIGUSR2);
        await_signal();

        // 子进程的启动过程:
        // 2. 启动线程池, 并使用线程池运行 context_y
        thread_.resize(4);
        for (int i = 0; i < thread_.size(); ++i) {
            thread_[i] = new std::thread([this] {
                gcontroller->context_y.run();
            });
        }
        // 3. 启动 context_x 运行
        gcontroller->context_x.run();
        // 4. 确认工作线程停止
        if (signal_) signal_.reset();
        work.reset();
        for (int i = 0; i < thread_.size(); ++i) {
            thread_[i]->join();
            delete thread_[i];
        }
    }
}
