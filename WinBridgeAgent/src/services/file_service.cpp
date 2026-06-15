/*
 * Copyright (C) 2026 Codyard
 *
 * This file is part of WinBridgeAgent.
 *
 * WinBridgeAgent is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * WinBridgeAgent is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with WinBridgeAgent. If not, see <https://www.gnu.org/licenses/\>.
 */
#include "services/file_service.h"
#include "support/config_manager.h"
#include "policy/policy_guard.h"
#include "utils/encoding_utils.h"
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {

// 诊断日志：输出到 Windows Debug Output
static void DiagLog(const std::string& msg) {
    OutputDebugStringA(msg.c_str());
    OutputDebugStringA("\n");
}

std::string toLower(const std::string& value) {
    std::string out = value;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return out;
}

std::string trimTrailingSlash(const std::string& path) {
    if (path.size() > 3 && (path.back() == '\\' || path.back() == '/')) {
        return path.substr(0, path.size() - 1);
    }
    return path;
}

static bool pathExistsA(const std::string& path) {
    std::wstring pathW = Utf8ToWide(path);
    if (pathW.empty()) {
        return false;
    }
    DWORD attrs = GetFileAttributesW(pathW.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES;
}

static std::string getDirNameA(const std::string& path) {
    if (path.empty()) return "";
    std::wstring pathW = Utf8ToWide(path);
    if (pathW.empty()) {
        return "";
    }
    wchar_t buf[MAX_PATH] = {0};
    wcsncpy(buf, pathW.c_str(), MAX_PATH - 1);
    if (!PathRemoveFileSpecW(buf)) {
        return "";
    }
    return WideToUtf8(buf);
}

static std::string normalizeLineEndings(const std::string& input, const std::string& mode) {
    std::string m = toLower(mode);
    if (m.empty() || m == "auto") {
        // Keep as-is.
        return input;
    }
    if (m == "lf") {
        std::string out;
        out.reserve(input.size());
        for (size_t i = 0; i < input.size(); ++i) {
            char c = input[i];
            if (c == '\r') {
                // Drop CR, preserve LF if present.
                continue;
            }
            out.push_back(c);
        }
        return out;
    }
    if (m == "crlf") {
        std::string out;
        out.reserve(input.size() + input.size() / 20);
        for (size_t i = 0; i < input.size(); ++i) {
            char c = input[i];
            if (c == '\r') {
                // If already CRLF, keep CR and let next LF through.
                out.push_back('\r');
                continue;
            }
            if (c == '\n') {
                // If previous wasn't CR, add CR.
                if (i == 0 || input[i - 1] != '\r') {
                    out.push_back('\r');
                }
                out.push_back('\n');
                continue;
            }
            out.push_back(c);
        }
        return out;
    }
    // Unknown mode: keep as-is.
    return input;
}
} // namespace

FileService::FileService(ConfigManager* configManager, PolicyGuard* policyGuard)
    : configManager_(configManager), policyGuard_(policyGuard) {
}

std::vector<FileInfo> FileService::findFiles(const FindFilesParams& params) {
    std::vector<FileInfo> results;
    if (!configManager_) {
        return results;
    }

    std::vector<std::string> allowedDirs = configManager_->getAllowedDirs();
    for (const auto& dir : allowedDirs) {
        if (params.max > 0 && static_cast<int>(results.size()) >= params.max) {
            break;
        }
        searchDirectory(dir, params, results);
    }

    return results;
}

std::vector<FileInfo> FileService::findFilesInPath(const std::string& path, const FindFilesParams& params) {
    std::vector<FileInfo> results;
    if (!configManager_) {
        return results;
    }
    if (policyGuard_ && !policyGuard_->isPathAllowed(path)) {
        return results;
    }
    searchDirectory(path, params, results);
    return results;
}

std::string FileService::readTextFile(const std::string& path) {
    if (policyGuard_ && !policyGuard_->isPathAllowed(path)) {
        throw std::runtime_error("Path not allowed");
    }

    int64_t size = getFileSize(path);
    if (size < 0) {
        throw std::runtime_error("File not found");
    }
    if (size > 200 * 1024) {
        throw std::runtime_error("File too large (max 200KB)");
    }
    if (!isTextFile(path)) {
        throw std::runtime_error("File type not allowed");
    }

    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        throw std::runtime_error("Failed to open file");
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

void FileService::writeTextFile(const std::string& path,
                                const std::string& content,
                                bool overwrite,
                                const std::string& lineEndings) {
    if (path.empty()) {
        throw std::runtime_error("Path required");
    }
    if (policyGuard_ && !policyGuard_->isPathAllowed(path)) {
        throw std::runtime_error("Path not allowed");
    }

    if (!overwrite && pathExistsA(path)) {
        throw std::runtime_error("File already exists");
    }

    std::string dir = getDirNameA(path);
    if (!dir.empty()) {
        // Create intermediate directories if needed.
        std::wstring dirW = Utf8ToWide(dir);
        if (!dirW.empty()) {
            SHCreateDirectoryExW(NULL, dirW.c_str(), NULL);
        }
    }

    std::string normalized = normalizeLineEndings(content, lineEndings);

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        throw std::runtime_error("Failed to open file for write");
    }
    out.write(normalized.data(), (std::streamsize)normalized.size());
    out.close();
    if (!out.good()) {
        throw std::runtime_error("Failed to write file");
    }
}

void FileService::writeBinaryFile(const std::string& path,
                                   const std::string& data,
                                   bool overwrite) {
    if (path.empty()) {
        throw std::runtime_error("Path required");
    }
    if (policyGuard_ && !policyGuard_->isPathAllowed(path)) {
        throw std::runtime_error("Path not allowed");
    }
    if (!overwrite && pathExistsA(path)) {
        throw std::runtime_error("File already exists");
    }

    std::string dir = getDirNameA(path);
    if (!dir.empty()) {
        std::wstring dirW = Utf8ToWide(dir);
        if (!dirW.empty()) {
            SHCreateDirectoryExW(NULL, dirW.c_str(), NULL);
        }
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        throw std::runtime_error("Failed to open file for write");
    }
    out.write(data.data(), (std::streamsize)data.size());
    out.close();
    if (!out.good()) {
        throw std::runtime_error("Failed to write file");
    }
}

std::vector<SearchMatch> FileService::searchTextInFile(const std::string& path,
                                                       const std::string& query) {
    if (query.empty()) {
        throw std::runtime_error("Query required");
    }

    if (policyGuard_ && !policyGuard_->isPathAllowed(path)) {
        throw std::runtime_error("Path not allowed");
    }

    int64_t size = getFileSize(path);
    if (size < 0) {
        throw std::runtime_error("File not found");
    }
    if (size > 200 * 1024) {
        throw std::runtime_error("File too large (max 200KB)");
    }
    if (!isTextFile(path)) {
        throw std::runtime_error("File type not allowed");
    }

    std::ifstream in(path);
    if (!in.is_open()) {
        throw std::runtime_error("Failed to open file");
    }

    std::vector<SearchMatch> matches;
    std::string line;
    int lineNumber = 0;
    std::string queryUtf8 = NormalizeToUtf8(query);
    std::string queryLower = toLower(queryUtf8);
    while (std::getline(in, line)) {
        lineNumber++;
        std::string lineUtf8 = NormalizeToUtf8(line);
        std::string lineLower = toLower(lineUtf8);
        if (lineLower.find(queryLower) != std::string::npos) {
            matches.push_back({lineNumber, lineUtf8});
            if (matches.size() >= 200) {
                break;
            }
        }
    }
    return matches;
}

std::vector<DirectoryEntry> FileService::listDirectory(const std::string& path, std::string* resolvedPath) {
    if (policyGuard_ && !policyGuard_->isPathAllowed(path)) {
        throw std::runtime_error("Path not allowed");
    }

    std::vector<DirectoryEntry> entries;
    
    // 编码保护：确保 path 是有效的 UTF-8
    std::string safePath = path;
    if (!IsValidUtf8(safePath)) {
        safePath = NormalizeToUtf8(safePath);
        DiagLog("[listDirectory] Encoding fixed: '" + path + "' -> '" + safePath + "'");
    }
    
    // 诊断：输出原始路径的 hex
    {
        std::string hex;
        for (unsigned char c : safePath) {
            char buf[8];
            sprintf(buf, "%02X ", c);
            hex += buf;
        }
        DiagLog("[listDirectory] Input path hex: " + hex);
        DiagLog("[listDirectory] Input path str: '" + safePath + "'");
    }
    
    // 规范化路径：将 UTF-8 转为宽字符，用 GetFullPathNameW 解析 .. 等相对路径
    std::string trimmed = trimTrailingSlash(safePath);
    std::wstring inputW = Utf8ToWide(trimmed);
    if (inputW.empty()) {
        throw std::runtime_error("Invalid path encoding (expected UTF-8)");
    }
    
    // 诊断：输出宽字符转换结果
    DiagLog("[listDirectory] Wide len=" + std::to_string(inputW.size()) + " back-to-utf8='" + WideToUtf8(inputW) + "'");
    
    // 用 GetFullPathNameW 解析到绝对路径，消除 ..、. 和相对路径
    wchar_t fullBuf[MAX_PATH] = {0};
    DWORD n = GetFullPathNameW(inputW.c_str(), MAX_PATH, fullBuf, NULL);
    if (n == 0 || n >= MAX_PATH) {
        throw std::runtime_error("Invalid path: cannot resolve full path");
    }
    std::wstring fullPathW(fullBuf, n);
    
    // 去掉尾部反斜杠（但保留盘符根目录如 C:\)
    while (fullPathW.size() > 3 && fullPathW.back() == L'\\') {
        fullPathW.pop_back();
    }
    
    // 诊断：输出 GetFullPathNameW 结果
    std::string resolvedStr = WideToUtf8(fullPathW);
    DiagLog("[listDirectory] Resolved path: '" + resolvedStr + "'");
    
    // 检查目录是否存在
    DWORD attrs = GetFileAttributesW(fullPathW.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        throw std::runtime_error("Directory not found: " + resolvedStr);
    }
    if (!(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        throw std::runtime_error("Path is not a directory: " + resolvedStr);
    }
    
    // 输出规范化后的实际路径
    if (resolvedPath) {
        *resolvedPath = resolvedStr;
    }
    
    std::wstring searchPath = fullPathW + L"\\*";
    DiagLog("[listDirectory] Search pattern: '" + WideToUtf8(searchPath) + "'");

    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        DiagLog("[listDirectory] FindFirstFileW FAILED, error=" + std::to_string(err));
        throw std::runtime_error("Cannot open directory: " + resolvedStr);
    }

    int diagCount = 0;
    do {
        std::string name = WideToUtf8(findData.cFileName);
        if (name == "." || name == "..") {
            continue;
        }
        // 诊断：记录前5个文件名
        if (diagCount < 5) {
            DiagLog("[listDirectory] File[" + std::to_string(diagCount) + "]: '" + name + "'");
            diagCount++;
        }

        bool isDir = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        int64_t size = 0;
        if (!isDir) {
            LARGE_INTEGER li;
            li.HighPart = findData.nFileSizeHigh;
            li.LowPart = findData.nFileSizeLow;
            size = li.QuadPart;
        }

        DirectoryEntry entry;
        entry.name = name;
        entry.type = isDir ? "directory" : "file";
        entry.size = size;
        entry.modified = formatFileTime(findData.ftLastWriteTime);
        entries.push_back(entry);
    } while (FindNextFileW(hFind, &findData));

    FindClose(hFind);
    return entries;
}

bool FileService::isTextFile(const std::string& path) {
    std::string ext = toLower(getFileExtension(path));
    if (ext.empty()) {
        return true;
    }
    static const std::vector<std::string> allowed = {
        ".txt", ".log", ".json", ".md", ".csv", ".yaml", ".yml", ".ini", ".xml"
    };
    return std::find(allowed.begin(), allowed.end(), ext) != allowed.end();
}

int64_t FileService::getFileSize(const std::string& path) {
    WIN32_FILE_ATTRIBUTE_DATA data;
    std::wstring pathW = Utf8ToWide(path);
    if (pathW.empty() || !GetFileAttributesExW(pathW.c_str(), GetFileExInfoStandard, &data)) {
        return -1;
    }
    LARGE_INTEGER li;
    li.HighPart = data.nFileSizeHigh;
    li.LowPart = data.nFileSizeLow;
    return li.QuadPart;
}

std::string FileService::getFileExtension(const std::string& path) {
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) {
        return "";
    }
    return path.substr(dot);
}

