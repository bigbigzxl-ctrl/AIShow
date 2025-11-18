#include "AIModel.h"
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <chrono>

int main() {
    AIModel model;

    // Create a simple chain DAG: 1 -> 2 -> 3
    AINode n1{1, "Conv2D", "Node1", {}, {}, {}, -1};
    AINode n2{2, "MaxPool", "Node2", {}, {}, {}, -1};
    AINode n3{3, "Generic", "Node3", {}, {}, {}, -1};

    model.AddNode(n1);
    model.AddNode(n2);
    model.AddNode(n3);

    model.AddConnection(1, 2, 0, 0);
    model.AddConnection(2, 3, 0, 0);

    std::mutex m;
    std::condition_variable cv;
    int completed = 0;
    const int total = 3;
    bool ok = true;

    model.SetProgressCallback([&](const ExecutionProgress& p) {
        std::cout << "Progress: node=" << p.nodeId << " status=" << p.status << " progress=" << p.progress << " msg=" << p.message << std::endl;
        if (p.status == "completed") {
            {
                std::lock_guard<std::mutex> lk(m);
                completed++;
            }
            cv.notify_one();
        }
    });

    // First run
    std::cout << "Test: starting first run" << std::endl;
    model.StartExecution(2);

    {
        std::unique_lock<std::mutex> lk(m);
        if (!cv.wait_for(lk, std::chrono::seconds(20), [&]{ return completed == total; })) {
            std::cerr << "Timeout waiting for first run completion" << std::endl;
            ok = false;
        }
    }

    model.StopExecution();
    std::cout << "Test: first run finished, completed=" << completed << std::endl;

    // Second run
    completed = 0;
    std::cout << "Test: starting second run" << std::endl;
    model.StartExecution(2);

    {
        std::unique_lock<std::mutex> lk(m);
        if (!cv.wait_for(lk, std::chrono::seconds(20), [&]{ return completed == total; })) {
            std::cerr << "Timeout waiting for second run completion" << std::endl;
            ok = false;
        }
    }

    model.StopExecution();
    std::cout << "Test: second run finished, completed=" << completed << std::endl;

    if (ok) {
        std::cout << "ai_execution_test: PASS" << std::endl;
        return 0;
    } else {
        std::cout << "ai_execution_test: FAIL" << std::endl;
        return 1;
    }
}
