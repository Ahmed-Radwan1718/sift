#include "MainWindow.h"
#include "DocumentDashboard.h"
#include "PdfViewerPage.h"
#include "Sidebar.h"

#include <QDateTime>
#include <QDir>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QStackedWidget>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

namespace {
Qt::CaseSensitivity filePathComparison()
{
#ifdef Q_OS_WIN
    return Qt::CaseInsensitive;
#else
    return Qt::CaseSensitive;
#endif
}

QLabel *centeredLabel(const QString &text, const char *styleName,
                      QWidget *parent = nullptr)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QString::fromLatin1(styleName));
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    return label;
}

QFrame *divider(QWidget *parent = nullptr)
{
    auto *line = new QFrame(parent);
    line->setObjectName(QStringLiteral("rowDivider"));
    line->setFixedHeight(1);
    return line;
}
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Sift"));
    setAcceptDrops(true);

    auto *root = new QWidget(this);
    root->setObjectName(QStringLiteral("root"));

    // Full-height navigation stays visible while switching document tabs.
    auto *rootLayout = new QHBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_sidebar = new Sidebar([this](const QString &page) {
        showSection(page);
    }, root);
    rootLayout->addWidget(m_sidebar);

    auto *mainArea = new QWidget(root);
    mainArea->setObjectName(QStringLiteral("mainArea"));
    auto *contentLayout = new QVBoxLayout(mainArea);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);
    rootLayout->addWidget(mainArea, 1);

    auto *topBar = new QFrame(mainArea);
    topBar->setObjectName(QStringLiteral("topBar"));
    topBar->setFixedHeight(64);
    auto *topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(27, 0, 27, 0);
    topLayout->setSpacing(12);

    m_headerTitle = new QLabel(QStringLiteral("Home"), topBar);
    m_headerTitle->setObjectName(QStringLiteral("headerTitle"));
    topLayout->addWidget(m_headerTitle);
    topLayout->addStretch();

    m_headerOpen = new QPushButton(QStringLiteral("Open files"), topBar);
    m_headerOpen->setObjectName(QStringLiteral("subtleButton"));
    m_headerOpen->setCursor(Qt::PointingHandCursor);
    topLayout->addWidget(m_headerOpen);
    contentLayout->addWidget(topBar);

    m_tabs = new QTabWidget(mainArea);
    m_tabs->setObjectName(QStringLiteral("documentTabs"));
    m_tabs->setDocumentMode(true);
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(false);

    m_tabs->addTab(createHomePage(), QStringLiteral("Home"));
    m_tabs->tabBar()->setTabButton(0, QTabBar::RightSide, nullptr);

    m_mainStack = new QStackedWidget(mainArea);
    m_mainStack->addWidget(m_tabs); // Index 0: home + open documents
    m_mainStack->addWidget(createRecentPage()); // Index 1
    m_mainStack->addWidget(createFavoritesPage()); // Index 2
    m_mainStack->addWidget(createPlaceholderPage(
        QStringLiteral("Folders"),
        QStringLiteral("Quick access to folders you choose will appear here. "
                       "Sift will not move or copy your original documents."))); // Index 3
    m_mainStack->addWidget(createPlaceholderPage(
        QStringLiteral("Settings"),
        QStringLiteral("App preferences will live here. "
                       "The sidebar collapse preference is already saved."))); // Index 4
    contentLayout->addWidget(m_mainStack, 1);
    setCentralWidget(root);

    connect(m_headerOpen, &QPushButton::clicked,
            this, &MainWindow::openFilesDialog);

    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int) {
        updateCurrentTabHeader();
    });

    connect(m_tabs, &QTabWidget::tabCloseRequested, this, [this](int index) {
        if (index <= 0)
            return;

        QWidget *page = m_tabs->widget(index);
        m_tabs->removeTab(index);
        page->deleteLater();

        if (m_tabs->count() == 1)
            m_tabs->setCurrentIndex(0);

        refreshTabVisibility();
    });

    refreshDashboards();
    refreshTabVisibility();
    updateCurrentTabHeader();
}

