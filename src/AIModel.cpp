#include "AIModel.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <unordered_set>

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
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return;
    }

    nodes_.clear();
    edges_.clear();
    allPorts_.clear();
    portIndex_.clear();
    nextPortId_ = 1000;
    nextEdgeId_ = 2000;

    std::string line;
    bool parsingNodes = false;
    bool parsingConnections = false;

    while (std::getline(file, line)) {
        // Trim whitespace
        if (line.empty() || line[0] == '\n') continue;

        if (line.find("Nodes:") != std::string::npos) {
            parsingNodes = true;
            parsingConnections = false;
            continue;
        }
        if (line.find("Connections:") != std::string::npos) {
            parsingNodes = false;
            parsingConnections = true;
            continue;
        }

        if (parsingNodes) {
            // Parse: nodeId,nodeType,nodeName
            int nodeId;
            std::string nodeType, nodeName;
            char comma;
            std::istringstream iss(line);
            iss >> nodeId >> comma >> nodeType >> comma;
            std::getline(iss, nodeName);

            if (nodeName.empty()) continue;

            AINode node;
            node.id = nodeId;
            node.type = nodeType;
            node.name = nodeName;
            node.boundUINodeId = -1;

            // Create default input/output ports for the node
            Port inputPort;
            inputPort.id = GetNextPortId();
            inputPort.name = "input";
            inputPort.dataType = "any";
            inputPort.isInput = true;
            inputPort.nodeId = nodeId;
            node.inputPorts.push_back(inputPort);
            allPorts_.push_back(inputPort);
            portIndex_[inputPort.id] = allPorts_.size() - 1;

            Port outputPort;
            outputPort.id = GetNextPortId();
            outputPort.name = "output";
            outputPort.dataType = "any";
            outputPort.isInput = false;
            outputPort.nodeId = nodeId;
            node.outputPorts.push_back(outputPort);
            allPorts_.push_back(outputPort);
            portIndex_[outputPort.id] = allPorts_.size() - 1;

            nodes_.push_back(node);
        }
        else if (parsingConnections) {
            // Parse: fromNodeId,toNodeId,fromPortIndex,toPortIndex
            int fromNodeId, toNodeId, fromPortIdx, toPortIdx;
            char comma;
            std::istringstream iss(line);
            iss >> fromNodeId >> comma >> toNodeId >> comma >> fromPortIdx >> comma >> toPortIdx;

            // Find the ports
            const Port* fromPort = nullptr;
            const Port* toPort = nullptr;

            // Find nodes
            AINode* fromNode = nullptr;
            AINode* toNode = nullptr;
            for (auto& node : nodes_) {
                if (node.id == fromNodeId) fromNode = &node;
                if (node.id == toNodeId) toNode = &node;
            }

            if (!fromNode || !toNode) continue;
            
            // Get the output port from source node
            if (fromPortIdx < fromNode->outputPorts.size()) {
                fromPort = &fromNode->outputPorts[fromPortIdx];
            }
            
            // Get the input port from target node
            if (toPortIdx < toNode->inputPorts.size()) {
                toPort = &toNode->inputPorts[toPortIdx];
            }

            if (fromPort && toPort) {
                Edge edge;
                edge.id = GetNextEdgeId();
                edge.fromPortId = fromPort->id;
                edge.toPortId = toPort->id;
                edge.dataType = "any"; // Default data type
                edges_.push_back(edge);
            }
        }
    }

    file.close();
    if (onModelChange_) onModelChange_();
}

void AIModel::SaveToFile(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return;
    }

    // Save nodes
    file << "Nodes:\n";
    for (const auto& node : nodes_) {
        file << node.id << "," << node.type << "," << node.name << "\n";
    }

    // Save edges (connections)
    file << "Connections:\n";
    for (const auto& edge : edges_) {
        // Find port indices for backward compatibility
        const Port* fromPort = GetPort(edge.fromPortId);
        const Port* toPort = GetPort(edge.toPortId);
        
        if (fromPort && toPort) {
            // Find the node and port index
            int fromPortIdx = 0, toPortIdx = 0;
            
            for (const auto& node : nodes_) {
                if (node.id == fromPort->nodeId) {
                    for (size_t i = 0; i < node.outputPorts.size(); i++) {
                        if (node.outputPorts[i].id == fromPort->id) {
                            fromPortIdx = i;
                            break;
                        }
                    }
                }
                if (node.id == toPort->nodeId) {
                    for (size_t i = 0; i < node.inputPorts.size(); i++) {
                        if (node.inputPorts[i].id == toPort->id) {
                            toPortIdx = i;
                            break;
                        }
                    }
                }
            }
            
            file << fromPort->nodeId << "," << toPort->nodeId << "," 
                 << fromPortIdx << "," << toPortIdx << "\n";
        }
    }
    
    file.close();
}

