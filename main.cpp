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
    DwmSetWindowAttribute(
        reinterpret_cast<HWND>(window.winId()),
        DWMWA_USE_IMMERSIVE_DARK_MODE,
        &darkTitleBar,
        sizeof(darkTitleBar));
#endif

    window.show();
    return app.exec();
}
