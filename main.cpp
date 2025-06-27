#include "mainwindow.h"
#include "version_def/version_def.h"

#include <QApplication>
#include <QThread>
#include <QString>
#include <QMessageBox>

#include "logger/logger.h"
#include "sysconfigs/sysconfigs.h"

int main(int argc, char *argv[])
{
    if(QT_VERSION>=QT_VERSION_CHECK(5,6,0))
            QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    /*init define app.*/
    QApplication a(argc, argv);

    /*initialization: start log thread. Now use the default log-level defined in logger mode.*/
    QThread th;
    int ret;
    start_log_thread(th);

    /*initialization: load configs.*/
    QString ret_str;
    bool log_cfg_ret = fill_sys_configs(&ret_str);
    if(!log_cfg_ret)
    {
        end_log_thread(th);

        QMessageBox::critical(nullptr, "", ret_str);
        return -1;
    }
    //use the log level defined in config file.
    update_log_level((LOG_LEVEL)(g_sys_configs_block.log_level));

    /*create main window.*/
    MainWindow w;
    QString title_str = QString("%1 %2").arg(a.applicationName(), APP_VER_STR);
    w.setWindowTitle(title_str);

    w.show();
    ret = a.exec();

    end_log_thread(th);

    return ret;
}
