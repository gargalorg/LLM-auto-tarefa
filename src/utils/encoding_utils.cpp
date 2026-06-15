#include "utils/encoding_utils.h"
#include <windows.h>

namespace {
std::string Utf16LeBytesToUtf8(const std::string& input) {
    if (input.empty()) {
        return "";
    }

    size_t offset = 0;
    if (input.size() >= 2 &&
        static_cast<unsigned char>(input[0]) == 0xFF &&
        static_cast<unsigned char>(input[1]) == 0xFE) {
        offset = 2;
    }

    const size_t usableBytes = (input.size() - offset) & ~static_cast<size_t>(1);
    if (usableBytes == 0) {
        return "";
    }

    std::wstring wide;
    wide.reserve(usableBytes / 2);
    for (size_t i = offset; i < offset + usableBytes; i += 2) {
        wchar_t ch = static_cast<wchar_t>(
            static_cast<unsigned char>(input[i]) |
            (static_cast<unsigned char>(input[i + 1]) << 8));
        wide.push_back(ch);
    }

    return WideToUtf8(wide);
}
} // namespace

std::string WideToUtf8(const std::wstring& wide) {
    if (wide.empty()) {
        return "";
    }
    int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, NULL, 0, NULL, NULL);
    if (size <= 0) {
        return "";
    }
    std::string out(static_cast<size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &out[0], size, NULL, NULL);
    return out;
}

std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) {
        return L"";
    }
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, NULL, 0);
    if (size <= 0) {
        return L"";
    }
    std::wstring out(static_cast<size_t>(size - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &out[0], size);
    return out;
}

std::string AnsiToUtf8(const std::string& ansi) {
    if (ansi.empty()) {
        return "";
    }
    int wideSize = MultiByteToWideChar(CP_ACP, 0, ansi.c_str(), -1, NULL, 0);
    if (wideSize <= 0) {
        return "";
    }
    std::wstring wide(static_cast<size_t>(wideSize - 1), L'\0');
    MultiByteToWideChar(CP_ACP, 0, ansi.c_str(), -1, &wide[0], wideSize);
    return WideToUtf8(wide);
}

bool IsValidUtf8(const std::string& input) {
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(input.data());
    const size_t len = input.size();
    size_t i = 0;

    while (i < len) {
        unsigned char c = bytes[i];
        if (c <= 0x7F) {
            i++;
            continue;
        }

        if ((c & 0xE0) == 0xC0) {
            if (i + 1 >= len) return false;
            unsigned char c1 = bytes[i + 1];
            if ((c1 & 0xC0) != 0x80) return false;
            if (c < 0xC2) return false;
            i += 2;
            continue;
        }

        if ((c & 0xF0) == 0xE0) {
            if (i + 2 >= len) return false;
            unsigned char c1 = bytes[i + 1];
            unsigned char c2 = bytes[i + 2];
            if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80) return false;
            if (c == 0xE0 && c1 < 0xA0) return false;
            if (c == 0xED && c1 >= 0xA0) return false;
            i += 3;
            continue;
        }

        if ((c & 0xF8) == 0xF0) {
            if (i + 3 >= len) return false;
            unsigned char c1 = bytes[i + 1];
            unsigned char c2 = bytes[i + 2];
            unsigned char c3 = bytes[i + 3];
            if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80) return false;
            if (c == 0xF0 && c1 < 0x90) return false;
            if (c > 0xF4) return false;
            if (c == 0xF4 && c1 > 0x8F) return false;
            i += 4;
            continue;
        }

        return false;
    }

    return true;
}

std::string NormalizeToUtf8(const std::string& input) {
    if (input.empty()) {
        return "";
    }
    if (IsValidUtf8(input)) {
        return input;
    }
    return AnsiToUtf8(input);
}

std::string NormalizeConsoleOutputToUtf8(const std::string& input) {
    if (input.empty()) {
        return "";
    }

    const bool hasUtf16LeBom = input.size() >= 2 &&
        static_cast<unsigned char>(input[0]) == 0xFF &&
        static_cast<unsigned char>(input[1]) == 0xFE;
    const bool containsNul = input.find('\0') != std::string::npos;

    if (hasUtf16LeBom || (containsNul && (input.size() % 2 == 0))) {
        std::string utf8 = Utf16LeBytesToUtf8(input);
        if (!utf8.empty()) {
            return utf8;
        }
    }

    return NormalizeToUtf8(input);
}
