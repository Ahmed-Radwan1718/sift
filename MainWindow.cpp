#include "MainWindow.h"
#include "DocumentDashboard.h"
#include "PdfViewerPage.h"
#include "Sidebar.h"

#include <QApplication>
#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QDropEvent>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QShortcut>
#include <QStackedWidget>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

namespace {
QIcon documentSidebarIcon(bool hidden)
{
    // Small monochrome icon resembling a document-reader side panel.
    QPixmap pixmap(40, 40);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(QStringLiteral("#dedede")), 1.8,
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);

    painter.drawRoundedRect(QRectF(4.5, 7.0, 31.0, 26.0), 4.0, 4.0);
    painter.drawLine(QPointF(15.5, 7.0), QPointF(15.5, 33.0));

    if (!hidden) {
        painter.fillRect(QRectF(6.5, 9.0, 7.3, 22.0),
                         QColor(QStringLiteral("#777777")));
        painter.drawLine(QPointF(25.5, 16.0), QPointF(22.0, 20.0));
        painter.drawLine(QPointF(22.0, 20.0), QPointF(25.5, 24.0));
    } else {
        painter.drawLine(QPointF(22.0, 16.0), QPointF(25.5, 20.0));
        painter.drawLine(QPointF(25.5, 20.0), QPointF(22.0, 24.0));
    }
    return QIcon(pixmap);
}

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
    // This is the reading preference, independent of the sidebar's own
    // expanded/collapsed navigation preference.
    m_documentSidebarHidden = QSettings().value(
        QStringLiteral("documentSidebarHidden"), false).toBool();

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

    m_tabs = new QTabWidget(mainArea);
    m_tabs->setObjectName(QStringLiteral("documentTabs"));
    m_tabs->setDocumentMode(true);
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(false);

    // Keep the document sidebar control accessible without a permanent header.
    // The tab bar is hidden on Home, so this uses no space until a file is open.
    m_sidebarVisibilityButton = new QToolButton(m_tabs);
    m_sidebarVisibilityButton->setObjectName(QStringLiteral("documentSidebarToggle"));
    m_sidebarVisibilityButton->setFixedSize(34, 34);
    m_sidebarVisibilityButton->setIconSize(QSize(20, 20));
    m_sidebarVisibilityButton->setCursor(Qt::PointingHandCursor);
    m_sidebarVisibilityButton->setVisible(false);
    m_tabs->setCornerWidget(m_sidebarVisibilityButton, Qt::TopRightCorner);

    m_tabs->addTab(createHomePage(), QStringLiteral("Home"));
    m_tabs->tabBar()->setTabButton(0, QTabBar::RightSide, nullptr);

    m_mainStack = new QStackedWidget(mainArea);
    m_mainStack->setAcceptDrops(true);
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

    // A transient overlay over the entire document area; never a permanent
    // "Add file" button. It also stays transparent to drag/drop hit testing.
    m_dropOverlay = new QFrame(m_mainStack);
    m_dropOverlay->setObjectName(QStringLiteral("fileDropOverlay"));
    m_dropOverlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_dropOverlay->setStyleSheet(QStringLiteral(
        "QFrame#fileDropOverlay { background-color: rgba(29, 31, 34, 235);"
        "border: 2px dashed #84929f; border-radius: 14px; }"
        "QLabel#fileDropTitle { background: transparent; color: #f3f3f3;"
        "font-size: 24px; font-weight: 600; border: none; }"
        "QLabel#fileDropHint { background: transparent; color: #b1b5bb;"
        "font-size: 13px; border: none; }"));
    auto *dropLayout = new QVBoxLayout(m_dropOverlay);
    dropLayout->setAlignment(Qt::AlignCenter);
    dropLayout->setSpacing(10);
    auto *dropTitle = new QLabel(QStringLiteral("Drop to open"), m_dropOverlay);
    dropTitle->setObjectName(QStringLiteral("fileDropTitle"));
    dropTitle->setAlignment(Qt::AlignCenter);
    dropLayout->addWidget(dropTitle);
    auto *dropHint = new QLabel(
        QStringLiteral("Open documents directly from File Explorer"), m_dropOverlay);
    dropHint->setObjectName(QStringLiteral("fileDropHint"));
    dropHint->setAlignment(Qt::AlignCenter);
    dropLayout->addWidget(dropHint);
    m_dropOverlay->setGeometry(m_mainStack->rect());
    m_dropOverlay->hide();

    // Still allow opening a document without drag-and-drop (Ctrl+O).
    auto *openShortcut = new QShortcut(QKeySequence::Open, this);
    openShortcut->setContext(Qt::WindowShortcut);
    connect(openShortcut, &QShortcut::activated,
            this, &MainWindow::openFilesDialog);

    connect(m_sidebarVisibilityButton, &QToolButton::clicked,
            this, &MainWindow::toggleDocumentSidebar);

    // Scope Ctrl+B to the window; it does nothing on Home or library pages.
    auto *sidebarShortcut = new QShortcut(
        QKeySequence(QStringLiteral("Ctrl+B")), this);
    sidebarShortcut->setContext(Qt::WindowShortcut);
    connect(sidebarShortcut, &QShortcut::activated,
            this, &MainWindow::toggleDocumentSidebar);

    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int) {
        updateCurrentTabNavigation();
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
        updateCurrentTabNavigation();
    });

    refreshDashboards();
    refreshTabVisibility();
    updateCurrentTabNavigation();

    // Catch drops even when the pointer is over a table's viewport or a tab.
    qApp->installEventFilter(this);
}

