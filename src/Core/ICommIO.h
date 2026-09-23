#ifndef ICOMMIO_H
#define ICOMMIO_H

/**
 * @file ICommIO.h
 * @brief 外部通信 IO 抽象接口（TCP/PLC/串口等，V2+ 扩展点）。
 *
 * RunEngine 或主界面在测量完成后可调用 sendResult 将 OK/NG 或数值上报。
 * 当前默认注入 NullCommIO，调用返回「尚未实现」。
 */

#include <QString>

namespace Me {

/**
 * 通信设备抽象。
 * 生命周期：connectDevice → sendResult（可多次）→ disconnectDevice。
 */
class ICommIO
{
public:
    virtual ~ICommIO() = default;

    virtual QString name() const = 0;
    virtual bool connectDevice(QString *error = nullptr) = 0;
    virtual void disconnectDevice() = 0;
    /** @param payload 通常为 JSON 或自定义协议字符串 */
    virtual bool sendResult(const QString &payload, QString *error = nullptr) = 0;
};

/** 空实现占位，避免主流程空指针判断 */
class NullCommIO : public ICommIO
{
public:
    QString name() const override { return QStringLiteral("Null"); }

    bool connectDevice(QString *error = nullptr) override
    {
        if (error) {
            *error = QStringLiteral("通信模块尚未实现");
        }
        return false;
    }

    void disconnectDevice() override {}

    bool sendResult(const QString &payload, QString *error = nullptr) override
    {
        Q_UNUSED(payload);
        if (error) {
            *error = QStringLiteral("通信模块尚未实现");
        }
        return false;
    }
};

} // namespace Me

#endif // ICOMMIO_H
