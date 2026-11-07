#include "Sidebar.h"

#include <QColor>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QSettings>
#include <QStyle>
#include <QVBoxLayout>

#include <cmath>
#include <utility>

namespace {
QIcon navigationIcon(const QString &name, bool active)
{
    if (name == QLatin1String("settings")) {
        // Tint the original PNG using its alpha channel, keeping its shape.
        // Match the other sidebar icons: gray normally, near-white when active.
        QPixmap source(QStringLiteral(":/settings.png"));
        if (source.isNull())
            return QIcon();

        QPixmap tinted(44, 44);
        tinted.fill(Qt::transparent);
        QPainter iconPainter(&tinted);
        iconPainter.setRenderHint(QPainter::SmoothPixmapTransform);
        iconPainter.drawPixmap(tinted.rect(), source);
        iconPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        iconPainter.fillRect(tinted.rect(),
            active ? QColor(QStringLiteral("#f0f0f0"))
                   : QColor(QStringLiteral("#a4a4a4")));
        iconPainter.end();
        return QIcon(tinted);
    }

    QPixmap pixmap(44, 44);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(2.0, 2.0);

    QPen pen(active ? QColor(QStringLiteral("#f0f0f0"))
                    : QColor(QStringLiteral("#a4a4a4")),
             1.65, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    if (name == QLatin1String("home")) {
        QPainterPath house;
        house.moveTo(3.5, 10.0);
        house.lineTo(11.0, 3.5);
        house.lineTo(18.5, 10.0);
        house.moveTo(5.5, 9.0);
        house.lineTo(5.5, 18.0);
        house.lineTo(16.5, 18.0);
        house.lineTo(16.5, 9.0);
        house.moveTo(9.0, 18.0);
        house.lineTo(9.0, 13.0);
        house.lineTo(13.0, 13.0);
        house.lineTo(13.0, 18.0);
        painter.drawPath(house);
    } else if (name == QLatin1String("recent")) {
        painter.drawEllipse(QRectF(3, 3, 16, 16));
        painter.drawLine(QPointF(11, 6.5), QPointF(11, 11));
        painter.drawLine(QPointF(11, 11), QPointF(14.5, 13));
    } else if (name == QLatin1String("favorites")) {
        QPainterPath star;
        constexpr qreal pi = 3.14159265358979323846;
        for (int i = 0; i < 10; ++i) {
            const qreal angle = -pi / 2 + i * pi / 5;
            const qreal radius = (i % 2 == 0) ? 8.5 : 3.8;
            const QPointF point(11 + std::cos(angle) * radius,
                                11 + std::sin(angle) * radius);
            if (i == 0)
                star.moveTo(point);
            else
                star.lineTo(point);
        }
        star.closeSubpath();
        painter.drawPath(star);
    } else if (name == QLatin1String("folders")) {
        QPainterPath folder;
        folder.moveTo(2.5, 7);
        folder.lineTo(8.5, 7);
        folder.lineTo(10.7, 9.2);
        folder.lineTo(19, 9.2);
        folder.quadTo(20, 9.2, 20, 10.5);
        folder.lineTo(20, 17.5);
        folder.quadTo(20, 19, 18.5, 19);
        folder.lineTo(3.5, 19);
        folder.quadTo(2, 19, 2, 17.5);
        folder.lineTo(2, 8.5);
        folder.quadTo(2, 7, 2.5, 7);
        painter.drawPath(folder);

    }
    painter.end();
    return QIcon(pixmap);
}

void refreshStyle(QWidget *widget)
{
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}
} // namespace

Sidebar::Sidebar(std::function<void(const QString &)> navigate, QWidget *parent)
    : QFrame(parent), m_navigate(std::move(navigate))
{
    setObjectName(QStringLiteral("sidebar"));
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 18, 12, 16);
    layout->setSpacing(3);

    m_section = new QLabel(QStringLiteral("WORKSPACE"), this);
    m_section->setObjectName(QStringLiteral("sidebarSection"));
    m_section->setContentsMargins(11, 0, 0, 8);
    layout->addWidget(m_section);

