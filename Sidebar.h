#pragma once

#include <QFrame>
#include <QString>
#include <QVector>

#include <functional>

class QEvent;
class QLabel;
class QPushButton;
class QVBoxLayout;
class QVariantAnimation;

// Sift's persistent left navigation.
class Sidebar final : public QFrame
{
public:
    explicit Sidebar(std::function<void(const QString &)> navigate,
                     QWidget *parent = nullptr);

    void setActivePage(const QString &pageId);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QPushButton *addNavigationItem(const QString &id,
                                   const QString &title,
                                   const QString &iconName,
                                   QVBoxLayout *target);
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
    QPushButton *m_settingsButton = nullptr;
    QVariantAnimation *m_settingsSpin = nullptr;
    qreal m_settingsAngle = 0.0;
};
