#include "DocumentDashboard.h"

#include <QAbstractItemView>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QStyle>
#include <QShortcut>
#include <QStackedWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QVariantMap>

#include <algorithm>
#include <utility>
#include <vector>

namespace {
struct Entry {
    QString path;
    QString name;
    QString category;
    QDateTime openedAt;
    int recentOrder = 0;
    bool favorite = false;
};

QString typeFor(const QString &path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == QLatin1String("pdf"))
        return QStringLiteral("PDF");
    if (ext == QLatin1String("doc") || ext == QLatin1String("docx"))
        return QStringLiteral("Word");
    if (ext == QLatin1String("xls") || ext == QLatin1String("xlsx"))
        return QStringLiteral("Excel");
    if (ext == QLatin1String("ppt") || ext == QLatin1String("pptx"))
        return QStringLiteral("PowerPoint");
    return {};
}

bool samePath(const QString &a, const QString &b)
{
#ifdef Q_OS_WIN
    return a.compare(b, Qt::CaseInsensitive) == 0;
#else
    return a == b;
#endif
}

bool containsPath(const QStringList &paths, const QString &path)
{
    for (const QString &candidate : paths) {
        if (samePath(candidate, path))
            return true;
    }
    return false;
}

QString openedText(const QDateTime &openedAt)
{
    if (!openedAt.isValid())
        return QStringLiteral("Earlier");

    const QDate date = openedAt.date();
    const QDate today = QDate::currentDate();
    if (date == today)
        return QStringLiteral("Today");
    if (date == today.addDays(-1))
        return QStringLiteral("Yesterday");
    return date.toString(QStringLiteral("MMM d, yyyy"));
}

QLabel *descriptionLabel(const QString &value, QWidget *parent)
{
    auto *label = new QLabel(value, parent);
    label->setObjectName(QStringLiteral("dashboardSubtitle"));
    return label;
}
} // namespace

