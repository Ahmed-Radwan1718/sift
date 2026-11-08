#pragma once

#include <QWidget>
#include <QList>
#include <QString>
#include <functional>

class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTreeWidget;
class QHBoxLayout;

// Read-only library of shortcuts to original files. Never copies or edits files.
class DocumentDashboard final : public QWidget
{
public:
    enum class Mode { Home, Recent, Favorites };
    using OpenCallback = std::function<void(const QString &)>;
    using FavoriteCallback = std::function<void(const QString &, bool)>;

    explicit DocumentDashboard(Mode mode,
                               OpenCallback openFile,
                               FavoriteCallback setFavorite,
                               QWidget *parent = nullptr);

    void refresh();

private:
    void setTypeFilter(const QString &type);
    void updateFilterButtons();
    QString currentTypeFilter() const;

    Mode m_mode;
    OpenCallback m_openFile;
    FavoriteCallback m_setFavorite;
    QString m_filter = QStringLiteral("All files");

    QLineEdit *m_search = nullptr;
    QTreeWidget *m_table = nullptr;
    QLabel *m_count = nullptr;
    QLabel *m_emptyMessage = nullptr;
    QStackedWidget *m_listStack = nullptr;
    QList<QPushButton *> m_filterButtons;
};
