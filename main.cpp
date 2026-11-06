#include "MainWindow.h"

#include <QApplication>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("Sift"));
    QApplication::setApplicationName(QStringLiteral("Sift"));

    app.setStyleSheet(R"qss(
        QWidget {
            color: #ededed;
            font-family: "Segoe UI";
            font-size: 14px;
        }
        QMainWindow, QWidget#root, QWidget#homeSurface, QWidget#documentPage {
            background-color: #232323;
        }
        QFrame#topBar {
            background-color: #202020;
            border-bottom: 1px solid #353535;
        }
        QLabel#logoMark {
            background-color: #383838;
            border: 1px solid #484848;
            border-radius: 9px;
            font-size: 17px;
            font-weight: 700;
        }
        QLabel#appName {
            font-size: 21px;
            font-weight: 650;
        }
        QLabel#pageTitle {
            font-size: 32px;
            font-weight: 650;
            color: #f5f5f5;
        }
        QLabel#subtitle {
            font-size: 14px;
            color: #a2a2a2;
        }
        QLabel#dropSymbol {
            background-color: #343434;
            border: 1px solid #494949;
            border-radius: 29px;
            font-size: 30px;
            font-weight: 400;
        }
        QFrame#dropZone {
            background-color: #272727;
            border: 1px dashed #505050;
            border-radius: 15px;
        }
        QFrame#dropZone[dragActive="true"] {
            background-color: #303030;
            border: 2px dashed #bdbdbd;
        }
        QLabel#dropHeading {
            color: #e3e3e3;
            font-size: 16px;
            font-weight: 600;
        }
        QLabel#hint, QLabel#supported, QLabel#smallText {
            color: #929292;
            font-size: 12px;
        }
        QLabel#sectionTitle {
            font-size: 15px;
            font-weight: 600;
        }
        QPushButton#primaryButton {
            background-color: #e9e9e9;
            color: #202020;
            border: 1px solid #e9e9e9;
            border-radius: 8px;
            padding: 10px 22px;
            font-weight: 600;
        }
        QPushButton#primaryButton:hover {
            background-color: #ffffff;
            border-color: #ffffff;
        }
        QPushButton#primaryButton:focus {
            border: 2px solid #a6bdda;
        }
        QPushButton#subtleButton {
            background-color: transparent;
            border: 1px solid #444444;
            border-radius: 8px;
            padding: 8px 14px;
        }
        QPushButton#subtleButton:hover {
            background-color: #333333;
        }
        QPushButton#recentItem {
            background-color: transparent;
            border: none;
            border-radius: 7px;
            padding: 12px 8px;
            text-align: left;
        }
        QPushButton#recentItem:hover, QPushButton#recentItem:focus {
            background-color: #343434;
        }
        QLabel#extensionTag {
            background-color: #353535;
            color: #b9b9b9;
            padding: 5px 9px;
            border-radius: 5px;
            font-size: 11px;
        }
        QFrame#rowDivider {
            border: none;
            background-color: #353535;
        }
        QScrollArea, QScrollArea > QWidget > QWidget {
            background-color: #232323;
            border: none;
        }
        QTabWidget::pane {
            border: none;
            background-color: #232323;
        }
        QTabBar::tab {
            background-color: #292929;
            color: #aaaaaa;
            padding: 11px 24px 11px 14px;
            border-right: 1px solid #3b3b3b;
            min-width: 90px;
        }
        QTabBar::tab:selected {
            background-color: #343434;
            color: #ffffff;
        }
        QTabBar::tab:hover:!selected {
            background-color: #303030;
        }
        QScrollBar:vertical {
            background: #232323;
            width: 10px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: #515151;
            border-radius: 5px;
            min-height: 24px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
    )qss");

    MainWindow window;
    window.resize(1100, 720);
    window.setMinimumSize(700, 480);

#ifdef Q_OS_WIN
    // Older MinGW headers do not expose the named Windows 11 DWM constant.
    const BOOL useDarkTitleBar = TRUE;
    constexpr DWORD darkTitleBarAttribute = 20;
    DwmSetWindowAttribute(reinterpret_cast<HWND>(window.winId()),
                          darkTitleBarAttribute,
                          &useDarkTitleBar,
                          sizeof(useDarkTitleBar));
#endif

    window.show();
    return app.exec();
}
