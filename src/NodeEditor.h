#pragma once

#include <vector>
#include <string>
#include <functional>
#include <algorithm>
#include <cassert>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imnodes.h"

template<typename ElementType>
struct Span
{
    using iterator = ElementType*;

    template<typename Container>
    Span(Container& c) : begin_(c.data()), end_(begin_ + c.size())
    {
    }

    iterator begin() const { return begin_; }
    iterator end() const { return end_; }

private:
    iterator begin_;
    iterator end_;
};

template<typename ElementType>
class IdMap
{
public:
    using iterator = typename std::vector<ElementType>::iterator;
    using const_iterator = typename std::vector<ElementType>::const_iterator;

    // Iterators

    const_iterator begin() const { return elements_.begin(); }
    const_iterator end() const { return elements_.end(); }

    // Element access

    Span<ElementType> elements() { return elements_; }
    Span<const ElementType> elements() const { return elements_; }

    // Capacity

    bool   empty() const { return sorted_ids_.empty(); }
    size_t size() const { return sorted_ids_.size(); }

    // Modifiers

    std::pair<iterator, bool> insert(int id, const ElementType& element);
    std::pair<iterator, bool> insert(int id, ElementType&& element);
    size_t                    erase(int id);
    void                      clear();

    // Lookup

    iterator       find(int id);
    const_iterator find(int id) const;
    bool           contains(int id) const;

private:
    std::vector<ElementType> elements_;
    std::vector<int>         sorted_ids_;
};

template<typename ElementType>
std::pair<typename IdMap<ElementType>::iterator, bool> IdMap<ElementType>::insert(
    const int          id,
    const ElementType& element)
{
    auto lower_bound = std::lower_bound(sorted_ids_.begin(), sorted_ids_.end(), id);

    if (lower_bound != sorted_ids_.end() && id == *lower_bound)
    {
        return std::make_pair(
            std::next(elements_.begin(), std::distance(sorted_ids_.begin(), lower_bound)), false);
    }

    auto insert_element_at =
        std::next(elements_.begin(), std::distance(sorted_ids_.begin(), lower_bound));

    sorted_ids_.insert(lower_bound, id);
    return std::make_pair(elements_.insert(insert_element_at, element), true);
}

template<typename ElementType>
std::pair<typename IdMap<ElementType>::iterator, bool> IdMap<ElementType>::insert(
    const int     id,
    ElementType&& element)
{
    auto lower_bound = std::lower_bound(sorted_ids_.begin(), sorted_ids_.end(), id);

    if (lower_bound != sorted_ids_.end() && id == *lower_bound)
    {
        return std::make_pair(
            std::next(elements_.begin(), std::distance(sorted_ids_.begin(), lower_bound)), false);
    }

    auto insert_element_at =
        std::next(elements_.begin(), std::distance(sorted_ids_.begin(), lower_bound));

    sorted_ids_.insert(lower_bound, id);
    return std::make_pair(elements_.insert(insert_element_at, std::move(element)), true);
}

template<typename ElementType>
size_t IdMap<ElementType>::erase(const int id)
{
    auto lower_bound = std::lower_bound(sorted_ids_.begin(), sorted_ids_.end(), id);

    if (lower_bound == sorted_ids_.end() || id != *lower_bound)
    {
        return 0ull;
    }

    auto erase_element_at =
        std::next(elements_.begin(), std::distance(sorted_ids_.begin(), lower_bound));

    sorted_ids_.erase(lower_bound);
    elements_.erase(erase_element_at);

    return 1ull;
}

template<typename ElementType>
void IdMap<ElementType>::clear()
{
    elements_.clear();
    sorted_ids_.clear();
}

template<typename ElementType>
typename IdMap<ElementType>::iterator IdMap<ElementType>::find(const int id)
{
    const auto lower_bound = std::lower_bound(sorted_ids_.cbegin(), sorted_ids_.cend(), id);
    return (lower_bound == sorted_ids_.cend() || *lower_bound != id)
               ? elements_.end()
               : std::next(elements_.begin(), std::distance(sorted_ids_.cbegin(), lower_bound));
}

