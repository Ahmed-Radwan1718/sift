#include <QApplication>
#include <QColor>
#include <QMainWindow>
#include <QPalette>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QMainWindow window;
    window.setWindowTitle(QStringLiteral("Sift"));
    window.resize(1100, 720);
    window.setMinimumSize(640, 400);

    QPalette palette = window.palette();
    palette.setColor(QPalette::Window, QColor("#262626"));
    window.setPalette(palette);
    window.setAutoFillBackground(true);

#ifdef Q_OS_WIN
    // Match the Windows 11 title bar to the dark application window.
    const BOOL darkTitleBar = TRUE;
    // Attribute 20 enables the dark title bar on Windows 10/11.
    // Use the numeric value because older MinGW headers may not define its name.
    constexpr DWORD darkTitleBarAttribute = 20;
    DwmSetWindowAttribute(
        reinterpret_cast<HWND>(window.winId()),
        darkTitleBarAttribute,
        &darkTitleBar,
        sizeof(darkTitleBar));
#endif

    window.show();
    return app.exec();
}