void MainWindow::showSection(const QString &section)
{
    if (section == QLatin1String("home")) {
        m_mainStack->setCurrentWidget(m_tabs);
        m_tabs->setCurrentIndex(0);
        updateCurrentTabHeader();
        return;
    }

    int index = -1;
    QString title;
    if (section == QLatin1String("recent")) {
        index = 1;
        title = QStringLiteral("Recent");
        refreshDashboards();
    } else if (section == QLatin1String("favorites")) {
        index = 2;
        title = QStringLiteral("Favorites");
        refreshDashboards();
    } else if (section == QLatin1String("folders")) {
        index = 3;
        title = QStringLiteral("Folders");
    } else if (section == QLatin1String("settings")) {
        index = 4;
        title = QStringLiteral("Settings");
    }

    if (index < 0)
        return;

    m_mainStack->setCurrentIndex(index);
    m_sidebar->setActivePage(section);
    m_headerTitle->setText(title);
}

void MainWindow::updateCurrentTabHeader()
{
    if (m_mainStack->currentWidget() != m_tabs)
        return;

    const int index = m_tabs->currentIndex();
    if (index <= 0) {
        m_sidebar->setActivePage(QStringLiteral("home"));
        m_headerTitle->setText(QStringLiteral("Home"));
    } else {
        // Documents are not navigation sections, so leave the sidebar neutral.
        m_sidebar->setActivePage(QString());
        m_headerTitle->setText(m_tabs->tabText(index));
    }
}

QWidget *MainWindow::createRecentPage()
{
    m_recentDashboard = new DocumentDashboard(
        DocumentDashboard::Mode::Recent,
        [this](const QString &path) { openFile(path); },
        [this](const QString &path, bool isFavorite) { setFavorite(path, isFavorite); },
        [this]() { openFilesDialog(); });
    return m_recentDashboard;
}

QWidget *MainWindow::createFavoritesPage()
{
    m_favoritesDashboard = new DocumentDashboard(
        DocumentDashboard::Mode::Favorites,
        [this](const QString &path) { openFile(path); },
        [this](const QString &path, bool isFavorite) { setFavorite(path, isFavorite); },
        [this]() { openFilesDialog(); });
    return m_favoritesDashboard;
}

QWidget *MainWindow::createPlaceholderPage(const QString &title,
                                             const QString &description)
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("libraryPage"));
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(36, 40, 36, 30);
    layout->setSpacing(0);

    auto *heading = new QLabel(title, page);
    heading->setObjectName(QStringLiteral("libraryTitle"));
    layout->addWidget(heading);
    layout->addSpacing(9);

    auto *note = new QLabel(description, page);
    note->setObjectName(QStringLiteral("libraryDescription"));
    note->setWordWrap(true);
    layout->addWidget(note);
    layout->addStretch();

    return page;
}

QWidget *MainWindow::createHomePage()
{
    m_homeDashboard = new DocumentDashboard(
        DocumentDashboard::Mode::Home,
        [this](const QString &path) { openFile(path); },
        [this](const QString &path, bool isFavorite) { setFavorite(path, isFavorite); },
        [this]() { openFilesDialog(); });
    return m_homeDashboard;
}

QWidget *MainWindow::createDocumentPage(const QString &filePath)
{
    if (QFileInfo(filePath).suffix().compare(QStringLiteral("pdf"), Qt::CaseInsensitive) == 0)
        return new PdfViewerPage(filePath);

    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("documentPage"));
    page->setProperty("filePath", filePath);

    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(12);
    layout->addStretch();

    auto *name = centeredLabel(QFileInfo(filePath).fileName(),
                               "pageTitle", page);
    layout->addWidget(name);

    auto *path = centeredLabel(QDir::toNativeSeparators(filePath),
                               "smallText", page);
    path->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(path);
    layout->addSpacing(18);

    auto *notice = centeredLabel(
        QStringLiteral("Document preview is coming next"),
        "dropHeading", page);
    layout->addWidget(notice);
    auto *explanation = centeredLabel(
        QStringLiteral("Sift has opened a tab for this document, but does not render its contents yet.\n"
                       "Your original file remains unchanged."),
        "subtitle", page);
    layout->addWidget(explanation);

    layout->addStretch();
    return page;
}

void MainWindow::openFilesDialog()
{
    const QString filter = QStringLiteral(
        "Supported documents (*.pdf *.docx *.doc *.xlsx *.xls *.pptx *.ppt)");
    const QStringList paths = QFileDialog::getOpenFileNames(
        this, QStringLiteral("Open documents"), QString(), filter);

    for (const QString &path : paths)
        openFile(path);
}

bool MainWindow::isSupported(const QString &filePath) const
{
    static const QStringList extensions = {
        QStringLiteral("pdf"), QStringLiteral("docx"),
        QStringLiteral("doc"), QStringLiteral("xlsx"),
        QStringLiteral("xls"), QStringLiteral("pptx"),
        QStringLiteral("ppt")
    };
    return extensions.contains(QFileInfo(filePath).suffix().toLower());
}

