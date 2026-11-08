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
        QFrame#sidebarRule {
            background-color: #353535;
            border: none;
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
        QToolButton#documentSidebarToggle {
            background-color: transparent;
            border: none;
            border-radius: 7px;
            padding: 0px;
        }
        QToolButton#documentSidebarToggle:hover {
            background-color: #353535;
        }
        QToolButton#documentSidebarToggle:pressed {
            background-color: #454545;
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
        QLabel#dashboardTitle {
            font-size: 26px;
            font-weight: 650;
            color: #f2f2f2;
        }
        QLabel#dashboardSubtitle {
            font-size: 13px;
            color: #969696;
        }
        QLabel#dashboardSection {
            font-size: 15px;
            font-weight: 600;
            color: #ededed;
        }
        QLabel#dashboardCount {
            font-size: 12px;
            color: #777777;
        }
        QLineEdit#documentSearch {
            color: #eeeeee;
            background-color: #2c2c2c;
            border: 1px solid #404040;
            border-radius: 8px;
            padding: 10px 13px;
            selection-background-color: #515151;
        }
        QLineEdit#documentSearch:hover {
            border-color: #565656;
        }
        QLineEdit#documentSearch:focus {
            border: 1px solid #8b8b8b;
        }
        QPushButton#typeFilter {
            background-color: #2c2c2c;
            color: #b4b4b4;
            border: 1px solid #363636;
            border-radius: 7px;
            padding: 6px 13px;
            font-size: 12px;
        }
        QPushButton#typeFilter:hover {
            background-color: #373737;
            color: #f0f0f0;
        }
        QPushButton#typeFilter[selected="true"] {
            background-color: #484848;
            color: #ffffff;
            border: 1px solid #5a5a5a;
        }
        QStackedWidget#dashboardResults, QWidget#emptyLibrary {
            background-color: #232323;
        }
        QTreeWidget#documentTable {
            background-color: #232323;
            color: #e4e4e4;
            border: none;
            outline: none;
            font-size: 13px;
            show-decoration-selected: 1;
        }
        QTreeWidget#documentTable::item {
            padding: 4px 8px;
            border-bottom: 1px solid #343434;
        }
        QTreeWidget#documentTable::item:hover {
            background-color: #303030;
        }
        QTreeWidget#documentTable::item:selected {
            background-color: #383838;
            color: #ffffff;
        }
        QHeaderView::section {
            background-color: #232323;
            color: #888888;
            border: none;
            border-bottom: 1px solid #414141;
            padding: 12px 8px;
            font-size: 10px;
            font-weight: 600;
        }
        QToolButton#favoriteStar {
            background: transparent;
            color: #858585;
            border: none;
            border-radius: 6px;
            font-size: 23px;
        }
        QToolButton#favoriteStar:hover {
            background: #3b3b3b;
            color: #ffffff;
        }
        QToolButton#favoriteStar[starred="true"] {
            color: #e5d39a;
        }
        QLabel#emptyTitle {
            font-size: 18px;
            font-weight: 600;
            color: #e5e5e5;
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
        QFrame#floatingPdfToolbar {
            background-color: #323232;
            border: 1px solid #505050;
            border-radius: 22px;
        }
        QFrame#floatingPdfDivider {
            background-color: #595959;
            border: none;
        }
        QPushButton#pdfControl {
            color: #e6e6e6;
            background-color: transparent;
            border: none;
            border-radius: 7px;
            padding: 0;
            font-size: 17px;
        }
        QPushButton#pdfControl:hover:enabled {
            background-color: #494949;
        }
        QPushButton#pdfControl:pressed:enabled {
            background-color: #5a5a5a;
        }
        QPushButton#pdfControl:disabled {
            color: #777777;
        }
        QLabel#floatingPdfText {
            color: #d9d9d9;
            font-size: 12px;
        }
        QLineEdit#floatingPdfPageInput {
            background-color: #414141;
            color: #f0f0f0;
            border: 1px solid #555555;
            border-radius: 6px;
            padding: 0 3px;
            font-size: 12px;
        }
        QLineEdit#floatingPdfPageInput:focus {
            border: 1px solid #929292;
        }
        QLabel#pdfMessage {
            color: #b6b6b6;
            font-size: 15px;
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
