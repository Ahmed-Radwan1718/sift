#pragma once

#include <QMainWindow>
#include <QStringList>

class QDragEnterEvent;
class QDragLeaveEvent;
class QDragMoveEvent;
class QDropEvent;
class QFrame;
class QLabel;
class QPushButton;
class QStackedWidget;
class Sidebar;
class QTabWidget;
class QVBoxLayout;
class QWidget;

class MainWindow final : public QMainWindow
{
public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    QWidget *createHomePage();
    QWidget *createRecentPage();
    QWidget *createPlaceholderPage(const QString &title,
                                   const QString &description);
    QWidget *createDocumentPage(const QString &filePath);
    void showSection(const QString &section);
    void updateCurrentTabHeader();
    void openFilesDialog();
    void openFile(const QString &filePath);
    void addToRecentFiles(const QString &filePath);
    void refreshRecentFiles();
    void refreshTabVisibility();
    void setDropHighlight(bool active);
    bool isSupported(const QString &filePath) const;
    QStringList recentFiles() const;

    Sidebar *m_sidebar = nullptr;
    QStackedWidget *m_mainStack = nullptr;
    QTabWidget *m_tabs = nullptr;
    QVBoxLayout *m_recentItems = nullptr;
    QVBoxLayout *m_recentSectionItems = nullptr;
    QLabel *m_headerTitle = nullptr;
    QPushButton *m_headerOpen = nullptr;
    QFrame *m_dropZone = nullptr;
};