void MainWindow::openFile(const QString &filePath)
{
    const QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        QMessageBox::warning(this, QStringLiteral("File not found"),
            QStringLiteral("This file could not be found:\n%1").arg(filePath));
        QStringList paths = recentFiles();
        for (int i = paths.size() - 1; i >= 0; --i) {
            if (QString::compare(paths.at(i), filePath,
                                 filePathComparison()) == 0)
                paths.removeAt(i);
        }
        QSettings().setValue(QStringLiteral("recentFiles"), paths);
        refreshDashboards();
        return;
    }

    if (!isSupported(filePath)) {
        QMessageBox::information(this, QStringLiteral("Unsupported file"),
            QStringLiteral("Sift supports PDF, DOC, DOCX, XLS, XLSX, PPT, and PPTX files only."));
        return;
    }

    const QString absolutePath = QDir::cleanPath(info.absoluteFilePath());

    for (int i = 1; i < m_tabs->count(); ++i) {
        if (QString::compare(
                m_tabs->widget(i)->property("filePath").toString(),
                absolutePath, filePathComparison()) == 0) {
            m_mainStack->setCurrentWidget(m_tabs);
            m_tabs->setCurrentIndex(i);
            updateCurrentTabHeader();
            addToRecentFiles(absolutePath);
            return;
        }
    }

    auto *page = createDocumentPage(absolutePath);
    const int index = m_tabs->addTab(page, info.fileName());
    m_tabs->setTabToolTip(index, QDir::toNativeSeparators(absolutePath));
    m_mainStack->setCurrentWidget(m_tabs);
    m_tabs->setCurrentIndex(index);
    updateCurrentTabHeader();
    addToRecentFiles(absolutePath);
    refreshTabVisibility();
}

QStringList MainWindow::recentFiles() const
{
    return QSettings().value(QStringLiteral("recentFiles")).toStringList();
}

void MainWindow::addToRecentFiles(const QString &filePath)
{
    QStringList paths = recentFiles();
    for (int i = paths.size() - 1; i >= 0; --i) {
        if (QString::compare(paths.at(i), filePath,
                             filePathComparison()) == 0)
            paths.removeAt(i);
    }
    paths.prepend(filePath);

    QSettings settings;
    settings.setValue(QStringLiteral("recentFiles"), paths);
    QVariantMap times = settings.value(QStringLiteral("lastOpenedTimes")).toMap();
    times.insert(filePath, QDateTime::currentDateTime().toString(Qt::ISODate));
    settings.setValue(QStringLiteral("lastOpenedTimes"), times);
    refreshDashboards();
}

void MainWindow::setFavorite(const QString &filePath, bool favorite)
{
    QSettings settings;
    QStringList favorites = settings.value(QStringLiteral("favoriteFiles")).toStringList();

    for (int i = favorites.size() - 1; i >= 0; --i) {
        if (QString::compare(favorites.at(i), filePath,
                             filePathComparison()) == 0)
            favorites.removeAt(i);
    }
    if (favorite)
        favorites.prepend(filePath);

    settings.setValue(QStringLiteral("favoriteFiles"), favorites);

    // Refresh after the clicked star button's signal returns, so its row
    // can be safely recreated (especially when unpinning in Favorites).
    QTimer::singleShot(0, this, [this]() { refreshDashboards(); });
}

void MainWindow::refreshDashboards()
{
    if (m_homeDashboard)
        m_homeDashboard->refresh();
    if (m_recentDashboard)
        m_recentDashboard->refresh();
    if (m_favoritesDashboard)
        m_favoritesDashboard->refresh();
}

void MainWindow::refreshTabVisibility()
{
    m_tabs->tabBar()->setVisible(m_tabs->count() > 1);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        for (const QUrl &url : event->mimeData()->urls()) {
            if (url.isLocalFile() && isSupported(url.toLocalFile())) {
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}

void MainWindow::dragMoveEvent(QDragMoveEvent *event)
{
    event->acceptProposedAction();
}

void MainWindow::dragLeaveEvent(QDragLeaveEvent *event)
{
    event->accept();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    bool openedAny = false;
    for (const QUrl &url : event->mimeData()->urls()) {
        if (!url.isLocalFile())
            continue;

        const QString path = url.toLocalFile();
        if (!isSupported(path))
            continue;

        openFile(path);
        openedAny = true;
    }

    if (openedAny) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}
