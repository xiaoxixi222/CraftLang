#include <craftlang/lang.h>

#include <iostream>
namespace craftlang
{
    // 全局变量，用于存储所有子节点
    std::unordered_map<CXCursor, Cursor, CursorHash, CursorEqual>
        all_cursors;
    // 回调函数：只负责收集直接子节点
    CXChildVisitResult collectChildrenCallback(CXCursor cursor, CXCursor parent,
                                               CXClientData client_data)
    {
        (void)client_data;
        all_cursors[cursor] = Cursor{
            cursor, parent, std::vector<CXCursor>()}; // 将子节点添加到全局变量中
        all_cursors[parent].children.push_back(
            cursor); // 将子节点添加到父节点的子节点列表中
#ifdef _DEBUG
        std::cout << "node: "
                  << clang_getCString(
                         clang_getCursorKindSpelling(clang_getCursorKind(cursor)))
                  << " father: "
                  << clang_getCString(
                         clang_getCursorKindSpelling(clang_getCursorKind(parent)))
                  << std::endl;
#endif
        return CXChildVisit_Recurse;
    }
    std::vector<CXCursor> getChildCursors(CXCursor cursor)
    {
        return all_cursors[cursor].children;
    }
    void printSpelling(CXCursor cursor)
    {
        CXString name = clang_getCursorSpelling(cursor);
        if (clang_getCString(name)[0] != '\0')
            std::cout << " \"" << clang_getCString(name) << "\"";
        clang_disposeString(name);
    }

    void visitCursorRecursive(CXCursor cursor, int depth)
    {
        // 1. 打印缩进
        for (int i = 0; i < depth; i++)
            std::cout << "  "; // 每级两个空格

        // 2. 打印当前节点信息
        CXCursorKind kind = clang_getCursorKind(cursor);
        CXString kindSpelling = clang_getCursorKindSpelling(kind);
        std::cout << clang_getCString(kindSpelling);
        printSpelling(cursor);
        std::cout << std::endl;
        clang_disposeString(kindSpelling);

        // 3. 递归遍历所有子节点
        std::vector<CXCursor> children = getChildCursors(cursor);
        for (const auto &child : children)
        {
            visitCursorRecursive(child, depth + 1);
        }
    }
}
