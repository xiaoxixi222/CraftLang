#pragma once

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <optional>
#include <iostream>

namespace craftsim::fs_utils
{
    namespace fs = std::filesystem;

    // read
    std::string readFile(const fs::path &path);                   // 读整个文件为字符串，失败打印错误并返回空串
    std::optional<std::string> tryReadFile(const fs::path &path); // 读整个文件，失败返回 nullopt，不打印
    std::vector<std::string> readLines(const fs::path &path);     // 逐行读，剥掉行尾 \r，失败返回空 vector
    // write
    bool writeFile(const fs::path &path, const std::string &content);             // 覆盖写入整个字符串
    bool appendFile(const fs::path &path, const std::string &content);            // 追加到文件末尾
    bool writeLines(const fs::path &path, const std::vector<std::string> &lines); // 每行后加 \n 写入
    // exists
    bool exists(const fs::path &path);      // 路径是否存在
    bool isFile(const fs::path &path);      // 是否是普通文件
    bool isDirectory(const fs::path &path); // 是否是目录
    // directory
    bool createDirectories(const fs::path &path);                   // 递归创建目录
    std::vector<fs::path> listDirectory(const fs::path &path);      // 列出目录条目（不递归）
    std::vector<fs::path> listFilesRecursive(const fs::path &path); // 递归列出所有普通文件
    // update file
    bool removeFile(const fs::path &path);                                          // 删除单个文件/空目录
    bool removeAll(const fs::path &path);                                           // 递归删除
    bool rename(const fs::path &from, const fs::path &to);                          // 重命名/移动
    bool copyFile(const fs::path &from, const fs::path &to, bool overwrite); // 复制文件
    // path
    std::string extension(const fs::path &path); // 扩展名，含点，如 ".cpp"
    std::string stem(const fs::path &path);      // 主文件名，不含目录和扩展名，如 "main"
    std::string filename(const fs::path &path);  // 文件名，含扩展名，如 "main.cpp"
    fs::path parent(const fs::path &path);       // 父目录
    fs::path normalize(const fs::path &path);    // 绝对化并弱化 . 和 ..
    // file_attr
    std::optional<std::uintmax_t> fileSize(const fs::path &path);   // 文件大小，失败 nullopt
    std::optional<std::time_t> lastWriteTime(const fs::path &path); // 最后修改时间，失败 nullopt

    // ---------------------------------------------------------------
    // 读取整个文件为字符串
    // ---------------------------------------------------------------
    inline std::string readFile(const fs::path &path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
        {
            std::cerr << "craftsim: failed to open " << path << "\n";
            return {};
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    // 读取失败时返回 nullopt，不打印错误
    inline std::optional<std::string> tryReadFile(const fs::path &path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
            return std::nullopt;
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    // ---------------------------------------------------------------
    // 写入字符串到文件（覆盖），返回是否成功
    // ---------------------------------------------------------------
    inline bool writeFile(const fs::path &path, const std::string &content)
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file.is_open())
        {
            std::cerr << "craftsim: failed to write " << path << "\n";
            return false;
        }
        file << content;
        return static_cast<bool>(file);
    }

    // 追加到文件末尾
    inline bool appendFile(const fs::path &path, const std::string &content)
    {
        std::ofstream file(path, std::ios::binary | std::ios::app);
        if (!file.is_open())
        {
            std::cerr << "craftsim: failed to append " << path << "\n";
            return false;
        }
        file << content;
        return static_cast<bool>(file);
    }

    // ---------------------------------------------------------------
    // 逐行读取
    // ---------------------------------------------------------------
    inline std::vector<std::string> readLines(const fs::path &path)
    {
        std::vector<std::string> lines;
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
        {
            std::cerr << "craftsim: failed to open " << path << "\n";
            return lines;
        }
        std::string line;
        while (std::getline(file, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back(); // 兼容 CRLF
            lines.push_back(std::move(line));
        }
        return lines;
    }

    // 把若干行写进文件，每行后自动加 '\n'
    inline bool writeLines(const fs::path &path, const std::vector<std::string> &lines)
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file.is_open())
        {
            std::cerr << "craftsim: failed to write " << path << "\n";
            return false;
        }
        for (const auto &line : lines)
            file << line << '\n';
        return static_cast<bool>(file);
    }

    // ---------------------------------------------------------------
    // 存在性 / 类型判断
    // ---------------------------------------------------------------
    inline bool exists(const fs::path &path)
    {
        std::error_code ec;
        return fs::exists(path, ec);
    }

    inline bool isFile(const fs::path &path)
    {
        std::error_code ec;
        return fs::is_regular_file(path, ec);
    }

