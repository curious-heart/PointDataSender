#include "mainwindow.h"
#include "version_def/version_def.h"

#include <QApplication>
#include <QThread>

#include "logger/logger.h"
#include "sysconfigs/sysconfigs.h"

int main(int argc, char *argv[])
{
    if(QT_VERSION>=QT_VERSION_CHECK(5,6,0))
            QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QApplication a(argc, argv);
    MainWindow w;

    if(!w.init_ok()) return -1;

    QThread th;
    int ret;
    start_log_thread(th, (LOG_LEVEL)(g_sys_configs_block.log_level));

    QString title_str = QString("%1 %2").arg(a.applicationName(), APP_VER_STR);
    w.setWindowTitle(title_str);

    w.show();
    ret = a.exec();

    end_log_thread(th);

    return ret;
}
