#include "NodeEditor.h"
#include <algorithm>
#include <iostream>
#include <unordered_map>
#include <map>

NodeEditor::NodeEditor()
    : nodes_(), links_(), current_id_(0), window_(nullptr), imnodes_context_(nullptr), onNodeChange_(nullptr) {
}

NodeEditor::~NodeEditor() {
    Shutdown();
}

bool NodeEditor::Initialize() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    // Create window
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window_ = glfwCreateWindow(1280, 720, "AI Model Node Display System", nullptr, nullptr);
    if (!window_) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1); // Enable vsync
    glfwShowWindow(window_); // Explicitly show the window

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    // Initialize ImGui backends
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Initialize ImNodes
    imnodes_context_ = ImNodes::CreateContext();
    ImNodes::SetCurrentContext(imnodes_context_);

    std::cout << "NodeEditor initialized successfully" << std::endl;
    return true;
}

void NodeEditor::Shutdown() {
    if (imnodes_context_) {
        ImNodes::DestroyContext(imnodes_context_);
        imnodes_context_ = nullptr;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }

    glfwTerminate();
    std::cout << "NodeEditor shutdown" << std::endl;
}

void NodeEditor::Render() {
    if (!window_) return;

    // Process operations queued during the last frame before starting a new ImGui frame
    deferredOps_ = pendingOps_;
    pendingOps_.clear();
    ProcessDeferredOps();

    // Poll events and start ImGui frame
    glfwPollEvents();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Main window
    // std::cout << "Render: Creating main window" << std::endl;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    // Enable MenuBar so BeginMenuBar() returns true and the toolbar/button is visible
    ImGui::Begin("AI Model Node Editor", nullptr,
                 ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar);

    // Small toolbar: allow manual sync request
    if (ImGui::BeginMenuBar()) {
        if (ImGui::Button("Sync Model -> Editor")) {
            if (onSyncRequest_) onSyncRequest_();
        }
        ImGui::EndMenuBar();
    }

    // Node editor
    ImNodes::BeginNodeEditor();

    // Render nodes
    for (const auto& node : nodes_.elements()) {
        // Set node position in ImNodes
        ImNodes::SetNodeGridSpacePos(node.id, ImVec2(node.positionX, node.positionY));

        ImNodes::BeginNode(node.id);
        ImNodes::BeginNodeTitleBar();
        ImGui::Text("%s", node.name.c_str());
        ImNodes::EndNodeTitleBar();

        // Draw progress bar below the title if node is executing
        if (node.boundAINodeId >= 0) {
            auto it = executionProgress_.find(node.boundAINodeId);
            if (it != executionProgress_.end() && it->second.status == "running") {
                // Get current cursor position (start of content area below title)
                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                ImVec2 cursor_min = ImGui::GetCursorScreenPos();
                
                // Progress bar dimensions: fill node width, small height
                float bar_height = 8.0f;
                float bar_width = 120.0f;  // Standard ImNodes node width
                
                ImVec2 bar_min = cursor_min;
                ImVec2 bar_max = ImVec2(cursor_min.x + bar_width, cursor_min.y + bar_height);
                
                // Draw background bar
                ImU32 bg_color = ImGui::GetColorU32(ImVec4(0.2f, 0.2f, 0.2f, 0.3f));
                draw_list->AddRectFilled(bar_min, bar_max, bg_color);
                
                // Draw filled progress (yellow, 50% opacity)
                float progress_width = bar_width * it->second.progress;
                if (progress_width > 0.0f) {
                    ImVec2 progress_max = ImVec2(bar_min.x + progress_width, bar_max.y);
                    ImU32 progress_color = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 0.0f, 0.9f));
                    draw_list->AddRectFilled(bar_min, progress_max, progress_color);
                }
                
                // Move cursor down to make space for the progress bar
                ImGui::Dummy(ImVec2(bar_width, bar_height));
            }
        }

        // Input attributes
        for (size_t i = 0; i < node.inputs.size(); ++i) {
            ImNodes::BeginInputAttribute(node.inputs[i]);
            ImGui::Text("Input %zu", i + 1);
            ImNodes::EndInputAttribute();
        }

        // Output attributes
        for (size_t i = 0; i < node.outputs.size(); ++i) {
            ImNodes::BeginOutputAttribute(node.outputs[i]);
            ImGui::Text("Output %zu", i + 1);
            ImNodes::EndOutputAttribute();
        }

        ImNodes::EndNode();
    }

    // Render links
    for (const auto& link : links_.elements()) {
        ImNodes::Link(link.id, link.start_attr, link.end_attr);
    }

    // Handle right-click context menu for adding nodes
    if (ImNodes::IsEditorHovered() && ImGui::IsMouseClicked(1)) {
        ImGui::OpenPopup("AddNodePopup");
    }

    if (ImGui::BeginPopup("AddNodePopup")) {
        if (ImGui::MenuItem("Add Convolution Layer")) {
            // Add node at a default position
            AddNode("Convolution Layer", 200.0f, 200.0f);
        }
        if (ImGui::MenuItem("Add Max Pooling")) {
            // Add node at a default position
            AddNode("Max Pooling", 400.0f, 200.0f);
        }
        if (ImGui::MenuItem("Add Dense Layer")) {
            // Add node at a default position
            AddNode("Dense Layer", 600.0f, 200.0f);
        }
        ImGui::EndPopup();
    }

    ImNodes::EndNodeEditor();

    // Update node positions from ImNodes (after user interaction)
    for (auto& node : nodes_.elements()) {
        ImVec2 currentPos = ImNodes::GetNodeGridSpacePos(node.id);
        if (currentPos.x != node.positionX || currentPos.y != node.positionY) {
            node.positionX = currentPos.x;
            node.positionY = currentPos.y;
        }
    }

    // Handle new links
    int start_attr, end_attr;
    if (ImNodes::IsLinkCreated(&start_attr, &end_attr)) {
        // Find nodes for these attributes
        int start_node = -1, end_node = -1;
        for (const auto& node : nodes_.elements()) {
            for (int input : node.inputs) {
                if (input == start_attr) start_node = node.id;
                if (input == end_attr) end_node = node.id;
            }
            for (int output : node.outputs) {
                if (output == start_attr) start_node = node.id;
                if (output == end_attr) end_node = node.id;
            }
        }

        if (start_node != -1 && end_node != -1) {
            AddLink(start_node, end_node, start_attr, end_attr);
        }
    }

    // Handle deleted links
    int link_id;
    if (ImNodes::IsLinkDestroyed(&link_id)) {
        RemoveLink(link_id);
    }

    {
        const int num_selected = ImNodes::NumSelectedLinks();
        if (num_selected > 0 && ImGui::IsKeyReleased(ImGuiKey_Delete))
        {
            static std::vector<int> selected_links;
            selected_links.resize(static_cast<size_t>(num_selected));
            ImNodes::GetSelectedLinks(selected_links.data());
            for (const int edge_id : selected_links)
            {
                RemoveLink(edge_id);
            }
        }
    }


    // Handle node deletion (Delete key)
    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        // Get selected nodes
        std::vector<int> selectedNodes;
        for (const auto& node : nodes_.elements()) {
            if (ImNodes::IsNodeSelected(node.id)) {
                selectedNodes.push_back(node.id);
            }
        }

        // Remove selected nodes
        for (int nodeId : selectedNodes) {
            RemoveNode(nodeId);
        }
    }

    ImGui::End();

    // Render
    // std::cout << "Render: Calling ImGui::Render()" << std::endl;
    ImGui::Render();
    // std::cout << "Render: Getting framebuffer size" << std::endl;
    int display_w, display_h;
    glfwGetFramebufferSize(window_, &display_w, &display_h);
    // std::cout << "Render: Setting viewport to " << display_w << "x" << display_h << std::endl;
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
    glClear(GL_COLOR_BUFFER_BIT);
    // std::cout << "Render: Drawing render data" << std::endl;
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // std::cout << "Render: Swapping buffers" << std::endl;
    glfwSwapBuffers(window_);
}

