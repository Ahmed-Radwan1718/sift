#include "MainWindow.h"
#include "PdfViewerPage.h"
#include "Sidebar.h"

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
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

namespace {
constexpr int kMaxRecentFiles = 6;

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
    m_mainStack->addWidget(createPlaceholderPage(
        QStringLiteral("Favorites"),
        QStringLiteral("Your pinned documents will appear here. "
                       "We'll add favoriting after the library layout."))); // Index 2
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

    refreshRecentFiles();
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
        refreshRecentFiles();
    } else if (section == QLatin1String("favorites")) {
        index = 2;
        title = QStringLiteral("Favorites");
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
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("libraryPage"));
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(36, 40, 36, 30);
    layout->setSpacing(0);

    auto *title = new QLabel(QStringLiteral("Recent documents"), page);
    title->setObjectName(QStringLiteral("libraryTitle"));
    layout->addWidget(title);
    layout->addSpacing(9);

    auto *description = new QLabel(
        QStringLiteral("Files you've recently opened in Sift."), page);
    description->setObjectName(QStringLiteral("libraryDescription"));
    layout->addWidget(description);
    layout->addSpacing(32);
    layout->addWidget(divider(page));
    layout->addSpacing(8);

    m_recentSectionItems = new QVBoxLayout;
    m_recentSectionItems->setSpacing(1);
    m_recentSectionItems->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(m_recentSectionItems);
    layout->addStretch();

    return page;
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
    auto *scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *surface = new QWidget;
    surface->setObjectName(QStringLiteral("homeSurface"));
    auto *outer = new QVBoxLayout(surface);
    outer->setContentsMargins(24, 30, 24, 30);
    outer->addStretch();

    auto *content = new QWidget(surface);
    content->setMaximumWidth(680);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *title = centeredLabel(QStringLiteral("Your files, one place."),
                                "pageTitle", content);
    layout->addWidget(title);
    layout->addSpacing(9);

    auto *subtitle = centeredLabel(
        QStringLiteral("A simple, read-only home for your documents."),
        "subtitle", content);
    layout->addWidget(subtitle);
    layout->addSpacing(29);

    m_dropZone = new QFrame(content);
    m_dropZone->setObjectName(QStringLiteral("dropZone"));
    m_dropZone->setProperty("dragActive", false);
    m_dropZone->setMinimumHeight(242);

    auto *dropLayout = new QVBoxLayout(m_dropZone);
    dropLayout->setContentsMargins(20, 22, 20, 22);
    dropLayout->setSpacing(10);
    dropLayout->setAlignment(Qt::AlignCenter);

    auto *icon = centeredLabel(QStringLiteral("⇧"), "dropSymbol", m_dropZone);
    icon->setFixedSize(60, 60);
    dropLayout->addWidget(icon, 0, Qt::AlignHCenter);

    dropLayout->addSpacing(4);
    auto *dropText = centeredLabel(QStringLiteral("Drag and drop files here"),
                                   "dropHeading", m_dropZone);
    dropLayout->addWidget(dropText);

    auto *dropHint = centeredLabel(QStringLiteral("or choose files from your computer"),
                                   "hint", m_dropZone);
    dropLayout->addWidget(dropHint);
    dropLayout->addSpacing(5);

    auto *openButton = new QPushButton(QStringLiteral("Open files"), m_dropZone);
    openButton->setObjectName(QStringLiteral("primaryButton"));
    openButton->setCursor(Qt::PointingHandCursor);
    connect(openButton, &QPushButton::clicked,
            this, &MainWindow::openFilesDialog);
    dropLayout->addWidget(openButton, 0, Qt::AlignHCenter);
    layout->addWidget(m_dropZone);

    layout->addSpacing(15);
    auto *formats = centeredLabel(
        QStringLiteral("PDF  ·  DOCX  ·  DOC  ·  XLSX  ·  XLS  ·  PPTX  ·  PPT"),
        "supported", content);
    layout->addWidget(formats);

    layout->addSpacing(37);
    auto *recentHeading = new QLabel(QStringLiteral("Recent files"), content);
    recentHeading->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(recentHeading);
    layout->addSpacing(12);
    layout->addWidget(divider(content));
    layout->addSpacing(8);

    m_recentItems = new QVBoxLayout;
    m_recentItems->setContentsMargins(0, 0, 0, 0);
    m_recentItems->setSpacing(1);
    layout->addLayout(m_recentItems);

    outer->addWidget(content, 0, Qt::AlignHCenter);
    outer->addStretch();
    scrollArea->setWidget(surface);
    return scrollArea;
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
        paths.removeAll(filePath);
        QSettings().setValue(QStringLiteral("recentFiles"), paths);
        refreshRecentFiles();
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
        if (QString::compare(paths.at(i), filePath, filePathComparison()) == 0)
            paths.removeAt(i);
    }
    paths.prepend(filePath);
    while (paths.size() > kMaxRecentFiles)
        paths.removeLast();

    QSettings().setValue(QStringLiteral("recentFiles"), paths);
    refreshRecentFiles();
}

void MainWindow::refreshRecentFiles()
{
    const QStringList paths = recentFiles();
    auto populate = [this, &paths](QVBoxLayout *items) {
        if (!items)
            return;

        while (QLayoutItem *item = items->takeAt(0)) {
            if (QWidget *widget = item->widget())
                widget->deleteLater();
            delete item;
        }

        if (paths.isEmpty()) {
            auto *empty = new QLabel(QStringLiteral("No recent files yet."));
            empty->setObjectName(QStringLiteral("smallText"));
            empty->setContentsMargins(8, 12, 8, 12);
            items->addWidget(empty);
            return;
        }

        for (const QString &path : paths) {
            auto *row = new QWidget;
            auto *rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(0, 0, 2, 0);
            rowLayout->setSpacing(10);

            const QFileInfo info(path);
            auto *button = new QPushButton(info.fileName(), row);
            button->setObjectName(QStringLiteral("recentItem"));
            button->setToolTip(QDir::toNativeSeparators(path));
            button->setCursor(Qt::PointingHandCursor);
            button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            connect(button, &QPushButton::clicked, this, [this, path]() {
                openFile(path);
            });
            rowLayout->addWidget(button, 1);

            auto *tag = new QLabel(info.suffix().toUpper(), row);
            tag->setObjectName(QStringLiteral("extensionTag"));
            rowLayout->addWidget(tag);

            items->addWidget(row);
        }
    };

    populate(m_recentItems);
    populate(m_recentSectionItems);
}

void MainWindow::refreshTabVisibility()
{
    m_tabs->tabBar()->setVisible(m_tabs->count() > 1);
}

void MainWindow::setDropHighlight(bool active)
{
    if (!m_dropZone)
        return;

    m_dropZone->setProperty("dragActive", active);
    m_dropZone->style()->unpolish(m_dropZone);
    m_dropZone->style()->polish(m_dropZone);
    m_dropZone->update();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        for (const QUrl &url : event->mimeData()->urls()) {
            if (url.isLocalFile() && isSupported(url.toLocalFile())) {
                event->acceptProposedAction();
                setDropHighlight(true);
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
    setDropHighlight(false);
    event->accept();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    setDropHighlight(false);
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
