#include "PdfViewerPage.h"

#include <QColor>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMargins>
#include <QPdfDocumentRenderOptions>
#include <QPdfPageNavigator>
#include <QPdfPageRenderer>
#include <QPdfView>
#include <QPainter>
#include <QPaintEvent>
#include <QPixmap>
#include <QPointF>
#include <QPushButton>
#include <QScreen>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>


// Keep the last rendered frame on screen while Qt rebuilds the PDF page
// raster cache. Scale this snapshot immediately so Ctrl+wheel stays responsive.
class ZoomPreviewOverlay final : public QWidget
{
public:
    explicit ZoomPreviewOverlay(QWidget *parent)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_OpaquePaintEvent);
        hide();
    }

    void capture(const QPixmap &image)
    {
        m_snapshot = image;
        m_ratio = 1.0;
    }

    void preview(qreal ratio, QPointF anchor)
    {
        m_ratio = ratio;
        m_anchor = anchor;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(QStringLiteral("#202020")));
        if (m_snapshot.isNull())
            return;

        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.translate(m_anchor);
        painter.scale(m_ratio, m_ratio);
        painter.translate(-m_anchor);
        painter.drawPixmap(QPointF(0, 0), m_snapshot);
    }

private:
    QPixmap m_snapshot;
    qreal m_ratio = 1.0;
    QPointF m_anchor;
};

namespace {
QPushButton *toolbarButton(const QString &label, const QString &tooltip,
                           QWidget *parent)
{
    auto *button = new QPushButton(label, parent);
    button->setObjectName(QStringLiteral("pdfControl"));
    button->setToolTip(tooltip);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}
} // namespace

