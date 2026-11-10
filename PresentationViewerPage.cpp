#include "PresentationViewerPage.h"

#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QShortcut>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace {
QPushButton *control(const QString &label, const QString &hint, QWidget *parent)
{
    auto *button = new QPushButton(label, parent);
    button->setObjectName(QStringLiteral("pdfControl"));
    button->setToolTip(hint);
    button->setAccessibleName(hint);
    button->setCursor(Qt::PointingHandCursor);
    button->setFixedSize(31, 31);
    return button;
}
} // namespace

PresentationViewerPage::PresentationViewerPage(const QString &filePath, QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("documentPage"));
    setProperty("filePath", QDir::cleanPath(QFileInfo(filePath).absoluteFilePath()));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    QString loadError;
    if (!m_document.load(filePath, &loadError)) {
        auto *error = new QLabel(loadError, this);
        error->setObjectName(QStringLiteral("pdfMessage"));
        error->setAlignment(Qt::AlignCenter);
        error->setWordWrap(true);
        error->setContentsMargins(36, 36, 36, 36);
        root->addWidget(error, 1);
        return;
    }

    auto *body = new QWidget(this);
    auto *bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    m_thumbnails = new QListWidget(body);
    m_thumbnails->setObjectName(QStringLiteral("presentationThumbnails"));
    m_thumbnails->setFixedWidth(220);
    m_thumbnails->setIconSize(QSize(175, 100));
    m_thumbnails->setViewMode(QListView::IconMode);
    m_thumbnails->setFlow(QListView::TopToBottom);
    m_thumbnails->setWrapping(false);
    m_thumbnails->setResizeMode(QListView::Adjust);
    m_thumbnails->setMovement(QListView::Static);
    m_thumbnails->setSpacing(8);
    m_thumbnails->setGridSize(QSize(202, 137));
    m_thumbnails->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_thumbnails->setStyleSheet(QStringLiteral(
        "QListWidget#presentationThumbnails { background:#1c1c1c; border:0;"
        "border-right:1px solid #393939; color:#c4c4c4; padding:8px 0; }"
        "QListWidget#presentationThumbnails::item { border:2px solid transparent;"
        "border-radius:7px; padding:4px; }"
        "QListWidget#presentationThumbnails::item:selected { border-color:#b5b5b5;"
        "background:#353535; color:#ffffff; }"
        "QListWidget#presentationThumbnails::item:hover:!selected { background:#292929; }"));
    bodyLayout->addWidget(m_thumbnails);

    m_scroll = new QScrollArea(body);
    m_scroll->setObjectName(QStringLiteral("presentationScroll"));
    m_scroll->setWidgetResizable(true);
    m_scroll->setAlignment(Qt::AlignCenter);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setStyleSheet(QStringLiteral(
        "QScrollArea#presentationScroll { background:#202020; border:0; }"
        "QWidget#slideCanvas { background:#202020; }"));
    m_scroll->viewport()->installEventFilter(this);

    auto *canvas = new QWidget;
    canvas->setObjectName(QStringLiteral("slideCanvas"));
    auto *canvasLayout = new QVBoxLayout(canvas);
    canvasLayout->setContentsMargins(24, 24, 24, 24);
    canvasLayout->setSpacing(0);
    canvasLayout->addStretch(1);
    m_slide = new QLabel(canvas);
    m_slide->setAlignment(Qt::AlignCenter);
    m_slide->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_slide->setMinimumSize(1, 1);
    canvasLayout->addWidget(m_slide, 0, Qt::AlignHCenter);
    canvasLayout->addStretch(1);
    m_scroll->setWidget(canvas);
    bodyLayout->addWidget(m_scroll, 1);

    root->addWidget(body, 1);

    // A compact control strip remains available while viewing slides.
    auto *toolbar = new QFrame(this);
    toolbar->setObjectName(QStringLiteral("presentationToolbar"));
    toolbar->setStyleSheet(QStringLiteral(
        "QFrame#presentationToolbar { background:#292929;"
        "border-top:1px solid #3a3a3a; }"));
    auto *controls = new QHBoxLayout(toolbar);
    controls->setContentsMargins(14, 8, 14, 8);
    controls->setSpacing(7);

    auto *toggleThumbnails = control(QStringLiteral("☰"),
        QStringLiteral("Show or hide slide thumbnails"), toolbar);
    controls->addWidget(toggleThumbnails);
    controls->addStretch(1);

    m_previous = control(QStringLiteral("‹"), QStringLiteral("Previous slide"), toolbar);
    m_next = control(QStringLiteral("›"), QStringLiteral("Next slide"), toolbar);
    m_pageNumber = new QLineEdit(QStringLiteral("1"), toolbar);
    m_pageNumber->setObjectName(QStringLiteral("floatingPdfPageInput"));
    m_pageNumber->setFixedSize(44, 29);
    m_pageNumber->setAlignment(Qt::AlignCenter);
    m_pageNumber->setMaxLength(5);
    m_pageNumber->setAccessibleName(QStringLiteral("Current slide number"));
    m_pageNumber->setInputMethodHints(Qt::ImhDigitsOnly);
    m_pageTotal = new QLabel(QStringLiteral("/ %1").arg(m_document.slideCount()), toolbar);
    m_pageTotal->setObjectName(QStringLiteral("floatingPdfText"));
    controls->addWidget(m_previous);
    controls->addWidget(m_pageNumber);
    controls->addWidget(m_pageTotal);
    controls->addWidget(m_next);
    controls->addSpacing(22);

    m_zoomOut = control(QStringLiteral("−"), QStringLiteral("Zoom out"), toolbar);
    m_zoomIn = control(QStringLiteral("+"), QStringLiteral("Zoom in"), toolbar);
    m_zoomLabel = new QLabel(QStringLiteral("Fit page"), toolbar);
    m_zoomLabel->setObjectName(QStringLiteral("floatingPdfText"));
    m_zoomLabel->setFixedWidth(70);
    m_zoomLabel->setAlignment(Qt::AlignCenter);
    controls->addWidget(m_zoomOut);
    controls->addWidget(m_zoomLabel);
    controls->addWidget(m_zoomIn);

    auto *width = control(QStringLiteral("↔"), QStringLiteral("Fit slide width"), toolbar);
    auto *page = control(QStringLiteral("⛶"), QStringLiteral("Fit entire slide"), toolbar);
    controls->addSpacing(12);
    controls->addWidget(width);
    controls->addWidget(page);
    controls->addStretch(1);
    root->addWidget(toolbar);

    m_warning = new QLabel(this);
    m_warning->setObjectName(QStringLiteral("pptxWarning"));
    m_warning->setStyleSheet(QStringLiteral(
        "QLabel#pptxWarning { background:#332d24; color:#efcca0;"
        "padding:7px 14px; font-size:12px; }"));
    m_warning->setWordWrap(true);
    m_warning->hide();
    root->addWidget(m_warning);

    m_resizeTimer = new QTimer(this);
    m_resizeTimer->setSingleShot(true);
    m_resizeTimer->setInterval(90);
    connect(m_resizeTimer, &QTimer::timeout, this, [this]() { updateSlide(); });

    connect(toggleThumbnails, &QPushButton::clicked, this, [this]() {
        m_thumbnails->setVisible(!m_thumbnails->isVisible());
        m_resizeTimer->start();
    });
    connect(m_previous, &QPushButton::clicked, this,
            [this]() { goToSlide(m_current - 1); });
    connect(m_next, &QPushButton::clicked, this,
            [this]() { goToSlide(m_current + 1); });
    connect(m_zoomOut, &QPushButton::clicked, this,
            [this]() { changeZoom(1.0 / 1.2); });
    connect(m_zoomIn, &QPushButton::clicked, this,
            [this]() { changeZoom(1.2); });
    connect(width, &QPushButton::clicked, this, [this]() { fitWidth(); });
    connect(page, &QPushButton::clicked, this, [this]() { fitPage(); });
    connect(m_pageNumber, &QLineEdit::returnPressed, this, [this]() {
        bool ok = false;
        const int number = m_pageNumber->text().toInt(&ok);
        if (ok)
            goToSlide(number - 1);
        m_pageNumber->setText(QString::number(m_current + 1));
        m_scroll->setFocus();
    });
    connect(m_thumbnails, &QListWidget::currentRowChanged,
            this, [this](int row) { if (row >= 0) goToSlide(row); });

    auto shortcut = [this](const QKeySequence &sequence, int step) {
        auto *action = new QShortcut(sequence, this);
        action->setContext(Qt::WidgetWithChildrenShortcut);
        connect(action, &QShortcut::activated, this,
                [this, step]() { goToSlide(m_current + step); });
    };
    shortcut(QKeySequence(Qt::Key_Right), 1);
    shortcut(QKeySequence(Qt::Key_Down), 1);
    shortcut(QKeySequence(Qt::Key_PageDown), 1);
    shortcut(QKeySequence(Qt::Key_Left), -1);
    shortcut(QKeySequence(Qt::Key_Up), -1);
    shortcut(QKeySequence(Qt::Key_PageUp), -1);

    for (int i = 0; i < m_document.slideCount(); ++i) {
        auto *item = new QListWidgetItem(QStringLiteral("Slide %1").arg(i + 1));
        item->setToolTip(m_document.slideTitle(i));
        item->setTextAlignment(Qt::AlignHCenter);
        item->setSizeHint(QSize(195, 129));
        m_thumbnails->addItem(item);
    }

    goToSlide(0);
    QTimer::singleShot(0, this, [this]() { addNextThumbnail(); });
}

