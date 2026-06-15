#ifndef CLAWDESK_ENCODING_UTILS_H
#define CLAWDESK_ENCODING_UTILS_H

#include <string>

// Convert a UTF-16 wide string to UTF-8.
std::string WideToUtf8(const std::wstring& wide);

// Convert a UTF-8 string to UTF-16 wide string.
std::wstring Utf8ToWide(const std::string& utf8);

// Convert current Windows ANSI code page bytes to UTF-8.
std::string AnsiToUtf8(const std::string& ansi);

// Check whether bytes are valid UTF-8.
bool IsValidUtf8(const std::string& input);

// Keep input if already UTF-8; otherwise decode as ANSI and convert to UTF-8.
std::string NormalizeToUtf8(const std::string& input);

// Convert console output bytes (UTF-16LE/UTF-8/ANSI) to UTF-8.
std::string NormalizeConsoleOutputToUtf8(const std::string& input);

#endif // CLAWDESK_ENCODING_UTILS_H
