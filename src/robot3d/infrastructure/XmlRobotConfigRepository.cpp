#include "robot3d/infrastructure/XmlRobotConfigRepository.h"
#include <QDir>
#include <QFile>
#include <QXmlStreamReader>
#include <QRegularExpression>
#include <cmath>
namespace robot3d
{
namespace infrastructure
{
using domain::RobotConfigurationPtr;
typedef core::Result<RobotConfigurationPtr> LoadResult;
XmlRobotConfigRepository::XmlRobotConfigRepository(const std::string &root)
    : root_(QDir(QString::fromStdString(root)).absolutePath().toStdString())
{
}
std::vector<std::string> XmlRobotConfigRepository::robotTypes() const
{
    std::vector<std::string> result;
    // Keep the original demo's default and ordering while discovering other valid directories.
    const QStringList preferred = {"G6L-3-20", "G6-1-1", "X20-1-3"};
    QDir root(QString::fromStdString(root_));
    for (const auto &id : preferred)
        if (QFile::exists(root.filePath(id + "/config.xml")))
            result.push_back(id.toStdString());
    for (const auto &id : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
        if (!preferred.contains(id) && QFile::exists(root.filePath(id + "/config.xml")))
            result.push_back(id.toStdString());
    return result;
}
LoadResult XmlRobotConfigRepository::load(const std::string &id)
{
    const QString name = QString::fromStdString(id);
    if (!QRegularExpression("^[A-Za-z0-9_-]+$").match(name).hasMatch())
        return LoadResult::failure("非法机型标识");
    QDir root(QString::fromStdString(root_));
    QFile file(root.filePath(name + "/config.xml"));
    if (!file.open(QIODevice::ReadOnly))
        return LoadResult::failure("无法打开机型配置: " + file.fileName().toStdString());
    std::shared_ptr<domain::RobotConfiguration> config(new domain::RobotConfiguration());
    config->id = id;
    config->modelDirectory = root.absoluteFilePath(name + "/model3d").toStdString();
    QXmlStreamReader xml(&file);
    bool found = false;
    int count = 0;
    while (!xml.atEnd())
    {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == QLatin1String("DHpara"))
        {
            if (found)
                return LoadResult::failure("DH 参数重复");
            found = true;
            while (xml.readNextStartElement())
            {
                if (count >= 6 || xml.name() != QString("j%1").arg(count))
                    return LoadResult::failure("DH 轴序或数量错误");
                const auto parts = xml.readElementText().trimmed().split(',');
                if (parts.size() != 5)
                    return LoadResult::failure("DH 参数需要五个数值");
                double v[5];
                for (int k = 0; k < 5; ++k)
                {
                    bool ok = false;
                    v[k] = parts[k].toDouble(&ok);
                    if (!ok || !std::isfinite(v[k]))
                        return LoadResult::failure("DH 数值无效");
                }
                config->dh[count++] = {v[0], v[1], v[2], v[3], v[4]};
            }
        }
    }
    if (xml.hasError())
        return LoadResult::failure("XML 解析失败: " + xml.errorString().toStdString());
    if (!found || count != 6)
        return LoadResult::failure("机型配置必须包含六轴 DH 参数");
    return LoadResult::success(config);
}
}
}