bool PresentationViewerPage::eventFilter(QObject *watched, QEvent *event)
{
    if (m_scroll && watched == m_scroll->viewport() && event->type() == QEvent::Wheel) {
        auto *wheel = static_cast<QWheelEvent *>(event);
        if (wheel->modifiers().testFlag(Qt::ControlModifier)) {
            changeZoom(wheel->angleDelta().y() >= 0 ? 1.12 : 1.0 / 1.12);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void PresentationViewerPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_resizeTimer && m_zoomMode != ZoomMode::Custom)
        m_resizeTimer->start();
}

QSize PresentationViewerPage::targetSize() const
{
    if (!m_scroll)
        return {};
    const QSize area = m_scroll->viewport()->size() - QSize(64, 64);
    const QSizeF slide = m_document.slideSize();
    const qreal ratio = slide.width() / slide.height();
    if (area.width() <= 0 || area.height() <= 0 || ratio <= 0)
        return {};

    qreal width = 0;
    if (m_zoomMode == ZoomMode::FitPage)
        width = std::min(static_cast<qreal>(area.width()), area.height() * ratio);
    else if (m_zoomMode == ZoomMode::FitWidth)
        width = area.width();
    else
        width = 960.0 * m_zoom;
    width = std::clamp(width, 180.0, 3600.0);
    return QSize(qRound(width), qRound(width / ratio));
}

void PresentationViewerPage::updateSlide()
{
    if (!m_scroll || m_document.slideCount() == 0)
        return;
    const QSize size = targetSize();
    if (size.isEmpty())
        return;

    QString warning;
    const QImage image = m_document.renderSlide(m_current, size, &warning);
    if (!image.isNull()) {
        m_slide->setStyleSheet(QString());
        m_slide->setPixmap(QPixmap::fromImage(image));
        m_slide->setFixedSize(image.size());
    } else {
        // Never show the previous slide when the current slide failed.
        m_slide->clear();
        m_slide->setText(QStringLiteral("Slide preview unavailable"));
        m_slide->setStyleSheet(QStringLiteral("color: #bdbdbd; background: #292929;"));
        m_slide->setFixedSize(size);
        if (warning.isEmpty())
            warning = QStringLiteral("This slide could not be rendered.");
    }
#ifndef SIFT_WITH_ASPOSE
    // A release without the commercial SDK must never imply that its
    // lightweight PowerPoint preview can faithfully show every slide.
    if (warning.isEmpty())
        warning = QStringLiteral(
            "Basic PowerPoint preview: some text, equations or diagrams may be missing.");
#endif
    m_warning->setText(warning);
    m_warning->setVisible(!warning.isEmpty());
    m_pageNumber->setText(QString::number(m_current + 1));
    m_previous->setEnabled(m_current > 0);
    m_next->setEnabled(m_current + 1 < m_document.slideCount());
    m_zoomLabel->setText(m_zoomMode == ZoomMode::FitPage
                             ? QStringLiteral("Fit page")
                             : m_zoomMode == ZoomMode::FitWidth
                                   ? QStringLiteral("Fit width")
                                   : QStringLiteral("%1%").arg(qRound(m_zoom * 100)));
}

void PresentationViewerPage::goToSlide(int index)
{
    if (index < 0 || index >= m_document.slideCount())
        return;
    m_current = index;
    if (m_thumbnails && m_thumbnails->currentRow() != index) {
        const QSignalBlocker block(m_thumbnails);
        m_thumbnails->setCurrentRow(index);
    }
    updateSlide();
}

void PresentationViewerPage::changeZoom(qreal factor)
{
    const QSize size = targetSize();
    if (m_zoomMode != ZoomMode::Custom)
        m_zoom = size.width() / 960.0;
    m_zoom = std::clamp(m_zoom * factor, 0.2, 3.75);
    m_zoomMode = ZoomMode::Custom;
    updateSlide();
}

void PresentationViewerPage::fitPage()
{
    m_zoomMode = ZoomMode::FitPage;
    updateSlide();
}

void PresentationViewerPage::fitWidth()
{
    m_zoomMode = ZoomMode::FitWidth;
    updateSlide();
}

void PresentationViewerPage::addNextThumbnail()
{
    if (!m_thumbnails || m_nextThumbnail >= m_document.slideCount())
        return;
    const int index = m_nextThumbnail++;
    const QSizeF logical = m_document.slideSize();
    const int width = 175;
    const QSize size(width, std::max(1, qRound(width * logical.height() / logical.width())));
    const QImage image = m_document.renderSlide(index, size);
    if (!image.isNull() && m_thumbnails->item(index))
        m_thumbnails->item(index)->setIcon(QIcon(QPixmap::fromImage(image)));
    QTimer::singleShot(0, this, [this]() { addNextThumbnail(); });
}