void AIModel::AddNode(const AINode& node) {
    AINode newNode = node;
    
    // If the node doesn't have ports, create default ones
    if (newNode.inputPorts.empty()) {
        Port inputPort;
        inputPort.id = GetNextPortId();
        inputPort.name = "input";
        inputPort.dataType = "any";
        inputPort.isInput = true;
        inputPort.nodeId = newNode.id;
        newNode.inputPorts.push_back(inputPort);
        allPorts_.push_back(inputPort);
        portIndex_[inputPort.id] = allPorts_.size() - 1;
    } else {
        // Register existing ports
        for (auto& port : newNode.inputPorts) {
            if (port.id <= 0) port.id = GetNextPortId();
            port.nodeId = newNode.id;
            allPorts_.push_back(port);
            portIndex_[port.id] = allPorts_.size() - 1;
        }
    }
    
    if (newNode.outputPorts.empty()) {
        Port outputPort;
        outputPort.id = GetNextPortId();
        outputPort.name = "output";
        outputPort.dataType = "any";
        outputPort.isInput = false;
        outputPort.nodeId = newNode.id;
        newNode.outputPorts.push_back(outputPort);
        allPorts_.push_back(outputPort);
        portIndex_[outputPort.id] = allPorts_.size() - 1;
    } else {
        // Register existing ports
        for (auto& port : newNode.outputPorts) {
            if (port.id <= 0) port.id = GetNextPortId();
            port.nodeId = newNode.id;
            allPorts_.push_back(port);
            portIndex_[port.id] = allPorts_.size() - 1;
        }
    }
    
    nodes_.push_back(newNode);
    if (onModelChange_) onModelChange_();
}

