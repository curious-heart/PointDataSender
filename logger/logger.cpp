#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QDateTime>
#include <QDebug>
#include <QString>

#include "logger.h"
#include "version_def/version_def.h"

LogSigEmitter *g_LogSigEmitter = nullptr;
static Logger * g_LogWorker = nullptr;
static LOG_LEVEL gs_log_display_levle = LOG_INFO;
Logger::Logger(QObject *parent) : QObject(parent)
{

}

/*
 * According to Qt help manual
 * [
 *  Threads and QObjects:
 *  In general, creating QObjects before the QApplication is not supported and can
 *  lead to weird crashes on exit, depending on the platform. This means static instances
 *  of QObject are also not supported. A properly structured single or multi-threaded
 *  application should make the QApplication be the first created, and last destroyed
 *  QObject.
 *  ]
 *  This may applies to GUI classes. But for safety, we decide to obey this rule strictly.
 *  So we do not use static Logger any more...
 *  For the same reason, we define g_LogSigEmitter as a pointer and new an object for it
 *  after QApplication object is created in main function.
Logger *Logger::instance()
{
    static Logger logger;
    return &logger;
}
*/

/**
 * @param level_str 级别字符串
 * @param loc_str 位置字符串，包括文件名和行号
 * @param log 日志内容
 */
const char* log_dir_str = "./log", *log_file_str = "log";
void writeLog(int level, QString loc_str, QString msg)
{
    QString level_str;

    if(level < gs_log_display_levle)
    {
        return;
    }

    if(!VALID_LOG_LVL(level))
    {
        level_str = QString("[%1]").arg("Unknown Level");
    }
    else
    {
        level_str = QString("[%1]").arg(g_log_level_strs[level]);
    }

    QFileInfo info(log_dir_str);
    QString path = info.absoluteFilePath();
    QDir dir(path);
    if(!dir.exists())
        dir.mkpath(path);
    QString date=QDateTime::currentDateTime().toString("yyyy-MM-dd");
    QString time=QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QFile file(QString(log_dir_str) + QString("/")
               + QString(log_file_str) + QString("_%1.log").arg(date));
    if(!file.open(QFile::WriteOnly | QFile::Append))
        return;
    QTextStream in(&file);
    QString str1, str2;
    str1 = "\t" + loc_str + "\t@" + APP_VER_STR + "\n";
    str2 = date + " " + time + " " + level_str + " " + msg + "\n";
    in << str1 << str2;
    file.flush();
    file.close();

    qDebug() << str1 << str2;
}
//void Logger::receive_log(LOG_LEVEL level, QString loc_str, QString log_str)
#undef LOG_LEVEL_ITEM
#define LOG_LEVEL_ITEM(lvl) #lvl
const char* g_log_level_strs[] =
{
    LOG_LEVEL_LIST
};
void Logger::receive_log(int level, QString loc_str, QString log_str)
{
    writeLog(level, loc_str, log_str);
}

bool start_log_thread(QThread &th, LOG_LEVEL display_lvl)
{
    gs_log_display_levle = display_lvl;

    qRegisterMetaType<LOG_LEVEL>();
    g_LogSigEmitter = new LogSigEmitter;
    g_LogWorker = new Logger;
    if(g_LogSigEmitter && g_LogWorker)
    {
        g_LogWorker->moveToThread(&th);
        QObject::connect(&th, &QThread::finished, g_LogWorker, &QObject::deleteLater);
        QObject::connect(g_LogSigEmitter, &LogSigEmitter::record_log,
                         g_LogWorker , &Logger::receive_log,
                         Qt::QueuedConnection);
        th.start();
        return true;
    }
    else
    {
        return false;
    }
}
void end_log_thread(QThread &th)
{
    /*
     * emitter is managed by main thread,
     * and worker is managed by log thread (th).
    */
    if(g_LogSigEmitter && g_LogWorker)
    {
        QObject::disconnect(g_LogSigEmitter, &LogSigEmitter::record_log,
                         g_LogWorker , &Logger::receive_log);
        th.quit();
        th.wait();
        g_LogWorker = nullptr;
        delete g_LogSigEmitter; g_LogSigEmitter = nullptr;
    }
    if(g_LogSigEmitter)
    {
        delete g_LogSigEmitter; g_LogSigEmitter = nullptr;
    }
}

void update_log_level(LOG_LEVEL display_lvl)
{
    gs_log_display_levle = display_lvl;
}

//void __emit_log_signal__(LOG_LEVEL level, QString loc_str, QString log)
void __emit_log_signal__(int level, QString loc_str, QString log)
{
    if(g_LogSigEmitter)
    {
        emit g_LogSigEmitter->record_log(level, loc_str, log);
    }
}
