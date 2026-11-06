#pragma once

#include <QMainWindow>
#include <QStringList>

class DocumentDashboard;
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
    QWidget *createFavoritesPage();
    QWidget *createPlaceholderPage(const QString &title,
                                   const QString &description);
    QWidget *createDocumentPage(const QString &filePath);
    void showSection(const QString &section);
    void updateCurrentTabHeader();
    void openFilesDialog();
    void openFile(const QString &filePath);
    void addToRecentFiles(const QString &filePath);
    void setFavorite(const QString &filePath, bool favorite);
    void refreshDashboards();
    void refreshTabVisibility();
    bool isSupported(const QString &filePath) const;
    QStringList recentFiles() const;

    Sidebar *m_sidebar = nullptr;
    QStackedWidget *m_mainStack = nullptr;
    QTabWidget *m_tabs = nullptr;
    DocumentDashboard *m_homeDashboard = nullptr;
    DocumentDashboard *m_recentDashboard = nullptr;
    DocumentDashboard *m_favoritesDashboard = nullptr;
    QLabel *m_headerTitle = nullptr;
    QPushButton *m_headerOpen = nullptr;
};