PdfViewerPage::PdfViewerPage(const QString &filePath, QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("documentPage"));
    setProperty("filePath", QDir::cleanPath(QFileInfo(filePath).absoluteFilePath()));

    // The document is owned by this tab, not by the main window. Closing the tab
    // releases the PDF and its renderer without touching the original file.
    m_document = new QPdfDocument(this);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *toolbar = new QFrame(this);
    toolbar->setObjectName(QStringLiteral("pdfToolbar"));
    auto *controls = new QHBoxLayout(toolbar);
    controls->setContentsMargins(18, 9, 18, 9);
    controls->setSpacing(8);

    m_previous = toolbarButton(QStringLiteral("‹"), QStringLiteral("Previous page"), toolbar);
    m_next = toolbarButton(QStringLiteral("›"), QStringLiteral("Next page"), toolbar);
    m_previous->setFixedWidth(34);
    m_next->setFixedWidth(34);

    m_pageInput = new QSpinBox(toolbar);
    m_pageInput->setObjectName(QStringLiteral("pdfPageInput"));
    m_pageInput->setRange(1, 1);
    m_pageInput->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_pageInput->setAlignment(Qt::AlignCenter);
    m_pageInput->setFixedWidth(55);
    m_pageInput->setToolTip(QStringLiteral("Go to page"));

    m_pageTotal = new QLabel(QStringLiteral("/ 0"), toolbar);
    m_pageTotal->setObjectName(QStringLiteral("pdfToolbarText"));

    controls->addWidget(m_previous);
    controls->addWidget(m_pageInput);
    controls->addWidget(m_pageTotal);
    controls->addWidget(m_next);
    controls->addStretch(1);

    m_zoomOut = toolbarButton(QStringLiteral("−"), QStringLiteral("Zoom out"), toolbar);
    m_zoomIn = toolbarButton(QStringLiteral("+"), QStringLiteral("Zoom in"), toolbar);
    m_zoomOut->setFixedWidth(36);
    m_zoomIn->setFixedWidth(36);

    m_zoomLabel = new QLabel(QStringLiteral("Fit width"), toolbar);
    m_zoomLabel->setObjectName(QStringLiteral("pdfToolbarText"));
    m_zoomLabel->setMinimumWidth(70);
    m_zoomLabel->setAlignment(Qt::AlignCenter);

    m_fitWidth = toolbarButton(QStringLiteral("Fit width"),
                               QStringLiteral("Fit page width to window"), toolbar);
    m_fitPage = toolbarButton(QStringLiteral("Fit page"),
                              QStringLiteral("Fit entire page to window"), toolbar);

    controls->addWidget(m_zoomOut);
    controls->addWidget(m_zoomLabel);
    controls->addWidget(m_zoomIn);
    controls->addSpacing(8);
    controls->addWidget(m_fitWidth);
    controls->addWidget(m_fitPage);
    layout->addWidget(toolbar);

    m_stack = new QStackedWidget(this);

    // Keep the actual PDF renderer in the stack for the life of this tab.
    m_view = new QPdfView(m_stack);
    m_view->setPageMode(QPdfView::PageMode::MultiPage);
    m_view->setZoomMode(QPdfView::ZoomMode::FitToWidth);
    m_view->setPageSpacing(14);
    m_view->setDocumentMargins(QMargins(22, 18, 22, 18));
    m_view->setDocument(m_document);
    // QAbstractScrollArea delivers wheel events to its viewport widget.
    // Intercept Ctrl+wheel before the built-in scroll handling.
    m_view->viewport()->installEventFilter(this);

    m_zoomPreview = new ZoomPreviewOverlay(m_view->viewport());
    m_zoomTimer = new QTimer(this);
    m_zoomTimer->setSingleShot(true);
    m_zoomTimer->setInterval(85);
    connect(m_zoomTimer, &QTimer::timeout,
            this, [this]() { applyPendingZoom(); });

    // QPdfView owns its worker renderer. It paints white pages when its cache
    // is invalidated; unveil the new frame only after its render completes.
    // If a Qt version changes this internal child, the fallback timer below
    // still prevents the preview from staying onscreen indefinitely.
    if (auto *renderer = m_view->findChild<QPdfPageRenderer *>()) {
        connect(renderer, &QPdfPageRenderer::pageRendered, this,
            [this](int page, QSize imageSize, const QImage &,
                   QPdfDocumentRenderOptions, quint64) {
                if (!m_waitingForRender || m_zoomTimer->isActive() ||
                    m_document->status() != QPdfDocument::Status::Ready ||
                    page < 0 || page >= m_document->pageCount())
                    return;

                const auto *screen = QGuiApplication::primaryScreen();
                const qreal dpiScale =
                    (screen ? screen->logicalDotsPerInch() : 96.0) / 72.0;
                const QSize pageSize =
                    (m_document->pagePointSize(page) * dpiScale *
                     effectiveZoomFactor()).toSize();
                const QSize expectedSize =
                    (QSizeF(pageSize) * m_view->devicePixelRatioF()).toSize();

                if (std::abs(expectedSize.width() - imageSize.width()) > 2 ||
                    std::abs(expectedSize.height() - imageSize.height()) > 2)
                    return; // An older zoom request finished late.

                const quint64 generation = m_renderGeneration;
                QTimer::singleShot(45, this, [this, generation]() {
                    if (m_waitingForRender && m_renderGeneration == generation &&
                        !m_zoomTimer->isActive())
                        finishZoomPreview();
                });
            });
    }
    m_stack->addWidget(m_view);

    m_messagePage = new QWidget(m_stack);
    m_messagePage->setObjectName(QStringLiteral("documentPage"));
    auto *messageLayout = new QVBoxLayout(m_messagePage);
    messageLayout->setContentsMargins(32, 32, 32, 32);
    messageLayout->addStretch();

    m_message = new QLabel(QStringLiteral("Loading PDF…"), m_messagePage);
    m_message->setObjectName(QStringLiteral("pdfMessage"));
    m_message->setAlignment(Qt::AlignCenter);
    m_message->setWordWrap(true);
    messageLayout->addWidget(m_message);
    messageLayout->addStretch();
    m_stack->addWidget(m_messagePage);
    m_stack->setCurrentWidget(m_messagePage);
    layout->addWidget(m_stack, 1);

    auto *navigator = m_view->pageNavigator();
    connect(m_previous, &QPushButton::clicked, this, [this, navigator]() {
        jumpToPage(navigator->currentPage() - 1);
    });
    connect(m_next, &QPushButton::clicked, this, [this, navigator]() {
        jumpToPage(navigator->currentPage() + 1);
    });
    connect(m_pageInput, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int pageNumber) {
        jumpToPage(pageNumber - 1);
    });
    connect(navigator, &QPdfPageNavigator::currentPageChanged,
            this, [this](int) { updatePageControls(); });
    connect(m_document, &QPdfDocument::pageCountChanged,
            this, [this](int) { updatePageControls(); });

    connect(m_zoomOut, &QPushButton::clicked,
            this, [this]() { changeZoom(1.0 / 1.25); });
    connect(m_zoomIn, &QPushButton::clicked,
            this, [this]() { changeZoom(1.25); });
    connect(m_fitWidth, &QPushButton::clicked, this, [this]() {
        changeFitMode(static_cast<int>(QPdfView::ZoomMode::FitToWidth));
    });
    connect(m_fitPage, &QPushButton::clicked, this, [this]() {
        changeFitMode(static_cast<int>(QPdfView::ZoomMode::FitInView));
    });
    connect(m_view, &QPdfView::zoomFactorChanged,
            this, [this](qreal) { updateZoomLabel(); });
    connect(m_view, &QPdfView::zoomModeChanged,
            this, [this](QPdfView::ZoomMode) { updateZoomLabel(); });

    connect(m_document, &QPdfDocument::statusChanged,
            this, [this](QPdfDocument::Status status) {
        handleStatus(status);
    });

    handleStatus(QPdfDocument::Status::Loading);

    // Loading is handled through statusChanged: no conversion or external app.
    const auto result = m_document->load(filePath);
    if (result != QPdfDocument::Error::None &&
        result != QPdfDocument::Error::DataNotYetAvailable) {
        handleStatus(QPdfDocument::Status::Error);
    } else {
        handleStatus(m_document->status());
    }

    updateZoomLabel();
}

