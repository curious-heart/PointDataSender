#ifndef LOGGER_H
#define LOGGER_H

/*
 * 从条纹相机上位机程序拷贝修改。
 */

#include <QObject>
#include <QThread>

#define LOG_LEVEL_ITEM(lvl) LOG_##lvl
#define LOG_LEVEL_LIST \
    LOG_LEVEL_ITEM(DEBUG),\
    LOG_LEVEL_ITEM(INFO),\
    LOG_LEVEL_ITEM(WARN),\
    LOG_LEVEL_ITEM(ERROR)

enum LOG_LEVEL {
    LOG_LEVEL_LIST
};
#define VALID_LOG_LVL(lvl) ((LOG_DEBUG <= (lvl)) && ((lvl) <= LOG_ERROR))
Q_DECLARE_METATYPE(LOG_LEVEL)

class Logger : public QObject
{
    Q_OBJECT
public:
    explicit Logger(QObject *parent = nullptr);

public slots:
    //void receive_log(LOG_LEVEL level, QString loc_str, QString log_str);
    void receive_log(int level, QString loc_str, QString log_str);
};

class LogSigEmitter: public QObject
{
    Q_OBJECT
signals:
    //void record_log(LOG_LEVEL level, QString loc_str, QString log_str);
    void record_log(int level, QString loc_str, QString log_str);
};
extern LogSigEmitter *g_LogSigEmitter;

/* !!!
 * DO NOT call this function directly. It is defined just for warning-elimination.
 * Use DIY_LOG macro.
*/
//void __emit_log_signal__(LOG_LEVEL level, QString loc_str, QString log);
void __emit_log_signal__(int level, QString loc_str, QString log);


/*
 * After start_log_thread is invoked with a QThread instance can DIY_LOG work.
 * Note: the life-cycle of th must expands to the whole thread.
 * E.g. you can define a QThread obj in main function, and call start/end_log_thread
 * with that obj, as below:
 *
    QThread log_thread;
    start_log_thread(log_thread);
    ....
    end_log_thread(log_thread);
 *
 * start_log_thread should be invoked as early as possible and end_log_thread
 * should be invoked as late as possible. Place them at proper position in your
 * program.
 *
*/
bool start_log_thread(QThread &th, LOG_LEVEL display_lvl = LOG_INFO);
void end_log_thread(QThread &th);
void update_log_level(LOG_LEVEL display_lvl);

extern const char* g_log_level_strs[];

/*DO NOT call this function, just use DIY_LOG macro.*/
void writeLog(int level, QString loc_str, QString msg);

/*
 * Use example:
 *     DIY_LOG(LOG_LEVEL::LOG_INFO, "info str.");
 *     QString err_str;
 *     DIY_LOG(LOG_LEVEL::LOG_WARN, err_str);
*/
#define DIY_LOG(level, log) \
    {\
        QString loc_str = QString(__FILE__) + QString("  [%1][%2]").arg(__LINE__).arg(__FUNCTION__);\
        if(g_LogSigEmitter)\
        {\
            __emit_log_signal__((int)level, loc_str, (log));\
        }\
        else\
        {\
            writeLog((level), loc_str, (log));\
        }\
    }

extern const char* log_dir_str, *log_file_str;
/*
 * Use example:
 *     DIY_LOG(LOG_LEVEL::LOG_INFO, "info str.");
 *     DIY_LOG(LOG_LEVEL::LOG_ERROR, "error code:%d", (int)err);
 *
 *     QString ws("warning...");
 *     DIY_LOG(LOG_LEVEL::LOG_WARN, "warn message:%ls", ws.utf16());
*/
/*
#define DIY_LOG(level, fmt_str, ...) \
    {\
        QString log = QString::asprintf(fmt_str, ##__VA_ARGS__);\
        QString loc_str = QString(__FILE__) + QString("  [%1]").arg(__LINE__);\
        if(g_LogSigEmitter)\
        {\
            __emit_log_signal__((int)level, loc_str, log);\
        }\
    }
*/
#endif // LOGGER_H
