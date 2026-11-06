#pragma once

#include <QMainWindow>
#include <QStringList>

class QDragEnterEvent;
class QDragLeaveEvent;
class QDragMoveEvent;
class QDropEvent;
class QFrame;
class QPushButton;
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
    QWidget *createDocumentPage(const QString &filePath);
    void openFilesDialog();
    void openFile(const QString &filePath);
    void addToRecentFiles(const QString &filePath);
    void refreshRecentFiles();
    void refreshTabVisibility();
    void setDropHighlight(bool active);
    bool isSupported(const QString &filePath) const;
    QStringList recentFiles() const;

    QTabWidget *m_tabs = nullptr;
    QVBoxLayout *m_recentItems = nullptr;
    QPushButton *m_headerHome = nullptr;
    QPushButton *m_headerOpen = nullptr;
    QFrame *m_dropZone = nullptr;
};
