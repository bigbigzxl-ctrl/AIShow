#pragma once

#include "NodeEditor.h"
#include "AIModel.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>

class SyncManager {
public:
    SyncManager(NodeEditor* editor, AIModel* model);
    ~SyncManager();

    void StartSync();
    void StopSync();
    void SyncEditorToModel();
    void SyncModelToEditor();

    // Execution control
    void StartExecution(int numThreads = 1);
    void StopExecution();
    bool IsExecuting() const;

    void SetExecutionProgressCallback(std::function<void(const ExecutionProgress&)> callback) {
        executionProgressCallback_ = callback;
    }

private:
    void SyncLoop();
    void HandleEditorChanges();
    void HandleModelChanges();
    void HandleExecutionProgress(const ExecutionProgress& progress);

    NodeEditor* editor_;
    AIModel* model_;
    std::thread syncThread_;
    std::mutex mutex_;
    std::atomic<bool> running_;
    bool editorChanged_;
    bool modelChanged_;

    std::function<void(const ExecutionProgress&)> executionProgressCallback_;
};
