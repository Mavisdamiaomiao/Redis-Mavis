#pragma once
#include <thread>
#include <functional>

// future_task_dispatcher.cpp
void dispatch_client(int client_fd) {
    // 现在是直接开线程
    std::thread t(handle_client, client_fd);
    t.detach();

    // 将来只需要改成：
    // thread_pool.add_task([client_fd]() { handle_client(client_fd); });
}