void AIModel::RemoveNode(int nodeId) {
    // Remove all edges connected to this node
    edges_.erase(std::remove_if(edges_.begin(), edges_.end(),
        [this, nodeId](const Edge& edge) {
            const Port* fromPort = GetPort(edge.fromPortId);
            const Port* toPort = GetPort(edge.toPortId);
            return (fromPort && fromPort->nodeId == nodeId) || (toPort && toPort->nodeId == nodeId);
        }), edges_.end());

    // Remove all ports belonging to this node
    allPorts_.erase(std::remove_if(allPorts_.begin(), allPorts_.end(),
        [nodeId](const Port& port) { return port.nodeId == nodeId; }), allPorts_.end());

    // Rebuild port index
    portIndex_.clear();
    for (size_t i = 0; i < allPorts_.size(); i++) {
        portIndex_[allPorts_[i].id] = i;
    }

    // Remove the node itself
    nodes_.erase(std::remove_if(nodes_.begin(), nodes_.end(),
        [nodeId](const AINode& node) { return node.id == nodeId; }), nodes_.end());

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

// New Edge-based connection methods
void AIModel::AddEdge(const Edge& edge) {
    if (!ValidateEdge(edge)) {
        std::cerr << "AIModel::AddEdge: Invalid edge configuration" << std::endl;
        return;
    }
    Edge newEdge = edge;
    if (newEdge.id <= 0) newEdge.id = GetNextEdgeId();
    edges_.push_back(newEdge);
    if (onModelChange_) onModelChange_();
}

void AIModel::RemoveEdge(int edgeId) {
    edges_.erase(std::remove_if(edges_.begin(), edges_.end(),
        [edgeId](const Edge& edge) { return edge.id == edgeId; }), edges_.end());
    if (onModelChange_) onModelChange_();
}

void AIModel::RemoveEdgesBetweenNodes(int fromNodeId, int toNodeId) {
    edges_.erase(std::remove_if(edges_.begin(), edges_.end(),
        [this, fromNodeId, toNodeId](const Edge& edge) {
            const Port* fromPort = GetPort(edge.fromPortId);
            const Port* toPort = GetPort(edge.toPortId);
            return (fromPort && fromPort->nodeId == fromNodeId &&
                    toPort && toPort->nodeId == toNodeId);
        }), edges_.end());
    if (onModelChange_) onModelChange_();
}

// Port lookup methods
const Port* AIModel::GetPort(int portId) const {
    auto it = portIndex_.find(portId);
    if (it != portIndex_.end()) {
        int idx = it->second;
        if (idx >= 0 && idx < static_cast<int>(allPorts_.size())) {
            return &allPorts_[idx];
        }
    }
    return nullptr;
}

std::vector<const Port*> AIModel::GetNodeInputPorts(int nodeId) const {
    std::vector<const Port*> result;
    for (const auto& port : allPorts_) {
        if (port.nodeId == nodeId && port.isInput) {
            result.push_back(&port);
        }
    }
    return result;
}

std::vector<const Port*> AIModel::GetNodeOutputPorts(int nodeId) const {
    std::vector<const Port*> result;
    for (const auto& port : allPorts_) {
        if (port.nodeId == nodeId && !port.isInput) {
            result.push_back(&port);
        }
    }
    return result;
}

// Edge validation
bool AIModel::ValidateEdge(const Edge& edge) const {
    const Port* fromPort = GetPort(edge.fromPortId);
    const Port* toPort = GetPort(edge.toPortId);
    
    if (!fromPort || !toPort) {
        std::cerr << "ValidateEdge: Port not found" << std::endl;
        return false;
    }
    
    // fromPort must be output, toPort must be input
    if (fromPort->isInput || !toPort->isInput) {
        std::cerr << "ValidateEdge: Invalid port directions" << std::endl;
        return false;
    }
    
    // Cannot connect port to itself
    if (fromPort->nodeId == toPort->nodeId) {
        std::cerr << "ValidateEdge: Cannot connect port to same node" << std::endl;
        return false;
    }
    
    // Data types should match (if both are not "any")
    if (fromPort->dataType != "any" && toPort->dataType != "any" &&
        fromPort->dataType != toPort->dataType) {
        std::cerr << "ValidateEdge: Data type mismatch: "
                  << fromPort->dataType << " != " << toPort->dataType << std::endl;
        return false;
    }
    
    return true;
}

// Backward compatibility: return edges in old tuple format
std::vector<std::tuple<int, int, int, int>> AIModel::GetConnectionsLegacy() const {
    std::vector<std::tuple<int, int, int, int>> result;
    for (const auto& edge : edges_) {
        const Port* fromPort = GetPort(edge.fromPortId);
        const Port* toPort = GetPort(edge.toPortId);
        
        if (fromPort && toPort) {
            // Find port indices
            int fromIdx = 0, toIdx = 0;
            
            for (const auto& node : nodes_) {
                if (node.id == fromPort->nodeId) {
                    for (size_t i = 0; i < node.outputPorts.size(); i++) {
                        if (node.outputPorts[i].id == fromPort->id) {
                            fromIdx = i;
                            break;
                        }
                    }
                }
                if (node.id == toPort->nodeId) {
                    for (size_t i = 0; i < node.inputPorts.size(); i++) {
                        if (node.inputPorts[i].id == toPort->id) {
                            toIdx = i;
                            break;
                        }
                    }
                }
            }
            
            result.emplace_back(fromPort->nodeId, toPort->nodeId, fromIdx, toIdx);
        }
    }
    return result;
}

// Legacy connection methods for backward compatibility
void AIModel::AddConnection(int fromNode, int toNode, int fromOutput, int toInput) {
    // Find the ports
    AINode* fromNodePtr = nullptr;
    AINode* toNodePtr = nullptr;
    for (auto& node : nodes_) {
        if (node.id == fromNode) fromNodePtr = &node;
        if (node.id == toNode) toNodePtr = &node;
    }
    
    if (!fromNodePtr || !toNodePtr) return;
    if (fromOutput >= static_cast<int>(fromNodePtr->outputPorts.size())) return;
    if (toInput >= static_cast<int>(toNodePtr->inputPorts.size())) return;
    
    Edge edge;
    edge.id = GetNextEdgeId();
    edge.fromPortId = fromNodePtr->outputPorts[fromOutput].id;
    edge.toPortId = toNodePtr->inputPorts[toInput].id;
    edge.dataType = "any";
    AddEdge(edge);
}

void AIModel::RemoveConnection(int fromNode, int toNode) {
    RemoveEdgesBetweenNodes(fromNode, toNode);
}

void AIModel::StartExecution(int numThreads) {
    // If there are leftover worker threads from a previous run, join them first
    if (!workerThreads_.empty()) {
        for (auto& t : workerThreads_) {
            if (t.joinable()) {
                t.join();
            }
        }
        workerThreads_.clear();
    }

    if (executing_) return;

    numThreads_ = numThreads;
    executing_ = true;

    // Build dependency graph (adjacency_ and indegree_)
    adjacency_.clear();
    indegree_.clear();

    // Initialize indegree for all nodes
    for (const auto& node : nodes_) {
        indegree_[node.id] = 0;
    }

    // Build graph from edges
    for (const auto& edge : edges_) {
        const Port* fromPort = GetPort(edge.fromPortId);
        const Port* toPort = GetPort(edge.toPortId);
        
        if (fromPort && toPort) {
            int from = fromPort->nodeId;
            int to = toPort->nodeId;
            adjacency_[from].push_back(to);
            // ensure keys exist
            if (indegree_.find(to) == indegree_.end()) indegree_[to] = 0;
            if (indegree_.find(from) == indegree_.end()) indegree_[from] = 0;
            indegree_[to]++;
        }
    }

    // Initialize ready queue with nodes that have indegree == 0
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        while (!readyQueue_.empty()) readyQueue_.pop();
        for (const auto& p : indegree_) {
            if (p.second == 0) readyQueue_.push(p.first);
        }
    }

    // If no ready nodes but there are nodes, there may be a cycle -> abort execution
    if (nodes_.size() > 0) {
        std::lock_guard<std::mutex> lock(queueMutex_);
        if (readyQueue_.empty()) {
            std::cerr << "AIModel::StartExecution: no entry nodes (possible cycle). Aborting execution." << std::endl;
            executing_ = false;
            return;
        }
    }

    remainingNodes_.store(static_cast<int>(nodes_.size()));

    // Start worker threads
    for (int i = 0; i < numThreads_; ++i) {
        workerThreads_.emplace_back(&AIModel::ExecutionLoop, this);
    }

    std::cout << "Started AI model execution with " << numThreads_ << " threads" << std::endl;
}

