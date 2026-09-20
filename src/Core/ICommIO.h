#ifndef ICOMMIO_H
#define ICOMMIO_H

#include <QString>

namespace Me {

// 通信 IO 空接口（TCP/PLC 等，V2+）
class ICommIO
{
public:
    virtual ~ICommIO() = default;

    virtual QString name() const = 0;
    virtual bool connectDevice(QString *error = nullptr) = 0;
    virtual void disconnectDevice() = 0;
    virtual bool sendResult(const QString &payload, QString *error = nullptr) = 0;
};

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
