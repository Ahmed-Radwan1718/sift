#pragma once

#include <QFrame>
#include <QString>
#include <QVector>

#include <functional>

class QLabel;
class QPushButton;
class QVBoxLayout;

// Sift's persistent left navigation. The layout can be expanded or collapsed.
class Sidebar final : public QFrame
{
public:
    explicit Sidebar(std::function<void(const QString &)> navigate,
                     QWidget *parent = nullptr);

    void setActivePage(const QString &pageId);

private:
    QPushButton *addNavigationItem(const QString &id,
                                   const QString &title,
                                   const QString &iconName,
                                   QVBoxLayout *target);
    void setCollapsed(bool collapsed);
    void refreshAppearance();

    struct NavItem {
        QString id;
        QString title;
        QString iconName;
        QPushButton *button = nullptr;
    };

    std::function<void(const QString &)> m_navigate;
    QVector<NavItem> m_items;
    QString m_activePage = QStringLiteral("home");
    QLabel *m_section = nullptr;
    QLabel *m_footer = nullptr;
    QPushButton *m_toggle = nullptr;
    bool m_collapsed = false;
};
