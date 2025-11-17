#include "AIModel.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <chrono>

AIModel::AIModel()
    : onModelChange_(nullptr),
      executing_(false),
      numThreads_(1),
      progressCallback_(nullptr) {
}

AIModel::~AIModel() {
    StopExecution();
}

void AIModel::LoadFromFile(const std::string& filename) {
    // Simple JSON-like loading (simplified)
    // std::ifstream file(filename);
    // if (!file.is_open()) {
    //     std::cerr << "Failed to open file: " << filename << std::endl;
    //     return;
    // }

    // For simplicity, assume a basic format
    // In real implementation, use JSON parser
    nodes_.clear();
    connections_.clear();

    // Dummy data for demonstration
    AINode node1 = {1, "Conv2D", "Convolution Layer", {{"filters", "32"}, {"kernel_size", "3"}}, {0}, {1}, -1};
    AINode node2 = {2, "MaxPool", "Max Pooling", {{"pool_size", "2"}}, {1}, {2}, -1};

    nodes_.push_back(node1);
    nodes_.push_back(node2);
    connections_.emplace_back(1, 2, 1, 1);

    if (onModelChange_) onModelChange_();
}

void AIModel::SaveToFile(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return;
    }

    // Simple saving
    file << "Nodes:\n";
    for (const auto& node : nodes_) {
        file << node.id << "," << node.type << "," << node.name << "\n";
    }
    file << "Connections:\n";
    for (const auto& conn : connections_) {
        file << std::get<0>(conn) << "," << std::get<1>(conn) << "," << std::get<2>(conn) << "," << std::get<3>(conn) << "\n";
    }
}

void AIModel::AddNode(const AINode& node) {
    nodes_.push_back(node);
    if (onModelChange_) onModelChange_();
}

void AIModel::RemoveNode(int nodeId) {
    nodes_.erase(std::remove_if(nodes_.begin(), nodes_.end(),
        [nodeId](const AINode& node) { return node.id == nodeId; }), nodes_.end());
    connections_.erase(std::remove_if(connections_.begin(), connections_.end(),
        [nodeId](const std::tuple<int, int, int, int>& conn) { return std::get<0>(conn) == nodeId || std::get<1>(conn) == nodeId; }), connections_.end());
    if (onModelChange_) onModelChange_();
}

void AIModel::UpdateNode(const AINode& updatedNode) {
    for (auto& node : nodes_) {
        if (node.id == updatedNode.id) {
            node = updatedNode;
            break;
        }
    }
    if (onModelChange_) onModelChange_();
}

void AIModel::AddConnection(int fromNode, int toNode, int fromOutput, int toInput) {
    connections_.emplace_back(fromNode, toNode, fromOutput, toInput);
    if (onModelChange_) onModelChange_();
}

void AIModel::RemoveConnection(int fromNode, int toNode) {
    connections_.erase(std::remove_if(connections_.begin(), connections_.end(),
        [fromNode, toNode](const std::tuple<int, int, int, int>& conn) { return std::get<0>(conn) == fromNode && std::get<1>(conn) == toNode; }), connections_.end());
    if (onModelChange_) onModelChange_();
}

void AIModel::StartExecution(int numThreads) {
    if (executing_) return;

    numThreads_ = numThreads;
    executing_ = true;

    // Initialize execution queue with all nodes
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        for (const auto& node : nodes_) {
            executionQueue_.push(node.id);
        }
    }

    // Start worker threads
    for (int i = 0; i < numThreads_; ++i) {
        workerThreads_.emplace_back(&AIModel::ExecutionLoop, this);
    }

    std::cout << "Started AI model execution with " << numThreads_ << " threads" << std::endl;
}

void AIModel::StopExecution() {
    if (!executing_) return;

    executing_ = false;

    // Wake up all waiting threads
    queueCondition_.notify_all();

    // Wait for all threads to finish
    for (auto& thread : workerThreads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    workerThreads_.clear();

    std::cout << "Stopped AI model execution" << std::endl;
}

void AIModel::ExecutionLoop() {
    while (executing_) {
        int nodeId = -1;

        // Get next node to execute
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCondition_.wait(lock, [this]() {
                return !executing_ || !executionQueue_.empty();
            });

            if (!executing_) break;

            if (!executionQueue_.empty()) {
                nodeId = executionQueue_.front();
                executionQueue_.pop();
            }
        }

        if (nodeId != -1) {
            ExecuteNode(nodeId);
        }
    }
}

void AIModel::ExecuteNode(int nodeId) {
    // Find the node
    auto it = std::find_if(nodes_.begin(), nodes_.end(),
        [nodeId](const AINode& node) { return node.id == nodeId; });

    if (it == nodes_.end()) return;

    const AINode& node = *it;

    // Report start
    ReportProgress(nodeId, 0.0f, "running", "Starting execution");

    // Simulate execution based on node type
    if (node.type == "Conv2D") {
        // Simulate convolution operation
        for (int i = 1; i <= 10; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            ReportProgress(nodeId, i * 0.1f, "running", "Processing convolution layer");
        }
    } else if (node.type == "MaxPool") {
        // Simulate pooling operation
        for (int i = 1; i <= 5; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            ReportProgress(nodeId, i * 0.2f, "running", "Processing pooling layer");
        }
    } else {
        // Generic processing
        for (int i = 1; i <= 8; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(75));
            ReportProgress(nodeId, i * 0.125f, "running", "Processing " + node.type);
        }
    }

    // Report completion
    ReportProgress(nodeId, 1.0f, "completed", "Execution completed successfully");
}

void AIModel::ReportProgress(int nodeId, float progress, const std::string& status, const std::string& message) {
    if (progressCallback_) {
        // Find node name
        std::string nodeName = "Unknown";
        auto it = std::find_if(nodes_.begin(), nodes_.end(),
            [nodeId](const AINode& node) { return node.id == nodeId; });
        if (it != nodes_.end()) {
            nodeName = it->name;
        }

        ExecutionProgress progressInfo = {nodeId, nodeName, progress, status, message};
        progressCallback_(progressInfo);
    }
}