std::string FileService::formatFileTime(const FILETIME& ft) {
    SYSTEMTIME st;
    if (!FileTimeToSystemTime(&ft, &st)) {
        return "";
    }
    char buffer[64];
    sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d",
            st.wYear, st.wMonth, st.wDay,
            st.wHour, st.wMinute, st.wSecond);
    return buffer;
}

void FileService::searchDirectory(const std::string& dir,
                                  const FindFilesParams& params,
                                  std::vector<FileInfo>& results) {
    if (params.max > 0 && static_cast<int>(results.size()) >= params.max) {
        return;
    }

    std::string normalized = trimTrailingSlash(dir);
    std::wstring searchPath = Utf8ToWide(normalized + "\\*");
    if (searchPath.empty()) {
        return;
    }
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return;
    }

    FILETIME nowFt;
    GetSystemTimeAsFileTime(&nowFt);
    ULARGE_INTEGER now;
    now.HighPart = nowFt.dwHighDateTime;
    now.LowPart = nowFt.dwLowDateTime;
    const uint64_t dayTicks = 24ULL * 60ULL * 60ULL * 10000000ULL;

    do {
        std::string name = WideToUtf8(findData.cFileName);
        if (name == "." || name == "..") {
            continue;
        }

        std::string fullPath = normalized + "\\" + name;
        bool isDir = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

        if (isDir) {
            searchDirectory(fullPath, params, results);
            if (params.max > 0 && static_cast<int>(results.size()) >= params.max) {
                break;
            }
            continue;
        }

        if (!params.query.empty()) {
            if (toLower(name).find(toLower(params.query)) == std::string::npos) {
                continue;
            }
        }

        if (!params.exts.empty()) {
            std::string ext = toLower(getFileExtension(name));
            bool ok = false;
            for (const auto& allowed : params.exts) {
                if (ext == toLower(allowed)) {
                    ok = true;
                    break;
                }
            }
            if (!ok) {
                continue;
            }
        }

        if (params.days > 0) {
            ULARGE_INTEGER fileTime;
            fileTime.HighPart = findData.ftLastWriteTime.dwHighDateTime;
            fileTime.LowPart = findData.ftLastWriteTime.dwLowDateTime;
            uint64_t age = (now.QuadPart - fileTime.QuadPart) / dayTicks;
            if (age > static_cast<uint64_t>(params.days)) {
                continue;
            }
        }

        LARGE_INTEGER size;
        size.HighPart = findData.nFileSizeHigh;
        size.LowPart = findData.nFileSizeLow;

        FileInfo info;
        info.path = fullPath;
        info.size = size.QuadPart;
        info.modified = formatFileTime(findData.ftLastWriteTime);
        info.extension = getFileExtension(name);
        results.push_back(info);

        if (params.max > 0 && static_cast<int>(results.size()) >= params.max) {
            break;
        }
    } while (FindNextFileW(hFind, &findData));

    FindClose(hFind);
}