void NodeEditor::AddNode(const std::string& name, float posX, float posY, int boundAINodeId) {
    // Avoid identical duplicate add requests within a single frame
    for (const auto& p : pendingOps_) {
        if (p.type == DeferredNodeOp::ADD && p.name == name && p.posX == posX && p.posY == posY && p.boundAINodeId == boundAINodeId) {
            return;
        }
    }
    DeferredNodeOp op;
    op.type = DeferredNodeOp::ADD;
    op.name = name;
    op.posX = posX;
    op.posY = posY;
    op.boundAINodeId = boundAINodeId;
    pendingOps_.push_back(op);
}

void NodeEditor::RemoveNode(int nodeId) {
    // Avoid duplicate remove requests
    for (const auto& p : pendingOps_) {
        if (p.type == DeferredNodeOp::REMOVE && p.nodeId == nodeId) return;
    }
    DeferredNodeOp op;
    op.type = DeferredNodeOp::REMOVE;
    op.nodeId = nodeId;
    pendingOps_.push_back(op);
}

void NodeEditor::AddLink(int startNode, int endNode, int startAttr, int endAttr) {
    const int id = current_id_++;
    assert(!links_.contains(id));

    Link link;
    link.id = id;
    link.start_node = startNode;
    link.end_node = endNode;
    link.start_attr = startAttr;
    link.end_attr = endAttr;

    links_.insert(id, link);
}

void NodeEditor::RemoveLink(int linkId) {
    links_.erase(linkId);
    std::cout << "Removed link with ID: " << linkId << std::endl;
}

