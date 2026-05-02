#include <QApplication>
#include <QCoreApplication>

#include "MainWindow.h"
#include "TestManager.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("QtTren");
    QCoreApplication::setApplicationName("QtTren");

    TestManager testManager;
    testManager.loadTests();

    MainWindow window(&testManager);
    window.resize(1220, 780);
    window.show();

    return QApplication::exec();
}
