#include <sim/storage.hpp> // 换成你项目里的实际路径

#include <type_traits>

namespace craftsim
{

    // ---------------------------------------------------------------
    // NBT_List 的比较（此时 NBT 已完整，可以实例化 vector<NBT>::operator==）
    // ---------------------------------------------------------------
    bool NBT_List::operator==(const NBT_List &other) const
    {
        return element_type == other.element_type && elements == other.elements;
    }

    bool NBT_List::operator!=(const NBT_List &other) const
    {
        return !(*this == other);
    }

    // ---------------------------------------------------------------
    // NbtPathEvaluator 实现
    // ---------------------------------------------------------------
    std::vector<NBT *> NbtPathEvaluator::evaluate(NBT &root, const NbtPath &path)
    {
        std::vector<NBT *> current = {&root};

        for (const auto &node : path.segments)
        {
            std::vector<NBT *> next;

            std::visit([&](const auto &n)
                       {
            using T = std::decay_t<decltype(n)>;

            if constexpr (std::is_same_v<T, PathNode::Key>)
            {
                for (NBT* candidate : current)
                {
                    if (!candidate->is_compound()) continue;
                    auto& map = candidate->as_compound();
                    auto it = map.find(n.name);
                    if (it != map.end())
                        next.push_back(&it->second);
                }
            }
            else if constexpr (std::is_same_v<T, PathNode::Index>)
            {
                for (NBT* candidate : current)
                {
                    if (!candidate->is_list()) continue;
                    auto& list = candidate->as_list().elements;
                    int idx = n.value;
                    if (idx < 0) idx += static_cast<int>(list.size());
                    if (idx >= 0 && idx < static_cast<int>(list.size()))
                        next.push_back(&list[idx]);
                }
            }
            else if constexpr (std::is_same_v<T, PathNode::Wildcard>)
            {
                for (NBT* candidate : current)
                {
                    if (candidate->is_list())
                    {
                        for (auto& elem : candidate->as_list().elements)
                            next.push_back(&elem);
                    }
                    else if (candidate->is_compound())
                    {
                        for (auto& [key, val] : candidate->as_compound())
                            next.push_back(&val);
                    }
                }
            }
            else if constexpr (std::is_same_v<T, PathNode::Filter>)
            {
                for (NBT* candidate : current)
                {
                    if (candidate->is_compound() && n.matches(*candidate))
                        next.push_back(candidate); // 过滤器只筛选，不产生子节点
                }
            } }, node.data);

            current = std::move(next);
        }

        return current;
    }

    std::vector<const NBT *> NbtPathEvaluator::evaluate(const NBT &root, const NbtPath &path)
    {
        std::vector<const NBT *> current = {&root};

        for (const auto &node : path.segments)
        {
            std::vector<const NBT *> next;

            std::visit([&](const auto &n)
                       {
            using T = std::decay_t<decltype(n)>;

            if constexpr (std::is_same_v<T, PathNode::Key>)
            {
                for (const NBT* candidate : current)
                {
                    if (!candidate->is_compound()) continue;
                    const auto& map = candidate->as_compound();
                    auto it = map.find(n.name);
                    if (it != map.end())
                        next.push_back(&it->second);
                }
            }
            else if constexpr (std::is_same_v<T, PathNode::Index>)
            {
                for (const NBT* candidate : current)
                {
                    if (!candidate->is_list()) continue;
                    const auto& list = candidate->as_list().elements;
                    int idx = n.value;
                    if (idx < 0) idx += static_cast<int>(list.size());
                    if (idx >= 0 && idx < static_cast<int>(list.size()))
                        next.push_back(&list[idx]);
                }
            }
            else if constexpr (std::is_same_v<T, PathNode::Wildcard>)
            {
                for (const NBT* candidate : current)
                {
                    if (candidate->is_list())
                    {
                        for (const auto& elem : candidate->as_list().elements)
                            next.push_back(&elem);
                    }
                    else if (candidate->is_compound())
                    {
                        for (const auto& [key, val] : candidate->as_compound())
                            next.push_back(&val);
                    }
                }
            }
            else if constexpr (std::is_same_v<T, PathNode::Filter>)
            {
                for (const NBT* candidate : current)
                {
                    if (candidate->is_compound() && n.matches(*candidate))
                        next.push_back(candidate);
                }
            } }, node.data);

            current = std::move(next);
        }

        return current;
    }

} // namespace craftsim