void AIModel::StopExecution() {
    // Always join leftover threads regardless of executing_ state.
    // If execution finished naturally, threads may be lingering.
    if (workerThreads_.empty()) {
        return;  // No threads to join
    }

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

    // Clear scheduling structures
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        while (!readyQueue_.empty()) readyQueue_.pop();
    }
    adjacency_.clear();
    indegree_.clear();
    remainingNodes_.store(0);

    std::cout << "Stopped AI model execution" << std::endl;
}

void AIModel::ExecutionLoop() {
    while (executing_) {
        int nodeId = -1;

        // Get next ready node to execute
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCondition_.wait(lock, [this]() {
                return !executing_ || !readyQueue_.empty();
            });

            if (!executing_) break;

            if (!readyQueue_.empty()) {
                nodeId = readyQueue_.front();
                readyQueue_.pop();
            }
        }

        if (nodeId != -1) {
            ExecuteNode(nodeId);

            // After executing, mark successors and push newly ready nodes
            std::lock_guard<std::mutex> lock(queueMutex_);
            auto it = adjacency_.find(nodeId);
            if (it != adjacency_.end()) {
                for (int succ : it->second) {
                    auto indegIt = indegree_.find(succ);
                    if (indegIt != indegree_.end()) {
                        indegIt->second--;
                        if (indegIt->second == 0) {
                            readyQueue_.push(succ);
                            queueCondition_.notify_one();
                        }
                    }
                }
            }

            remainingNodes_.fetch_sub(1);

            // If we've finished all nodes, stop execution
            if (remainingNodes_.load() <= 0) {
                executing_ = false;
                queueCondition_.notify_all();
            }
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