void NodeEditor::QueueConnectionForSync(int fromAiNodeId, int toAiNodeId, int fromOutput, int toInput) {
    PendingConnection conn;
    conn.fromAiNodeId = fromAiNodeId;
    conn.toAiNodeId = toAiNodeId;
    conn.fromOutput = fromOutput;
    conn.toInput = toInput;
    pendingConnections_.push_back(conn);
}

void NodeEditor::UpdateNodePosition(int nodeId, float posX, float posY) {
    auto iter = nodes_.find(nodeId);
    if (iter != nodes_.end()) {
        iter->positionX = posX;
        iter->positionY = posY;
    }
}

// Element access
UINode& NodeEditor::node(int node_id) {
    return const_cast<UINode&>(static_cast<const NodeEditor*>(this)->node(node_id));
}

const UINode& NodeEditor::node(int node_id) const {
    const auto iter = nodes_.find(node_id);
    assert(iter != nodes_.end());
    return *iter;
}

Span<const UINode> NodeEditor::nodes() const {
    return nodes_.elements();
}

Span<const Link> NodeEditor::links() const {
    return links_.elements();
}

// Capacity
size_t NodeEditor::num_nodes() const {
    return nodes_.size();
}

size_t NodeEditor::num_links() const {
    return links_.size();
}

// Compatibility interface implementations
std::vector<UINode> NodeEditor::GetNodes() const {
    std::vector<UINode> result;
    for (const auto& node : nodes_.elements()) {
        result.push_back(node);
    }
    return result;
}

std::vector<Link> NodeEditor::GetLinks() const {
    std::vector<Link> result;
    for (const auto& link : links_.elements()) {
        result.push_back(link);
    }
    return result;
}

void NodeEditor::ProcessDeferredOps() {
    if (deferredOps_.empty()) {
        return;
    }
    try {
        for (const auto& op : deferredOps_) {
            if (op.type == DeferredNodeOp::ADD) {
                const int id = current_id_++;
                assert(!nodes_.contains(id));

                UINode node;
                node.id = id;
                node.name = op.name;
                node.positionX = op.posX;
                node.positionY = op.posY;
                node.selected = false;
                node.boundAINodeId = op.boundAINodeId;
                node.inputs.push_back(current_id_++);
                node.outputs.push_back(current_id_++);

                nodes_.insert(id, node);
            } else if (op.type == DeferredNodeOp::REMOVE) {
                // Remove associated links first
                std::vector<int> links_to_remove;
                for (const auto& link : links_.elements()) {
                    if (link.start_node == op.nodeId || link.end_node == op.nodeId) {
                        links_to_remove.push_back(link.id);
                    }
                }
                for (int link_id : links_to_remove) {
                    links_.erase(link_id);
                }

                nodes_.erase(op.nodeId);
            }
        }
        
        // After all nodes have been created, process pending connections
        // Build a map from boundAINodeId to UI node ID
        std::map<int, int> aiToUiNodeMap;
        for (const auto& node : nodes_.elements()) {
            if (node.boundAINodeId != -1) {
                aiToUiNodeMap[node.boundAINodeId] = node.id;
            }
        }

        // Now create links for pending connections
        for (const auto& conn : pendingConnections_) {
            auto fromIt = aiToUiNodeMap.find(conn.fromAiNodeId);
            auto toIt = aiToUiNodeMap.find(conn.toAiNodeId);

            if (fromIt != aiToUiNodeMap.end() && toIt != aiToUiNodeMap.end()) {
                int fromUiNodeId = fromIt->second;
                int toUiNodeId = toIt->second;

                // Find the attribute IDs from the nodes
                const UINode* fromNode = nullptr;
                const UINode* toNode = nullptr;

                for (const auto& node : nodes_.elements()) {
                    if (node.id == fromUiNodeId) fromNode = &node;
                    if (node.id == toUiNodeId) toNode = &node;
                }

                if (fromNode && toNode && !fromNode->outputs.empty() && !toNode->inputs.empty()) {
                    int startAttr = fromNode->outputs[0];
                    int endAttr = toNode->inputs[0];
                    AddLink(fromUiNodeId, toUiNodeId, startAttr, endAttr);
                }
            }
        }
        
        if (onNodeChange_) onNodeChange_();
        deferredOps_.clear();
        pendingConnections_.clear();
    } catch (const std::exception& e) {
        std::cerr << "ProcessDeferredOps: Exception caught: " << e.what() << std::endl;
        deferredOps_.clear();
        pendingConnections_.clear();
        throw;
    } catch (...) {
        std::cerr << "ProcessDeferredOps: Unknown exception caught" << std::endl;
        deferredOps_.clear();
        pendingConnections_.clear();
        throw;
    }
}
