#ifndef HALCONUTILS_H
#define HALCONUTILS_H

/**
 * @file HalconUtils.h
 * @brief HALCON 异常与 C 字符串的安全转换工具（header-only）。
 *
 * HALCON 的 HString 在不同平台/版本下可能返回 UTF-8 或本地编码；
 * safeFromCString 限制扫描长度，避免异常指针导致 QString 断言。
 */

#include <QString>

#include "HalconCpp.h"

namespace Me {
namespace {

/**
 * 从 HALCON 返回的 char* 构造 QString。
 * @param utf8 true 时用 fromUtf8，否则 fromLocal8Bit
 */
inline QString safeFromCString(const char *text, bool utf8)
{
    if (!text) {
        return {};
    }
    // 限制扫描长度，避免野指针/非 \0 结尾导致 QString::fromUtf8 触发 MaxSize 断言
    constexpr qsizetype kMaxChars = 4096;
    qsizetype n = 0;
    while (n < kMaxChars && text[n] != '\0') {
        ++n;
    }
    if (n <= 0) {
        return {};
    }
    return utf8 ? QString::fromUtf8(text, n) : QString::fromLocal8Bit(text, n);
}

} // namespace

/** 将 HException 转为可展示的 QString（优先 UTF-8 ErrorMessage） */
inline QString fromHException(const HalconCpp::HException &ex)
{
    try {
        const int code = static_cast<int>(ex.ErrorCode());
        const HalconCpp::HString msg = ex.ErrorMessage();
        QString text = safeFromCString(msg.ToUtf8(), true);
        if (text.isEmpty()) {
            text = safeFromCString(msg.TextA(), false);
        }
        if (text.isEmpty()) {
            return QStringLiteral("HALCON error (%1)").arg(code);
        }
        return text;
    } catch (...) {
        return QStringLiteral("HALCON error");
    }
}

} // namespace Me

#endif // HALCONUTILS_H