    inline bool isDirectory(const fs::path &path)
    {
        std::error_code ec;
        return fs::is_directory(path, ec);
    }

    // ---------------------------------------------------------------
    // 目录操作
    // ---------------------------------------------------------------
    inline bool createDirectories(const fs::path &path)
    {
        std::error_code ec;
        fs::create_directories(path, ec);
        if (ec)
        {
            std::cerr << "craftsim: failed to create " << path
                      << ": " << ec.message() << "\n";
            return false;
        }
        return true;
    }

    // 列出目录下的条目（不递归），失败返回空 vector
    inline std::vector<fs::path> listDirectory(const fs::path &path)
    {
        std::vector<fs::path> entries;
        std::error_code ec;
        if (!fs::is_directory(path, ec))
            return entries;
        for (auto it = fs::directory_iterator(path, ec);
             it != fs::directory_iterator(); it.increment(ec))
        {
            if (ec)
                break;
            entries.push_back(it->path());
        }
        return entries;
    }

    // 递归列出目录下所有普通文件
    inline std::vector<fs::path> listFilesRecursive(const fs::path &path)
    {
        std::vector<fs::path> files;
        std::error_code ec;
        if (!fs::is_directory(path, ec))
            return files;
        for (auto it = fs::recursive_directory_iterator(path, ec);
             it != fs::recursive_directory_iterator(); it.increment(ec))
        {
            if (ec)
                break;
            if (it->is_regular_file(ec))
                files.push_back(it->path());
        }
        return files;
    }

    // ---------------------------------------------------------------
    // 删除 / 重命名 / 复制
    // ---------------------------------------------------------------
    inline bool removeFile(const fs::path &path)
    {
        std::error_code ec;
        bool ok = fs::remove(path, ec);
        if (ec)
        {
            std::cerr << "craftsim: failed to remove " << path
                      << ": " << ec.message() << "\n";
            return false;
        }
        return ok;
    }

    inline bool removeAll(const fs::path &path)
    {
        std::error_code ec;
        fs::remove_all(path, ec);
        if (ec)
        {
            std::cerr << "craftsim: failed to remove_all " << path
                      << ": " << ec.message() << "\n";
            return false;
        }
        return true;
    }

    inline bool rename(const fs::path &from, const fs::path &to)
    {
        std::error_code ec;
        fs::rename(from, to, ec);
        if (ec)
        {
            std::cerr << "craftsim: failed to rename " << from << " -> " << to
                      << ": " << ec.message() << "\n";
            return false;
        }
        return true;
    }

    inline bool copyFile(const fs::path &from, const fs::path &to, bool overwrite = true)
    {
        std::error_code ec;
        auto opts = overwrite ? fs::copy_options::overwrite_existing
                              : fs::copy_options::none;
        fs::copy_file(from, to, opts, ec);
        if (ec)
        {
            std::cerr << "craftsim: failed to copy " << from << " -> " << to
                      << ": " << ec.message() << "\n";
            return false;
        }
        return true;
    }

    // ---------------------------------------------------------------
    // 路径字符串工具
    // ---------------------------------------------------------------
    // 扩展名（含点），没有则返回空
    inline std::string extension(const fs::path &path)
    {
        return path.extension().string();
    }

    // 主文件名（不含目录，不含扩展名）
    inline std::string stem(const fs::path &path)
    {
        return path.stem().string();
    }

    // 文件名（含扩展名）
    inline std::string filename(const fs::path &path)
    {
        return path.filename().string();
    }

    // 父目录
    inline fs::path parent(const fs::path &path)
    {
        return path.parent_path();
    }

    // 规范化：转成绝对路径并弱化 . 和 ..
    inline fs::path normalize(const fs::path &path)
    {
        std::error_code ec;
        auto abs = fs::absolute(path, ec);
        if (ec)
            return path;
        return abs.lexically_normal();
    }

    // ---------------------------------------------------------------
    // 文件大小 / 修改时间
    // ---------------------------------------------------------------
    inline std::optional<std::uintmax_t> fileSize(const fs::path &path)
    {
        std::error_code ec;
        auto sz = fs::file_size(path, ec);
        if (ec)
            return std::nullopt;
        return sz;
    }

    // 修改时间转 time_t；失败返回 nullopt
    inline std::optional<std::time_t> lastWriteTime(const fs::path &path)
    {
        std::error_code ec;
        auto t = fs::last_write_time(path, ec);
        if (ec)
            return std::nullopt;
        // C++20 有 clock_cast；这里用简单近似，转成 time_t
        return std::chrono::system_clock::to_time_t(
            std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                t - fs::file_time_type::clock::now() + std::chrono::system_clock::now()));
    }

} // namespace craftsim::fs_utils