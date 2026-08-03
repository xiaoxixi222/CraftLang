#ifndef CRAFTLANG_LANG_H
#define CRAFTLANG_LANG_H

#include <clang-c/Index.h>
#include <cstdint>
#include <cstddef>
#include <unordered_map>
#include <vector>


namespace craftlang {

struct Cursor
{
    CXCursor cursor;
    CXCursor parent;
    std::vector<CXCursor> children;
};

// 自定义哈希（基于内部数据）
struct CursorHash
{
    std::size_t operator()(const CXCursor &c) const
    {
        return reinterpret_cast<uintptr_t>(c.data[0]) ^
               reinterpret_cast<uintptr_t>(c.data[1]) ^
               reinterpret_cast<uintptr_t>(c.data[2]);
    }
};

// 自定义相等（使用 libclang 官方函数）
struct CursorEqual
{
    bool operator()(const CXCursor &a, const CXCursor &b) const
    {
        return clang_equalCursors(a, b);
    }
};

extern std::unordered_map<CXCursor, Cursor, CursorHash, CursorEqual> all_cursors;

// 回调函数：只负责收集直接子节点
CXChildVisitResult collectChildrenCallback(CXCursor cursor, CXCursor parent,
                                           CXClientData client_data);
std::vector<CXCursor> getChildCursors(CXCursor cursor);
void printSpelling(CXCursor cursor);
void visitCursorRecursive(CXCursor cursor, int depth);

} // namespace craftlang

#endif // CRAFTLANG_LANG_H