DocumentDashboard::DocumentDashboard(Mode mode,
                                     OpenCallback openFile,
                                     FavoriteCallback setFavorite,
                                     OpenDialogCallback openDialog,
                                     QWidget *parent)
    : QWidget(parent),
      m_mode(mode),
      m_openFile(std::move(openFile)),
      m_setFavorite(std::move(setFavorite)),
      m_openDialog(std::move(openDialog))
{
    setObjectName(QStringLiteral("libraryPage"));
    setAcceptDrops(false);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(36, 32, 36, 28);
    layout->setSpacing(0);

    QString title;
    QString subtitle;
    QString section;
    if (mode == Mode::Home) {
        title = QStringLiteral("Your documents");
        subtitle = QStringLiteral("Quick access to recently opened files.");
        section = QStringLiteral("Recently opened");
    } else if (mode == Mode::Recent) {
        title = QStringLiteral("Recent documents");
        subtitle = QStringLiteral("Documents you've opened in Sift.");
        section = QStringLiteral("All recent files");
    } else {
        title = QStringLiteral("Favorites");
        subtitle = QStringLiteral("Documents you've pinned for quick access.");
        section = QStringLiteral("Pinned documents");
    }

    // The Home page shows the recent-files list directly.
    // Keep the full search/filter header on Recent and Favorites pages.
    auto *headingPanel = new QWidget(this);
    auto *headingLayout = new QVBoxLayout(headingPanel);
    headingLayout->setContentsMargins(0, 0, 0, 0);
    headingLayout->setSpacing(0);

    auto *pageTitle = new QLabel(title, this);
    pageTitle->setObjectName(QStringLiteral("dashboardTitle"));
    headingLayout->addWidget(pageTitle);
    headingLayout->addSpacing(7);
    headingLayout->addWidget(descriptionLabel(subtitle, this));
    headingLayout->addSpacing(29);

    m_search = new QLineEdit(this);
    m_search->setObjectName(QStringLiteral("documentSearch"));
    m_search->setPlaceholderText(QStringLiteral("Search documents by name..."));
    m_search->setClearButtonEnabled(true);
    m_search->setMinimumHeight(43);
    m_search->setMaximumWidth(600);
    m_search->setAccessibleName(QStringLiteral("Search documents by name"));

    // Keep the search field compact rather than stretching across the dashboard.
    auto *searchRow = new QHBoxLayout;
    searchRow->setContentsMargins(0, 0, 0, 0);
    searchRow->addWidget(m_search);
    searchRow->addStretch(1);
    headingLayout->addLayout(searchRow);
    headingLayout->addSpacing(20);

    auto *filters = new QHBoxLayout;
    filters->setSpacing(8);
    const QStringList types = {
        QStringLiteral("All files"), QStringLiteral("PDF"),
        QStringLiteral("Word"), QStringLiteral("Excel"),
        QStringLiteral("PowerPoint")
    };
    for (const QString &type : types) {
        auto *button = new QPushButton(type, this);
        button->setObjectName(QStringLiteral("typeFilter"));
        button->setProperty("selected", type == m_filter);
        button->setCheckable(false);
        button->setCursor(Qt::PointingHandCursor);
        button->setMinimumHeight(32);
        filters->addWidget(button);
        m_filterButtons.append(button);
        connect(button, &QPushButton::clicked, this, [this, type]() {
            setTypeFilter(type);
        });
    }
    filters->addStretch(1);
    headingLayout->addLayout(filters);
    headingLayout->addSpacing(30);
    headingPanel->setVisible(mode != Mode::Home);
    layout->addWidget(headingPanel);

    auto *sectionRow = new QHBoxLayout;
    sectionRow->setSpacing(14);
    auto *sectionLabel = new QLabel(section, this);
    sectionLabel->setObjectName(QStringLiteral("dashboardSection"));
    sectionRow->addWidget(sectionLabel);

    m_count = new QLabel(QStringLiteral("0 documents"), this);
    m_count->setObjectName(QStringLiteral("dashboardCount"));
    sectionRow->addWidget(m_count);
    sectionRow->addStretch(1);

    layout->addLayout(sectionRow);
    layout->addSpacing(13);

    m_listStack = new QStackedWidget(this);
    m_listStack->setObjectName(QStringLiteral("dashboardResults"));

    m_table = new QTreeWidget(m_listStack);
    m_table->setObjectName(QStringLiteral("documentTable"));
    m_table->setColumnCount(4);
    m_table->setHeaderLabels({
        QStringLiteral("NAME"), QStringLiteral("TYPE"),
        QStringLiteral("LAST OPENED"), QStringLiteral("FAVORITE")
    });
    m_table->setRootIsDecorated(false);
    m_table->setUniformRowHeights(true);
    m_table->setSortingEnabled(false);
    m_table->setIndentation(0);
    m_table->setAlternatingRowColors(false);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAllColumnsShowFocus(false);
    m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_table->header()->setStretchLastSection(false);
    m_table->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->header()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_table->header()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_table->header()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_table->setColumnWidth(1, 115);
    m_table->setColumnWidth(2, 140);
    m_table->setColumnWidth(3, 95);
    m_table->header()->setMinimumSectionSize(40);
    m_table->setIconSize(QSize(18, 18));
    m_listStack->addWidget(m_table);

    auto *emptyPage = new QWidget(m_listStack);
    emptyPage->setObjectName(QStringLiteral("emptyLibrary"));
    auto *emptyLayout = new QVBoxLayout(emptyPage);
    emptyLayout->setContentsMargins(18, 26, 18, 26);
    emptyLayout->setSpacing(12);
    emptyLayout->addStretch(1);

    auto *emptyTitle = new QLabel(QStringLiteral("Nothing to show yet"), emptyPage);
    emptyTitle->setObjectName(QStringLiteral("emptyTitle"));
    emptyTitle->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyTitle);

    m_emptyMessage = descriptionLabel(QStringLiteral("Open a document to see it here."),
                                      emptyPage);
    m_emptyMessage->setAlignment(Qt::AlignCenter);
    m_emptyMessage->setWordWrap(true);
    emptyLayout->addWidget(m_emptyMessage);

    auto *emptyOpen = new QPushButton(QStringLiteral("Open files"), emptyPage);
    emptyOpen->setObjectName(QStringLiteral("subtleButton"));
    emptyOpen->setCursor(Qt::PointingHandCursor);
    emptyLayout->addSpacing(5);
    emptyLayout->addWidget(emptyOpen, 0, Qt::AlignHCenter);
    emptyLayout->addStretch(2);
    m_listStack->addWidget(emptyPage);
    layout->addWidget(m_listStack, 1);

    connect(emptyOpen, &QPushButton::clicked, this, [this]() {
        if (m_openDialog)
            m_openDialog();
    });

    connect(m_table, &QTreeWidget::itemActivated, this,
            [this](QTreeWidgetItem *item, int) {
        if (item && m_openFile)
            m_openFile(item->data(0, Qt::UserRole).toString());
    });
    connect(m_search, &QLineEdit::textChanged, this,
            [this](const QString &) { refresh(); });

    auto *findShortcut = new QShortcut(QKeySequence::Find, this);
    findShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(findShortcut, &QShortcut::activated, this, [this]() {
        if (m_mode == Mode::Home)
            return;
        m_search->setFocus();
        m_search->selectAll();
    });

    refresh();
}

void DocumentDashboard::setTypeFilter(const QString &type)
{
    if (type == m_filter)
        return;
    m_filter = type;
    updateFilterButtons();
    refresh();
}

void DocumentDashboard::updateFilterButtons()
{
    for (auto *button : m_filterButtons) {
        button->setProperty("selected", button->text() == m_filter);
        button->style()->unpolish(button);
        button->style()->polish(button);
        button->update();
    }
}

QString DocumentDashboard::currentTypeFilter() const
{
    return m_filter;
}

