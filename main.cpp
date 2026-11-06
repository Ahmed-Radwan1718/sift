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
        QMainWindow, QWidget#root, QWidget#mainArea, QWidget#homeSurface,
        QWidget#documentPage, QWidget#libraryPage {
            background-color: #232323;
        }
        QFrame#sidebar {
            background-color: #191919;
            border-right: 1px solid #353535;
        }
        QLabel#sidebarSection {
            color: #777777;
            font-size: 10px;
            font-weight: 700;
        }
        QLabel#sidebarFooter {
            color: #626262;
            font-size: 10px;
            font-weight: 600;
        }
        QFrame#sidebarRule {
            background-color: #353535;
            border: none;
        }
        QPushButton#sidebarToggle {
            border: none;
            border-radius: 6px;
            background-color: transparent;
            color: #9b9b9b;
            font-size: 23px;
            padding: 0px;
        }
        QPushButton#sidebarToggle:hover {
            background-color: #303030;
            color: #ffffff;
        }
        QPushButton#sidebarNav {
            background-color: transparent;
            color: #a9a9a9;
            border: 1px solid transparent;
            border-radius: 7px;
            padding: 0px 12px;
            text-align: left;
            font-size: 13px;
            font-weight: 500;
        }
        QPushButton#sidebarNav:hover {
            background-color: #2a2a2a;
            color: #f0f0f0;
        }
        QPushButton#sidebarNav[active="true"] {
            background-color: #343434;
            color: #f2f2f2;
            border-left: 2px solid #dedede;
            font-weight: 600;
        }
        QPushButton#sidebarNav[compact="true"] {
            text-align: center;
            padding: 0px;
        }
        QLabel#headerTitle {
            font-size: 14px;
            font-weight: 600;
            color: #e9e9e9;
        }
        QLabel#libraryTitle {
            color: #eeeeee;
            font-size: 27px;
            font-weight: 650;
        }
        QLabel#libraryDescription {
            color: #a0a0a0;
            font-size: 13px;
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
        QFrame#pdfToolbar {
            background-color: #282828;
            border-bottom: 1px solid #3a3a3a;
        }
        QPushButton#pdfControl {
            background-color: #333333;
            border: 1px solid #454545;
            border-radius: 6px;
            padding: 7px 10px;
        }
        QPushButton#pdfControl:hover:enabled {
            background-color: #434343;
        }
        QPushButton#pdfControl:disabled {
            color: #666666;
            background-color: #2b2b2b;
            border-color: #333333;
        }
        QLabel#pdfToolbarText {
            color: #c8c8c8;
            font-size: 13px;
        }
        QLabel#pdfMessage {
            color: #b6b6b6;
            font-size: 15px;
        }
        QSpinBox#pdfPageInput {
            color: #eeeeee;
            background-color: #343434;
            border: 1px solid #4a4a4a;
            border-radius: 5px;
            padding: 5px;
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