template<typename ElementType>
typename IdMap<ElementType>::const_iterator IdMap<ElementType>::find(const int id) const
{
    const auto lower_bound = std::lower_bound(sorted_ids_.cbegin(), sorted_ids_.cend(), id);
    return (lower_bound == sorted_ids_.cend() || *lower_bound != id)
               ? elements_.cend()
               : std::next(elements_.cbegin(), std::distance(sorted_ids_.cbegin(), lower_bound));
}

template<typename ElementType>
bool IdMap<ElementType>::contains(const int id) const
{
    const auto lower_bound = std::lower_bound(sorted_ids_.cbegin(), sorted_ids_.cend(), id);

    if (lower_bound == sorted_ids_.cend())
    {
        return false;
    }

    return *lower_bound == id;
}


struct UINode {
    int id;
    std::string name;
    std::vector<int> inputs;
    std::vector<int> outputs;
    float positionX;
    float positionY;
    bool selected;
    int boundAINodeId; // ID of the bound AI node
};

struct Link {
    int id;
    int start_node;
    int end_node;
    int start_attr;
    int end_attr;
};

class NodeEditor {
public:
    NodeEditor();
    ~NodeEditor();

    bool Initialize();
    void Shutdown();
    void Render();
    void AddNode(const std::string& name, float posX, float posY, int boundAINodeId = -1);
    void RemoveNode(int nodeId);
    void AddLink(int startNode, int endNode, int startAttr, int endAttr);
    void RemoveLink(int linkId);
    void UpdateNodePosition(int nodeId, float posX, float posY);
    void QueueConnectionForSync(int fromAiNodeId, int toAiNodeId, int fromOutput, int toInput);

    // Element access
    UINode&       node(int node_id);
    const UINode& node(int node_id) const;
    Span<const UINode> nodes() const;
    Span<const Link>   links() const;

    // Capacity
    size_t num_nodes() const;
    size_t num_links() const;

    // Compatibility interface for existing code
    std::vector<UINode> GetNodes() const;
    std::vector<Link> GetLinks() const;

    void SetNodeChangeCallback(std::function<void()> callback) { onNodeChange_ = callback; }
    void SetSyncRequestCallback(std::function<void()> callback) { onSyncRequest_ = callback; }
    void UpdateExecutionProgress(int nodeId, float progress, const std::string& status) {
        executionProgress_[nodeId] = {progress, status};
    }
    void ClearExecutionProgress() {
        executionProgress_.clear();
    }

    GLFWwindow* GetWindow() { return window_; }

private:
    IdMap<UINode> nodes_;
    IdMap<Link>   links_;
    int current_id_;
    GLFWwindow* window_;
    ImNodesContext* imnodes_context_;
    std::function<void()> onNodeChange_;
    std::function<void()> onSyncRequest_;

    // Track execution progress for each AI node ID
    struct ExecutionState {
        float progress; // 0.0 to 1.0
        std::string status; // "running", "completed", etc.
    };
    std::unordered_map<int, ExecutionState> executionProgress_;

    // Deferred operations to avoid modifying ImNodes state during Render()
    struct DeferredNodeOp {
        enum Type { ADD, REMOVE } type;
        std::string name;
        float posX, posY;
        int boundAINodeId;
        int nodeId;
    };
    struct PendingConnection {
        int fromAiNodeId;
        int toAiNodeId;
        int fromOutput;
        int toInput;
    };
    std::vector<DeferredNodeOp> deferredOps_;
    std::vector<DeferredNodeOp> pendingOps_; // Operations queued during this frame
    std::vector<PendingConnection> pendingConnections_; // Connections to be made after nodes are created
    void ProcessDeferredOps();
};