void DocumentDashboard::refresh()
{
    QSettings settings;
    const QStringList recent = settings.value(QStringLiteral("recentFiles")).toStringList();
    const QStringList favorites = settings.value(QStringLiteral("favoriteFiles")).toStringList();
    const QVariantMap times = settings.value(QStringLiteral("lastOpenedTimes")).toMap();

    QStringList candidates;
    if (m_mode == Mode::Favorites) {
        candidates = favorites;
    } else {
        candidates = recent;
    }

    std::vector<Entry> entries;
    entries.reserve(static_cast<size_t>(candidates.size()));

    int order = 0;
    for (const QString &path : candidates) {
        const QString type = typeFor(path);
        if (type.isEmpty())
            continue;

        bool alreadyAdded = false;
        for (const Entry &entry : entries) {
            if (samePath(entry.path, path)) {
                alreadyAdded = true;
                break;
            }
        }
        if (alreadyAdded)
            continue;

        const QFileInfo info(path);
        Entry entry;
        entry.path = path;
        entry.name = info.fileName();
        entry.category = type;
        entry.favorite = containsPath(favorites, path);
        entry.openedAt = QDateTime::fromString(times.value(path).toString(),
                                             Qt::ISODate);
        entry.recentOrder = m_mode == Mode::Favorites
            ? (recent.indexOf(path) < 0 ? 100000 + order : recent.indexOf(path))
            : order;
        entries.push_back(std::move(entry));
        ++order;
    }

    // Preserve the default most-recently-opened order without a sort control.
    std::stable_sort(entries.begin(), entries.end(),
        [](const Entry &a, const Entry &b) {
            return a.recentOrder < b.recentOrder;
        });

    const QString searchTerm = m_search->text().trimmed();
    const QString selectedType = currentTypeFilter();
    std::vector<Entry> filtered;
    for (const Entry &entry : entries) {
        if (selectedType != QLatin1String("All files") &&
            selectedType != entry.category)
            continue;
        if (!searchTerm.isEmpty() &&
            !entry.name.contains(searchTerm, Qt::CaseInsensitive))
            continue;
        filtered.push_back(entry);
    }

    // Home is a short launch surface. Searching or filtering still checks
    // the whole history, instead of hiding matches beyond the newest ten.
    if (m_mode == Mode::Home && searchTerm.isEmpty() &&
        selectedType == QLatin1String("All files") &&
        filtered.size() > 10) {
        filtered.resize(10);
    }

    m_table->clear();
    for (const Entry &entry : filtered) {
        auto *item = new QTreeWidgetItem(m_table);
        item->setText(0, entry.name);
        item->setText(1, entry.category);
        item->setText(2, openedText(entry.openedAt));
        item->setData(0, Qt::UserRole, entry.path);
        item->setToolTip(0, QDir::toNativeSeparators(entry.path));
        item->setSizeHint(0, QSize(0, 45));

        auto *star = new QToolButton(m_table);
        star->setObjectName(QStringLiteral("favoriteStar"));
        star->setText(entry.favorite ? QStringLiteral("★") : QStringLiteral("☆"));
        star->setProperty("starred", entry.favorite);
        star->setToolTip(entry.favorite
            ? QStringLiteral("Remove from favorites")
            : QStringLiteral("Add to favorites"));
        star->setAccessibleName(star->toolTip());
        star->setCursor(Qt::PointingHandCursor);
        star->setAutoRaise(true);
        star->setFixedSize(36, 32);
        const QString filePath = entry.path;
        const bool isFavorite = entry.favorite;
        connect(star, &QToolButton::clicked, this, [this, filePath, isFavorite]() {
            if (m_setFavorite)
                m_setFavorite(filePath, !isFavorite);
        });

        auto *starCell = new QWidget(m_table);
        auto *starLayout = new QHBoxLayout(starCell);
        starLayout->setContentsMargins(0, 0, 0, 0);
        starLayout->setAlignment(Qt::AlignCenter);
        starLayout->addWidget(star);
        m_table->setItemWidget(item, 3, starCell);
    }

    m_count->setText(QStringLiteral("%1 %2")
        .arg(static_cast<qlonglong>(filtered.size()))
        .arg(filtered.size() == 1
             ? QStringLiteral("document") : QStringLiteral("documents")));

    if (!filtered.empty()) {
        m_listStack->setCurrentWidget(m_table);
        return;
    }

    if (!searchTerm.isEmpty() ||
        selectedType != QLatin1String("All files")) {
        m_emptyMessage->setText(QStringLiteral(
            "No documents match your search or file-type filter."));
    } else if (m_mode == Mode::Favorites) {
        m_emptyMessage->setText(QStringLiteral(
            "Star a document in Home or Recent to pin it here."));
    } else {
        m_emptyMessage->setText(QStringLiteral(
            "Open a PDF, Word, Excel, or PowerPoint file to get started."));
    }
    m_listStack->setCurrentIndex(1);
}
