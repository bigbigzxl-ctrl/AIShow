#include "NodeEditor.h"
#include "AIModel.h"
#include "SyncManager.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <map>

// Global execution progress tracking
std::map<int, ExecutionProgress> executionProgressMap;

void HandleExecutionProgress(const ExecutionProgress& progress) {
    executionProgressMap[progress.nodeId] = progress;
    std::cout << "Node " << progress.nodeName << " (" << progress.nodeId << "): "
              << progress.status << " - " << progress.message
              << " (" << (progress.progress * 100.0f) << "%)" << std::endl;
}

int main() {
    std::cout << "=== AI Model Node Display System ===" << std::endl;

    // Initialize our components
    NodeEditor editor;
    if (!editor.Initialize()) {
        std::cerr << "Failed to initialize NodeEditor" << std::endl;
        return -1;
    }

    AIModel model;

    // Create SyncManager
    SyncManager syncManager(&editor, &model);
    syncManager.StartSync();

    // Set up execution progress callback
    syncManager.SetExecutionProgressCallback(HandleExecutionProgress);

    // Load a sample model
    model.LoadFromFile("./model.txt");

    // Start execution with 2 threads
    syncManager.StartExecution(2);

    // Main loop
    while (!glfwWindowShouldClose(editor.GetWindow())) {
        editor.Render();

        // Handle input for execution control
        if (ImGui::IsKeyPressed(ImGuiKey_Space)) {
            if (syncManager.IsExecuting()) {
                syncManager.StopExecution();
                std::cout << "Stopped execution" << std::endl;
            } else {
                syncManager.StartExecution(2);
                std::cout << "Started execution" << std::endl;
            }
        }
    }

    // Stop execution and sync
    syncManager.StopExecution();
    syncManager.StopSync();
    editor.Shutdown();

    std::cout << "\n=== Application closed ===" << std::endl;

    return 0;
}
