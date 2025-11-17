#include "SyncManager.h"
#include <iostream>
#include <chrono>
#include <map>

SyncManager::SyncManager(NodeEditor* editor, AIModel* model)
    : editor_(editor), model_(model), running_(false), editorChanged_(false), modelChanged_(false), executionProgressCallback_(nullptr) {
}

SyncManager::~SyncManager() {
    StopSync();
}

void SyncManager::StartSync() {
    if (running_) return;

    running_ = true;

    // Set up callbacks
    editor_->SetNodeChangeCallback([this]() {
        std::lock_guard<std::mutex> lock(mutex_);
        editorChanged_ = true;
    });

    model_->SetModelChangeCallback([this]() {
        std::lock_guard<std::mutex> lock(mutex_);
        modelChanged_ = true;
    });

    // Start sync thread
    syncThread_ = std::thread(&SyncManager::SyncLoop, this);
}

void SyncManager::StopSync() {
    if (!running_) return;

    running_ = false;

    if (syncThread_.joinable()) {
        syncThread_.join();
    }
}

void SyncManager::SyncEditorToModel() {
    std::lock_guard<std::mutex> lock(mutex_);

    // Temporarily disable model callback to avoid recursive sync
    model_->SetModelChangeCallback(nullptr);

    // Clear model and rebuild
    const auto& currentNodes = model_->GetNodes();
    std::vector<int> nodeIds;
    for (const auto& node : currentNodes) {
        nodeIds.push_back(node.id);
    }
    for (int id : nodeIds) {
        model_->RemoveNode(id);
    }

    // Sync nodes from editor to model
    const auto& editorNodes = editor_->GetNodes();
    for (const auto& editorNode : editorNodes) {
        AINode modelNode;
        modelNode.id = editorNode.id;
        modelNode.name = editorNode.name;
        modelNode.type = "Generic"; // Default type, could be enhanced
        modelNode.inputs = editorNode.inputs;
        modelNode.outputs = editorNode.outputs;
        modelNode.boundUINodeId = editorNode.id; // Bind to UI node
        // Add default parameters
        modelNode.parameters = {{"position_x", std::to_string(editorNode.positionX)},
                               {"position_y", std::to_string(editorNode.positionY)}};

        model_->AddNode(modelNode);
    }

    // Sync links from editor to model
    const auto& editorLinks = editor_->GetLinks();
    for (const auto& editorLink : editorLinks) {
        model_->AddConnection(editorLink.start_node, editorLink.end_node,
                            editorLink.start_attr, editorLink.end_attr);
    }

    // Re-enable callback
    model_->SetModelChangeCallback([this]() {
        std::lock_guard<std::mutex> lock(mutex_);
        modelChanged_ = true;
    });

    editorChanged_ = false;
}

void SyncManager::SyncModelToEditor() {
    std::lock_guard<std::mutex> lock(mutex_);

    // Temporarily disable editor callback to avoid recursive sync
    editor_->SetNodeChangeCallback(nullptr);

    // Clear editor
    const auto& editorNodes = editor_->GetNodes();
    for (const auto& node : editorNodes) {
        editor_->RemoveNode(node.id);
    }

    // Sync nodes from model to editor
    const auto& modelNodes = model_->GetNodes();

    // Create mapping from AI node ID to UI node ID
    std::map<int, int> aiToUiNodeMap;

    for (const auto& modelNode : modelNodes) {
        // Extract position from parameters if available
        float posX = 100.0f + (modelNode.id - 1) * 150.0f; // Default spacing
        float posY = 100.0f;

        for (const auto& param : modelNode.parameters) {
            if (param.first == "position_x") {
                posX = std::stof(param.second);
            } else if (param.first == "position_y") {
                posY = std::stof(param.second);
            }
        }

        editor_->AddNode(modelNode.name, posX, posY, modelNode.id);

        // Get the newly added UI node ID
        const auto& uiNodes = editor_->GetNodes();
        if (!uiNodes.empty()) {
            int uiNodeId = uiNodes.back().id;
            aiToUiNodeMap[modelNode.id] = uiNodeId;
        }
    }

    // Sync connections from model to editor
    const auto& modelConnections = model_->GetConnections();
    for (const auto& connection : modelConnections) {
        int fromAiNode, toAiNode, fromOutput, toInput;
        std::tie(fromAiNode, toAiNode, fromOutput, toInput) = connection;

        // Map AI node IDs to UI node IDs
        auto fromUiIt = aiToUiNodeMap.find(fromAiNode);
        auto toUiIt = aiToUiNodeMap.find(toAiNode);

        if (fromUiIt != aiToUiNodeMap.end() && toUiIt != aiToUiNodeMap.end()) {
            int fromUiNode = fromUiIt->second;
            int toUiNode = toUiIt->second;

            // Find the correct attribute IDs
            const auto& uiNodes = editor_->GetNodes();
            int startAttr = -1, endAttr = -1;

            for (const auto& uiNode : uiNodes) {
                if (uiNode.id == fromUiNode) {
                    if (!uiNode.outputs.empty()) {
                        startAttr = uiNode.outputs[0]; // Use first output
                    }
                }
                if (uiNode.id == toUiNode) {
                    if (!uiNode.inputs.empty()) {
                        endAttr = uiNode.inputs[0]; // Use first input
                    }
                }
            }

            if (startAttr != -1 && endAttr != -1) {
                editor_->AddLink(fromUiNode, toUiNode, startAttr, endAttr);
            }
        }
    }

    // Re-enable callback
    editor_->SetNodeChangeCallback([this]() {
        std::lock_guard<std::mutex> lock(mutex_);
        editorChanged_ = true;
    });

    modelChanged_ = false;
}

void SyncManager::SyncLoop() {
    while (running_) {
        bool shouldSyncEditor = false;
        bool shouldSyncModel = false;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (editorChanged_) {
                shouldSyncEditor = true;
                editorChanged_ = false; // Reset flag immediately
            }
            if (modelChanged_) {
                shouldSyncModel = true;
                modelChanged_ = false; // Reset flag immediately
            }
        }

        // Perform sync operations outside the lock to avoid deadlock
        if (shouldSyncEditor) {
            std::cout << "Syncing editor changes to model..." << std::endl;
            SyncEditorToModel();
        }

        if (shouldSyncModel) {
            std::cout << "Syncing model changes to editor..." << std::endl;
            SyncModelToEditor();
        }
        // std::cout << "thread..." << std::endl;
        // Sleep for a short time to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void SyncManager::HandleEditorChanges() {
    if (editorChanged_) {
        SyncEditorToModel();
    }
}

void SyncManager::HandleModelChanges() {
    if (modelChanged_) {
        SyncModelToEditor();
    }
}

void SyncManager::StartExecution(int numThreads) {
    model_->SetExecutionConfig(numThreads);
    model_->SetProgressCallback([this](const ExecutionProgress& progress) {
        HandleExecutionProgress(progress);
    });
    model_->StartExecution(numThreads);
}

void SyncManager::StopExecution() {
    model_->StopExecution();
}

bool SyncManager::IsExecuting() const {
    return model_->IsExecuting();
}

void SyncManager::HandleExecutionProgress(const ExecutionProgress& progress) {
    if (executionProgressCallback_) {
        executionProgressCallback_(progress);
    }
}
