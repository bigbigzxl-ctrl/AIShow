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
    // Do NOT start a background thread — callbacks are installed but actual
    // sync (which touches UI) must run on the main thread to avoid
    // modifying ImGui/ImNodes data from a background thread.

    // Set up callbacks (they only set flags)
    editor_->SetNodeChangeCallback([this]() {
        std::lock_guard<std::mutex> lock(mutex_);
        editorChanged_ = true;
    });

    model_->SetModelChangeCallback([this]() {
        std::lock_guard<std::mutex> lock(mutex_);
        modelChanged_ = true;
    });
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

    // Note: boundAINodeId parameter in AddNode stores the AI model node ID
    // The UI node ID will be created during ProcessDeferredOps (next frame)
    // So we use the boundAINodeId to map connections later
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

        // boundAINodeId stores the AI model's node ID for later connection mapping
        editor_->AddNode(modelNode.name, posX, posY, modelNode.id);
    }

    // Get the current UI nodes after deferred add operations
    // Note: This must be called BEFORE we try to connect them
    // We need to manually process deferred ops here to ensure nodes exist
    // Actually, let's defer connection add as well, so they happen after ProcessDeferredOps
    
    // Instead of trying to connect now, we'll queue the connections as deferred operations
    // Store connections to be added after nodes are created
    const auto& modelEdges = model_->GetEdges();
    for (const auto& edge : modelEdges) {
        const Port* fromPort = model_->GetPort(edge.fromPortId);
        const Port* toPort = model_->GetPort(edge.toPortId);
        
        if (fromPort && toPort) {
            // Find port indices within their nodes
            int fromIdx = 0, toIdx = 0;
            const auto& nodes = model_->GetNodes();
            
            for (const auto& node : nodes) {
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
            
            editor_->QueueConnectionForSync(fromPort->nodeId, toPort->nodeId, fromIdx, toIdx);
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
