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

// Forward declare editor for progress callback
NodeEditor* g_editor = nullptr;

void UpdateEditorProgress(const ExecutionProgress& progress) {
    if (g_editor) {
        g_editor->UpdateExecutionProgress(progress.nodeId, progress.progress, progress.status);
    }
}

int main() {
    std::cout << "=== AI Model Node Display System ===" << std::endl;

    // Initialize our components
    NodeEditor editor;
    g_editor = &editor;  // Store global pointer for progress updates
    if (!editor.Initialize()) {
        std::cerr << "Failed to initialize NodeEditor" << std::endl;
        return -1;
    }

    AIModel model;

    // Create SyncManager
    SyncManager syncManager(&editor, &model);
    // NOTE: Do NOT call syncManager.StartSync() as it would cause blocking
    // bidirectional sync. Nodes are added via right-click menu in the UI.

    // Wire editor's sync request button to the sync manager action
    editor.SetSyncRequestCallback([&syncManager]() {
        syncManager.SyncModelToEditor();
    });

    // Set up execution progress callback
    syncManager.SetExecutionProgressCallback([](const ExecutionProgress& progress) {
        HandleExecutionProgress(progress);
        UpdateEditorProgress(progress);
    });

    // Load a sample model (to AIModel, not to UI yet)
    model.LoadFromFile("./model.txt");

    // Sync the loaded model to the editor UI
    syncManager.SyncModelToEditor();

    std::cout << "Press SPACE to start/stop execution, or close the window to exit" << std::endl;
    std::cout << "Right-click in the editor to add nodes" << std::endl;

    // Track space key state across loop iterations
    bool spaceWasPressed = false;

    // Main loop - keep running until window is closed
    try {
        while (!glfwWindowShouldClose(editor.GetWindow())) {
            editor.Render();

            // Handle keyboard input for execution control
            // Note: This must be called after Render() which sets up ImGui context
            if (glfwGetKey(editor.GetWindow(), GLFW_KEY_SPACE) == GLFW_PRESS) {
                if (!spaceWasPressed) {
                    spaceWasPressed = true;
                    if (syncManager.IsExecuting()) {
                        syncManager.StopExecution();
                        std::cout << "Stopped execution" << std::endl;
                    } else {
                        // Clear old progress data before starting new execution
                        editor.ClearExecutionProgress();
                        syncManager.StartExecution(2);
                        std::cout << "Started execution with 2 threads" << std::endl;
                    }
                }
            } else {
                // Reset the flag when key is released
                spaceWasPressed = false;
            }

            // Small sleep to prevent busy waiting and reduce CPU usage
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception in main loop: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception in main loop" << std::endl;
    }

    // Stop execution and sync
    syncManager.StopExecution();
    // No background sync thread is used; ensure callbacks are cleared if needed
    syncManager.StopSync();
    g_editor = nullptr;  // Clear global pointer
    editor.Shutdown();

    std::cout << "\n=== Application closed ===" << std::endl;

    return 0;
}