    addNavigationItem(QStringLiteral("home"), QStringLiteral("Home"),
                      QStringLiteral("home"), layout);
    addNavigationItem(QStringLiteral("recent"), QStringLiteral("Recent"),
                      QStringLiteral("recent"), layout);
    addNavigationItem(QStringLiteral("favorites"), QStringLiteral("Favorites"),
                      QStringLiteral("favorites"), layout);
    addNavigationItem(QStringLiteral("folders"), QStringLiteral("Folders"),
                      QStringLiteral("folders"), layout);

    layout->addStretch(1);

    auto *separator = new QFrame(this);
    separator->setObjectName(QStringLiteral("sidebarRule"));
    separator->setFixedHeight(1);
    layout->addWidget(separator);
    layout->addSpacing(10);

    addNavigationItem(QStringLiteral("settings"), QStringLiteral("Settings"),
                      QStringLiteral("settings"), layout);

    // Keep the collapse action available, without a branding header.
    auto *bottom = new QWidget(this);
    auto *bottomLayout = new QHBoxLayout(bottom);
    bottomLayout->setContentsMargins(7, 9, 3, 0);
    bottomLayout->setSpacing(4);

    m_footer = new QLabel(QStringLiteral("READ ONLY"), bottom);
    m_footer->setObjectName(QStringLiteral("sidebarFooter"));
    bottomLayout->addWidget(m_footer, 1);

    m_toggle = new QPushButton(QStringLiteral("‹"), bottom);
    m_toggle->setObjectName(QStringLiteral("sidebarToggle"));
    m_toggle->setFixedSize(30, 30);
    m_toggle->setToolTip(QStringLiteral("Collapse sidebar"));
    m_toggle->setCursor(Qt::PointingHandCursor);
    bottomLayout->addWidget(m_toggle);

    layout->addWidget(bottom);

    connect(m_toggle, &QPushButton::clicked, this, [this]() {
        setCollapsed(!m_collapsed);
    });

    setCollapsed(QSettings().value(QStringLiteral("sidebarCollapsed"), false).toBool());
}

QPushButton *Sidebar::addNavigationItem(const QString &id,
                                        const QString &title,
                                        const QString &iconName,
                                        QVBoxLayout *target)
{
    auto *button = new QPushButton(title, this);
    button->setObjectName(QStringLiteral("sidebarNav"));
    button->setCursor(Qt::PointingHandCursor);
    button->setFixedHeight(42);
    button->setIconSize(QSize(20, 20));
    button->setToolTip(title);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    target->addWidget(button);

    m_items.push_back({id, title, iconName, button});

    connect(button, &QPushButton::clicked, this, [this, id]() {
        if (m_navigate)
            m_navigate(id);
    });
    return button;
}

void Sidebar::setActivePage(const QString &pageId)
{
    if (m_activePage == pageId)
        return;
    m_activePage = pageId;
    refreshAppearance();
}

void Sidebar::setCollapsed(bool collapsed)
{
    m_collapsed = collapsed;
    setFixedWidth(collapsed ? 66 : 220);
    m_section->setVisible(!collapsed);
    m_footer->setVisible(!collapsed);

    m_toggle->setText(collapsed ? QStringLiteral("›") : QStringLiteral("‹"));
    m_toggle->setToolTip(collapsed ? QStringLiteral("Expand sidebar")
                                 : QStringLiteral("Collapse sidebar"));
    for (auto &item : m_items) {
        item.button->setText(collapsed ? QString() : item.title);
        item.button->setProperty("compact", collapsed);
    }
    QSettings().setValue(QStringLiteral("sidebarCollapsed"), collapsed);
    refreshAppearance();
}

void Sidebar::refreshAppearance()
{
    for (auto &item : m_items) {
        const bool active = item.id == m_activePage;
        item.button->setProperty("active", active);
        item.button->setIcon(navigationIcon(item.iconName, active));
        refreshStyle(item.button);
    }
}
