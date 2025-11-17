#include "NodeEditor.h"
#include <algorithm>
#include <iostream>

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

    // Poll events
    glfwPollEvents();

    // Start ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Main window
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("AI Model Node Editor", nullptr,
                // ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

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
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window_, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window_);
}

void NodeEditor::AddNode(const std::string& name, float posX, float posY, int boundAINodeId) {
    const int id = current_id_++;
    assert(!nodes_.contains(id));

    UINode node;
    node.id = id;
    node.name = name;
    node.positionX = posX;
    node.positionY = posY;
    node.selected = false;
    node.boundAINodeId = boundAINodeId;
    node.inputs.push_back(current_id_++);
    node.outputs.push_back(current_id_++);

    nodes_.insert(id, node);

    std::cout << "Added node: " << name << " at (" << posX << ", " << posY << ")" << std::endl;
    if (onNodeChange_) onNodeChange_();
}

void NodeEditor::RemoveNode(int nodeId) {
    // Remove associated links first
    std::vector<int> links_to_remove;
    for (const auto& link : links_.elements()) {
        if (link.start_node == nodeId || link.end_node == nodeId) {
            links_to_remove.push_back(link.id);
        }
    }
    for (int link_id : links_to_remove) {
        links_.erase(link_id);
    }

    nodes_.erase(nodeId);
    std::cout << "Removed node with ID: " << nodeId << std::endl;
    if (onNodeChange_) onNodeChange_();
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
    std::cout << "Added link from node " << startNode << " to node " << endNode << std::endl;
}

void NodeEditor::RemoveLink(int linkId) {
    links_.erase(linkId);
    std::cout << "Removed link with ID: " << linkId << std::endl;
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