void PdfViewerPage::handleStatus(QPdfDocument::Status status)
{
    const bool ready = status == QPdfDocument::Status::Ready;
    m_previous->setEnabled(ready);
    m_next->setEnabled(ready);
    m_pageInput->setEnabled(ready);
    m_zoomOut->setEnabled(ready);
    m_zoomIn->setEnabled(ready);
    m_fitWidth->setEnabled(ready);
    m_fitPage->setEnabled(ready);

    if (ready) {
        m_stack->setCurrentWidget(m_view);
        updatePageControls();
        return;
    }

    if (status == QPdfDocument::Status::Error) {
        QString details = QStringLiteral("The file may be damaged or is not a valid PDF.");
        switch (m_document->error()) {
        case QPdfDocument::Error::IncorrectPassword:
            details = QStringLiteral("This PDF is password protected.");
            break;
        case QPdfDocument::Error::UnsupportedSecurityScheme:
            details = QStringLiteral("This PDF uses an unsupported protection method.");
            break;
        case QPdfDocument::Error::FileNotFound:
            details = QStringLiteral("The PDF file could not be found.");
            break;
        default:
            break;
        }
        m_message->setText(QStringLiteral("Could not open PDF\n\n%1").arg(details));
    } else {
        m_message->setText(QStringLiteral("Loading PDF…"));
    }
    m_stack->setCurrentWidget(m_messagePage);
}

void PdfViewerPage::jumpToPage(int pageIndex)
{
    if (m_document->status() != QPdfDocument::Status::Ready ||
        pageIndex < 0 || pageIndex >= m_document->pageCount())
        return;

    m_view->pageNavigator()->jump(pageIndex, QPointF(0, 0));
}

void PdfViewerPage::updatePageControls()
{
    const int pageCount = m_document->pageCount();
    const int currentPage = m_view->pageNavigator()->currentPage();
    {
        const QSignalBlocker blocker(m_pageInput);
        m_pageInput->setRange(1, std::max(1, pageCount));
        m_pageInput->setValue(std::clamp(currentPage + 1, 1, std::max(1, pageCount)));
    }
    m_pageTotal->setText(QStringLiteral("/ %1").arg(pageCount));

    const bool ready = m_document->status() == QPdfDocument::Status::Ready;
    m_previous->setEnabled(ready && currentPage > 0);
    m_next->setEnabled(ready && currentPage + 1 < pageCount);
}