void MainWindow::showSection(const QString &section)
{
    if (section == QLatin1String("home")) {
        m_mainStack->setCurrentWidget(m_tabs);
        m_tabs->setCurrentIndex(0);
        updateCurrentTabNavigation();
        return;
    }

    int index = -1;
    if (section == QLatin1String("recent")) {
        index = 1;
        refreshDashboards();
    } else if (section == QLatin1String("favorites")) {
        index = 2;
        refreshDashboards();
    } else if (section == QLatin1String("folders")) {
        index = 3;
    } else if (section == QLatin1String("settings")) {
        index = 4;
    }

    if (index < 0)
        return;

    m_mainStack->setCurrentIndex(index);
    m_sidebar->setActivePage(section);
    updateSidebarVisibility();
}

void MainWindow::toggleDocumentSidebar()
{
    if (!isDocumentActive())
        return;

    m_documentSidebarHidden = !m_documentSidebarHidden;
    QSettings().setValue(QStringLiteral("documentSidebarHidden"),
                         m_documentSidebarHidden);
    updateSidebarVisibility();
}

bool MainWindow::isDocumentActive() const
{
    return m_mainStack && m_tabs &&
           m_mainStack->currentWidget() == m_tabs &&
           m_tabs->currentIndex() > 0;
}

void MainWindow::updateSidebarVisibility()
{
    const bool documentActive = isDocumentActive();

    // Library pages always expose navigation. Document tabs use the
    // remembered reading preference and reclaim the sidebar's entire width.
    m_sidebar->setVisible(!documentActive || !m_documentSidebarHidden);
    m_sidebarVisibilityButton->setVisible(documentActive);

    if (documentActive) {
        m_sidebarVisibilityButton->setIcon(
            documentSidebarIcon(m_documentSidebarHidden));
        const QString hint = m_documentSidebarHidden
            ? QStringLiteral("Show sidebar (Ctrl+B)")
            : QStringLiteral("Hide sidebar (Ctrl+B)");
        m_sidebarVisibilityButton->setToolTip(hint);
        m_sidebarVisibilityButton->setAccessibleName(hint);
    }
}

