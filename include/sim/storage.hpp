#pragma once

#include <cstdint>
#include <map>
#include <vector>
#include <string>
#include <variant>

namespace craftsim
{

    // NBT 标签类型（对应 Minecraft NBT 格式的 13 种标签）
    enum class NBTType : uint8_t
    {
        End = 0,
        Byte = 1,
        Short = 2,
        Int = 3,
        Long = 4,
        Float = 5,
        Double = 6,
        ByteArray = 7,
        String = 8,
        List = 9,
        Compound = 10,
        IntArray = 11,
        LongArray = 12
    };

    struct NBT;

    using NBT_Map = std::map<std::string, NBT>;

    // NBT_List 在 NBT 之前定义，所以比较运算符只声明不定义。
    struct NBT_List
    {
        NBTType element_type = NBTType::End; // 空列表时为 End
        std::vector<NBT> elements;

        bool operator==(const NBT_List &other) const; // 定义见 storage.cpp
        bool operator!=(const NBT_List &other) const;
    };

    struct NBT
    {
        NBTType type = NBTType::End;

        using Value = std::variant<
            std::nullptr_t,       // TAG_End
            int8_t,               // TAG_Byte
            int16_t,              // TAG_Short
            int32_t,              // TAG_Int
            int64_t,              // TAG_Long
            float,                // TAG_Float
            double,               // TAG_Double
            std::vector<int8_t>,  // TAG_Byte_Array
            std::string,          // TAG_String
            NBT_List,             // TAG_List
            NBT_Map,              // TAG_Compound
            std::vector<int32_t>, // TAG_Int_Array
            std::vector<int64_t>  // TAG_Long_Array
            >;

        Value value;

        NBT() : type(NBTType::End), value(nullptr) {}
        NBT(NBTType t, Value v) : type(t), value(std::move(v)) {}

        // ---- 工厂方法 ----
        static NBT make_end() { return {NBTType::End, nullptr}; }
        static NBT make_byte(int8_t b) { return {NBTType::Byte, b}; }
        static NBT make_bool(bool b) { return make_byte(b ? 1 : 0); } // Bool 映射为 Byte
        static NBT make_short(int16_t s) { return {NBTType::Short, s}; }
        static NBT make_int(int32_t i) { return {NBTType::Int, i}; }
        static NBT make_long(int64_t l) { return {NBTType::Long, l}; }
        static NBT make_float(float f) { return {NBTType::Float, f}; }
        static NBT make_double(double d) { return {NBTType::Double, d}; }

        static NBT make_byte_array(std::vector<int8_t> arr)
        {
            return {NBTType::ByteArray, std::move(arr)};
        }

        static NBT make_string(std::string s)
        {
            return {NBTType::String, std::move(s)};
        }

        static NBT make_list(NBTType elem_type = NBTType::End)
        {
            NBT_List list;
            list.element_type = elem_type;
            return {NBTType::List, std::move(list)};
        }

        static NBT make_compound()
        {
            return {NBTType::Compound, NBT_Map{}};
        }

        static NBT make_int_array(std::vector<int32_t> arr)
        {
            return {NBTType::IntArray, std::move(arr)};
        }

        static NBT make_long_array(std::vector<int64_t> arr)
        {
            return {NBTType::LongArray, std::move(arr)};
        }

        // ---- 类型判断 ----
        bool is_end() const { return type == NBTType::End; }
        bool is_byte() const { return type == NBTType::Byte; }
        bool is_short() const { return type == NBTType::Short; }
        bool is_int() const { return type == NBTType::Int; }
        bool is_long() const { return type == NBTType::Long; }
        bool is_float() const { return type == NBTType::Float; }
        bool is_double() const { return type == NBTType::Double; }
        bool is_byte_array() const { return type == NBTType::ByteArray; }
        bool is_string() const { return type == NBTType::String; }
        bool is_list() const { return type == NBTType::List; }
        bool is_compound() const { return type == NBTType::Compound; }
        bool is_int_array() const { return type == NBTType::IntArray; }
        bool is_long_array() const { return type == NBTType::LongArray; }

        // ---- 便捷访问 ----
        NBT_Map &as_compound() { return std::get<NBT_Map>(value); }
        const NBT_Map &as_compound() const { return std::get<NBT_Map>(value); }
        NBT_List &as_list() { return std::get<NBT_List>(value); }
        const NBT_List &as_list() const { return std::get<NBT_List>(value); }

        // 类型 + 值的精确比较
        bool operator==(const NBT &other) const
        {
            if (type != other.type)
                return false;
            return value == other.value;
        }
        bool operator!=(const NBT &other) const { return !(*this == other); }
    };

    // ---- NBT 路径 AST ----
    struct PathNode
    {
        // 1. 键，如 foo、bar、"A [crazy name]"
        struct Key
        {
            std::string name;
        };

        // 2. 索引，如 [0]
        struct Index
        {
            int value;
        };

        // 3. 通配符，如 []（代表全部）
        struct Wildcard
        {
        };

        // 4. 过滤器，如 {foo:bar}
        struct Filter
        {
            NBT_Map kv; // 值类型是 NBT，不是字符串

            // 子集匹配：目标 Compound 包含过滤器里所有键值对即匹配
            bool matches(const NBT &target) const
            {
                if (!target.is_compound())
                    return false;
                const auto &target_map = target.as_compound();
                for (const auto &[key, filter_val] : kv)
                {
                    auto it = target_map.find(key);
                    if (it == target_map.end())
                        return false;
                    if (it->second != filter_val)
                        return false; // 类型和值精确比较
                }
                return true;
            }
        };

        std::variant<Key, Index, Wildcard, Filter> data;
    };

    // NBT 路径就是节点序列（AST 形式，由外部构造）
    struct NbtPath
    {
        std::vector<PathNode> segments;
    };

    // ---- 路径求值器 ----
    class NbtPathEvaluator
    {
    public:
        // 非 const：返回所有匹配节点的指针
        static std::vector<NBT *> evaluate(NBT &root, const NbtPath &path);

        // const：返回所有匹配节点的 const 指针
        static std::vector<const NBT *> evaluate(const NBT &root, const NbtPath &path);
    };

    // ---- 存储（对应 command_storage.dat） ----
    struct Storage
    {
        std::map<std::string, NBT> data;
        // 若要持久化，根标签需包含 DataVersion（Int）；内存模拟可忽略。
    };

    // ---- Scoreboard ----
    enum class ScoreType
    {
        DUMMY,
    };

    struct ScoreProgram
    {
        std::string name;
        ScoreType type = ScoreType::DUMMY;
        std::map<std::string, int> scores; // player name -> score
    };

    struct Scoreboard
    {
        std::map<std::string, ScoreProgram> objectives; // objective name -> ScoreProgram
    };

} // namespace craftsim