bool PdfViewerPage::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_view->viewport() && event->type() == QEvent::Wheel) {
        auto *wheel = static_cast<QWheelEvent *>(event);

        if (wheel->modifiers().testFlag(Qt::ControlModifier)) {
            if (m_document->status() == QPdfDocument::Status::Ready) {
                // A standard wheel notch is 120 angle units; trackpads may
                // report smaller angles or only pixel deltas.
                qreal steps = wheel->angleDelta().y() / 120.0;
                if (qFuzzyIsNull(steps))
                    steps = wheel->pixelDelta().y() / 100.0;

                if (!qFuzzyIsNull(steps))
                    queueZoom(std::pow(1.15, steps), wheel->position());
            }

            // Ctrl+wheel zooms; don't scroll the document as well.
            wheel->accept();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

qreal PdfViewerPage::effectiveZoomFactor() const
{
    if (m_view->zoomMode() == QPdfView::ZoomMode::Custom)
        return m_view->zoomFactor();

    if (m_document->status() != QPdfDocument::Status::Ready ||
        m_document->pageCount() == 0)
        return 1.0;

    const int page = std::clamp(m_view->pageNavigator()->currentPage(),
                                0, m_document->pageCount() - 1);
    const QSizeF points = m_document->pagePointSize(page);
    if (points.isEmpty())
        return 1.0;

    // QPdfView renders PDF points using logical screen DPI / 72.
    const auto *screen = QGuiApplication::primaryScreen();
    const qreal resolution = (screen ? screen->logicalDotsPerInch() : 96.0) / 72.0;
    const QMargins margins = m_view->documentMargins();
    const qreal availableWidth = std::max<qreal>(
        1.0, m_view->viewport()->width() - margins.left() - margins.right());
    const qreal fitWidth = availableWidth / (points.width() * resolution);

    if (m_view->zoomMode() == QPdfView::ZoomMode::FitToWidth)
        return fitWidth;

    const qreal availableHeight = std::max<qreal>(
        1.0, m_view->viewport()->height() - margins.top() - m_view->pageSpacing());
    return std::min(fitWidth, availableHeight / (points.height() * resolution));
}

void PdfViewerPage::beginZoomPreview(const QPointF &anchor)
{
    if (m_zoomPreview->isVisible())
        return;

    m_previewBaseZoom = effectiveZoomFactor();
    m_pendingZoom = m_previewBaseZoom;
    m_zoomPreview->capture(m_view->viewport()->grab());
    m_zoomPreview->setGeometry(m_view->viewport()->rect());
    m_zoomPreview->preview(1.0, anchor);
    m_zoomPreview->show();
    m_zoomPreview->raise();
}

void PdfViewerPage::queueZoom(qreal multiplier, const QPointF &anchor)
{
    if (m_document->status() != QPdfDocument::Status::Ready)
        return;

    beginZoomPreview(anchor);

    // Accumulate wheel events but rebuild PDF bitmaps only after input settles.
    // This avoids repeated cache invalidations for every trackpad delta.
    m_pendingZoom = std::clamp(m_pendingZoom * multiplier, 0.25, 4.0);
    m_zoomPreview->preview(m_pendingZoom / m_previewBaseZoom, anchor);
    m_waitingForRender = false;
    m_zoomTimer->start();
    updateZoomLabel();
}

void PdfViewerPage::applyPendingZoom()
{
    if (m_document->status() != QPdfDocument::Status::Ready) {
        finishZoomPreview();
        return;
    }

    const qreal existingZoom = effectiveZoomFactor();
    if (std::abs(m_pendingZoom - existingZoom) < 0.0001 &&
        m_view->zoomMode() == QPdfView::ZoomMode::Custom) {
        finishZoomPreview();
        return;
    }

    m_waitingForRender = true;
    ++m_renderGeneration;
    m_view->setZoomMode(QPdfView::ZoomMode::Custom);
    m_view->setZoomFactor(m_pendingZoom);
    updateZoomLabel();
    waitForRenderedPage();
}

void PdfViewerPage::waitForRenderedPage()
{
    const quint64 generation = m_renderGeneration;

    // Avoid an indefinitely frozen preview if rendering fails or the Qt
    // renderer implementation changes. Normally pageRendered removes it.
    QTimer::singleShot(3000, this, [this, generation]() {
        if (m_renderGeneration == generation && m_waitingForRender &&
            !m_zoomTimer->isActive())
            finishZoomPreview();
    });
}

void PdfViewerPage::finishZoomPreview()
{
    m_waitingForRender = false;
    m_zoomTimer->stop();
    if (m_zoomPreview->isVisible()) {
        // Paint the completed cache behind the snapshot before exposing it.
        m_view->viewport()->repaint();
        m_zoomPreview->hide();
    }
    updateZoomLabel();
}

void PdfViewerPage::changeFitMode(int mode)
{
    if (m_document->status() != QPdfDocument::Status::Ready)
        return;

    const auto newMode = static_cast<QPdfView::ZoomMode>(mode);
    if (m_view->zoomMode() == newMode && !m_zoomTimer->isActive())
        return;

    beginZoomPreview(QPointF(m_view->viewport()->rect().center()));
    m_zoomTimer->stop();
    m_waitingForRender = true;
    ++m_renderGeneration;

    m_view->setZoomMode(newMode);
    m_pendingZoom = effectiveZoomFactor();
    m_zoomPreview->preview(m_pendingZoom / m_previewBaseZoom,
                           QPointF(m_view->viewport()->rect().center()));

    updateZoomLabel();
    waitForRenderedPage();
}

void PdfViewerPage::changeZoom(qreal multiplier)
{
    queueZoom(multiplier, QPointF(m_view->viewport()->rect().center()));
}

void PdfViewerPage::updateZoomLabel()
{
    if (m_zoomPreview && m_zoomPreview->isVisible() &&
        m_zoomTimer && m_zoomTimer->isActive()) {
        m_zoomLabel->setText(QStringLiteral("%1%")
            .arg(qRound(m_pendingZoom * 100.0)));
        return;
    }
    switch (m_view->zoomMode()) {
    case QPdfView::ZoomMode::FitToWidth:
        m_zoomLabel->setText(QStringLiteral("Fit width"));
        break;
    case QPdfView::ZoomMode::FitInView:
        m_zoomLabel->setText(QStringLiteral("Fit page"));
        break;
    case QPdfView::ZoomMode::Custom:
        m_zoomLabel->setText(QStringLiteral("%1%")
            .arg(qRound(m_view->zoomFactor() * 100.0)));
        break;
    }
}
