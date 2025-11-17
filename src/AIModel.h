#pragma once

#include <vector>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>

struct AINode {
    int id;
    std::string type;
    std::string name;
    std::vector<std::pair<std::string, std::string>> parameters;
    std::vector<int> inputs;
    std::vector<int> outputs;
    int boundUINodeId; // ID of the bound UI node
};

struct ExecutionProgress {
    int nodeId;
    std::string nodeName;
    float progress; // 0.0 to 1.0
    std::string status; // "running", "completed", "failed"
    std::string message;
};

class AIModel {
public:
    AIModel();
    ~AIModel();

    void LoadFromFile(const std::string& filename);
    void SaveToFile(const std::string& filename);
    void AddNode(const AINode& node);
    void RemoveNode(int nodeId);
    void UpdateNode(const AINode& node);
    void AddConnection(int fromNode, int toNode, int fromOutput, int toInput);
    void RemoveConnection(int fromNode, int toNode);

    const std::vector<AINode>& GetNodes() const { return nodes_; }
    const std::vector<std::tuple<int, int, int, int>>& GetConnections() const { return connections_; }

    void SetModelChangeCallback(std::function<void()> callback) { onModelChange_ = callback; }

    // Execution methods
    void StartExecution(int numThreads = 1);
    void StopExecution();
    bool IsExecuting() const { return executing_; }

    void SetProgressCallback(std::function<void(const ExecutionProgress&)> callback) {
        progressCallback_ = callback;
    }

    void SetExecutionConfig(int numThreads) { numThreads_ = numThreads; }

private:
    void ExecutionLoop();
    void ExecuteNode(int nodeId);
    void ReportProgress(int nodeId, float progress, const std::string& status, const std::string& message = "");

    std::vector<AINode> nodes_;
    std::vector<std::tuple<int, int, int, int>> connections_; // fromNode, toNode, fromOutput, toInput
    std::function<void()> onModelChange_;

    // Execution related
    std::atomic<bool> executing_;
    std::vector<std::thread> workerThreads_;
    std::queue<int> executionQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCondition_;
    int numThreads_;

    std::function<void(const ExecutionProgress&)> progressCallback_;
};
