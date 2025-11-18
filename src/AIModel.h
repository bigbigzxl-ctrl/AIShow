#pragma once

#include <vector>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <unordered_map>
#include <map>
#include <tuple>

// Port represents an input/output connector on a node
struct Port {
    int id;                        // Unique port ID
    std::string name;              // Port name (e.g., "input_text", "output_vector")
    std::string dataType;          // Data type (e.g., "string", "vector", "int", "float")
    bool isInput;                  // true for input port, false for output port
    int nodeId;                    // ID of the node this port belongs to
};

// Edge represents a connection between two ports
struct Edge {
    int id;                        // Unique edge ID
    int fromPortId;                // Source port ID
    int toPortId;                  // Target port ID
    std::string dataType;          // Data type being transmitted (for validation)
    std::map<std::string, std::string> metadata; // Edge metadata (weights, conditions, etc.)
};

struct AINode {
    int id;
    std::string type;
    std::string name;
    std::vector<std::pair<std::string, std::string>> parameters;
    std::vector<Port> inputPorts;  // Input ports (replacing simple inputs vector)
    std::vector<Port> outputPorts; // Output ports (replacing simple outputs vector)
    int boundUINodeId;             // ID of the bound UI node
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

    // Non-copyable and non-movable to avoid accidental copying of threads/queues
    AIModel(const AIModel&) = delete;
    AIModel& operator=(const AIModel&) = delete;
    AIModel(AIModel&&) = delete;
    AIModel& operator=(AIModel&&) = delete;

    void LoadFromFile(const std::string& filename);
    void SaveToFile(const std::string& filename);
    void AddNode(const AINode& node);
    void RemoveNode(int nodeId);
    void UpdateNode(const AINode& node);
    void AddEdge(const Edge& edge);
    void RemoveEdge(int edgeId);
    void RemoveEdgesBetweenNodes(int fromNodeId, int toNodeId);

    const std::vector<AINode>& GetNodes() const { return nodes_; }
    const std::vector<Edge>& GetEdges() const { return edges_; }
    const std::vector<Port>& GetAllPorts() const { return allPorts_; }
    
    // Helper methods for port lookup
    const Port* GetPort(int portId) const;
    std::vector<const Port*> GetNodeInputPorts(int nodeId) const;
    std::vector<const Port*> GetNodeOutputPorts(int nodeId) const;
    
    // Validate connection compatibility
    bool ValidateEdge(const Edge& edge) const;

    // Backward compatibility: return edges in old tuple format for UI
    std::vector<std::tuple<int, int, int, int>> GetConnectionsLegacy() const;
    
    // Connection methods (legacy interface for backward compatibility)
    void AddConnection(int fromNode, int toNode, int fromOutput, int toInput);
    void RemoveConnection(int fromNode, int toNode);

    void SetModelChangeCallback(std::function<void()> callback) { onModelChange_ = callback; }

    // Execution methods
    void StartExecution(int numThreads = 1);
    void StopExecution();
    bool IsExecuting() const { return executing_.load(); }

    void SetProgressCallback(std::function<void(const ExecutionProgress&)> callback) {
        progressCallback_ = callback;
    }

    void SetExecutionConfig(int numThreads) { numThreads_ = numThreads; }

private:
    void ExecutionLoop();
    void ExecuteNode(int nodeId);
    void ReportProgress(int nodeId, float progress, const std::string& status, const std::string& message = "");

    std::vector<AINode> nodes_;
    std::vector<Edge> edges_;      // New edge-based connection storage
    std::vector<Port> allPorts_;   // All ports indexed by ID for quick lookup
    std::unordered_map<int, int> portIndex_; // Map portId to index in allPorts_
    
    std::function<void()> onModelChange_;

    // Execution related
    std::atomic<bool> executing_{false};
    std::vector<std::thread> workerThreads_;
    // ready queue holds nodes whose dependencies have been satisfied
    std::queue<int> readyQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCondition_;
    int numThreads_{1};

    // Dependency graph for topological scheduling
    std::unordered_map<int, std::vector<int>> adjacency_; // from -> list of to
    std::unordered_map<int, int> indegree_; // node -> remaining incoming edges
    std::atomic<int> remainingNodes_{0};

    std::function<void(const ExecutionProgress&)> progressCallback_;
    
    // Helper to assign unique IDs
    int nextPortId_{1000};
    int nextEdgeId_{2000};
    int GetNextPortId() { return nextPortId_++; }
    int GetNextEdgeId() { return nextEdgeId_++; }
};