void MainWindow::updateCurrentTabNavigation()
{
    if (m_mainStack->currentWidget() != m_tabs)
        return;

    const int index = m_tabs->currentIndex();
    if (index <= 0) {
        m_sidebar->setActivePage(QStringLiteral("home"));
    } else {
        // Documents are not navigation sections, so leave the sidebar neutral.
        m_sidebar->setActivePage(QString());
    }
    updateSidebarVisibility();
}

QWidget *MainWindow::createRecentPage()
{
    m_recentDashboard = new DocumentDashboard(
        DocumentDashboard::Mode::Recent,
        [this](const QString &path) { openFile(path); },
        [this](const QString &path, bool isFavorite) { setFavorite(path, isFavorite); });
    return m_recentDashboard;
}

QWidget *MainWindow::createFavoritesPage()
{
    m_favoritesDashboard = new DocumentDashboard(
        DocumentDashboard::Mode::Favorites,
        [this](const QString &path) { openFile(path); },
        [this](const QString &path, bool isFavorite) { setFavorite(path, isFavorite); });
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
        [this](const QString &path, bool isFavorite) { setFavorite(path, isFavorite); });
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
            updateCurrentTabNavigation();
            addToRecentFiles(absolutePath);
            return;
        }
    }

    auto *page = createDocumentPage(absolutePath);
    const int index = m_tabs->addTab(page, info.fileName());
    m_tabs->setTabToolTip(index, QDir::toNativeSeparators(absolutePath));
    m_mainStack->setCurrentWidget(m_tabs);
    m_tabs->setCurrentIndex(index);
    updateCurrentTabNavigation();
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

QStringList MainWindow::droppedFiles(const QMimeData *mimeData) const
{
    QStringList result;
    if (!mimeData || !mimeData->hasUrls())
        return result;

    for (const QUrl &url : mimeData->urls()) {
        if (!url.isLocalFile())
            continue;
        const QFileInfo file(url.toLocalFile());
        if (!file.isFile() || !isSupported(file.fileName()))
            continue;
        const QString path = QDir::cleanPath(file.absoluteFilePath());
        if (!result.contains(path, filePathComparison()))
            result.append(path);
    }
    return result;
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_mainStack &&
        (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
        m_dropOverlay->setGeometry(m_mainStack->rect());
        if (m_dropOverlay->isVisible())
            m_dropOverlay->raise();
    }
    if (watched == this &&
        (event->type() == QEvent::WindowDeactivate || event->type() == QEvent::Hide)) {
        m_dropOverlay->hide();
    }

    auto *widget = qobject_cast<QWidget *>(watched);
    if (!widget || widget->window() != this)
        return QMainWindow::eventFilter(watched, event);

    if (event->type() == QEvent::DragLeave) {
        m_dropOverlay->hide();
        return false;
    }

    if (event->type() != QEvent::DragEnter &&
        event->type() != QEvent::DragMove &&
        event->type() != QEvent::Drop)
        return QMainWindow::eventFilter(watched, event);

    auto *drop = static_cast<QDropEvent *>(event);
    const QPoint local = m_mainStack->mapFromGlobal(
        widget->mapToGlobal(drop->position().toPoint()));
    const bool inside = m_mainStack->isVisible() &&
                        m_mainStack->rect().contains(local);
    const QStringList files = droppedFiles(drop->mimeData());

    // Never move or modify the source documents. Reject unsupported files
    // and drops outside the central document area.
    if (!inside || files.isEmpty() ||
        !drop->possibleActions().testFlag(Qt::CopyAction)) {
        m_dropOverlay->hide();
        return false;
    }

    drop->setDropAction(Qt::CopyAction);
    drop->accept();

    if (event->type() == QEvent::Drop) {
        m_dropOverlay->hide();
        for (const QString &path : files)
            openFile(path);
    } else {
        m_dropOverlay->setGeometry(m_mainStack->rect());
        m_dropOverlay->raise();
        m_dropOverlay->show();
    }
    return true;
}
