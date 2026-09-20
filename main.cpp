#include <QApplication>
#include <QSurfaceFormat>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDebug>

#include "demoui.h"

/**
 * @brief 机械臂 3D 显示模块移植 Demo 入口
 *
 * 运行前请确保可执行文件所在目录存在：
 *   - robotType/   机型模型与 config.xml（按相对路径读取）
 *   - libassimp.dll / Qt5*.dll / MinGW 运行库
 *
 * 启动后会在当前目录生成 demo_debug.log，记录模型加载与运行信息。
 */

static QTextStream *g_logStream = nullptr;

static void demoMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    Q_UNUSED(context)
    static const char *levelStr[] = {"DEBUG", "WARN ", "ERROR", "FATAL", "INFO "};
    const QString line = QString("[%1][%2] %3")
            .arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz"))
            .arg(levelStr[type])
            .arg(msg);

    if (g_logStream != nullptr) {
        *g_logStream << line << "\r\n";
        g_logStream->flush();
    }
    fprintf(stderr, "%s\n", line.toLocal8Bit().constData());
    fflush(stderr);
}

int main(int argc, char *argv[])
{
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication app(argc, argv);

    QFile logFile("demo_debug.log");
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        g_logStream = new QTextStream(&logFile);
    }
    qInstallMessageHandler(demoMessageHandler);

    qDebug() << "===================================================";
    qDebug() << "机械臂 3D 显示模块移植 Demo 启动";
    qDebug() << "当前工作目录:" << QDir::currentPath();
    qDebug() << "robotType 目录存在:" << QDir("robotType").exists();
    qDebug() << "libassimp.dll 存在:" << QFile::exists("libassimp.dll");

    DemoUi w;
    w.resize(1280, 800);
    w.show();

    const int ret = app.exec();
    qDebug() << "程序退出，返回码:" << ret;
    return ret;
}
