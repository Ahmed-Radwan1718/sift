#include "PdfViewerPage.h"

#include <QCursor>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMargins>
#include <QPdfPageNavigator>
#include <QPdfView>
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
    m_view->viewport()->setMouseTracking(true);

    // The toolbar is a child of the PDF viewport, not a layout row:
    // it floats over the document without reducing the reading area.
    m_floatingControls = new QFrame(m_view->viewport());
    m_floatingControls->setObjectName(QStringLiteral("floatingPdfToolbar"));
    auto *controls = new QHBoxLayout(m_floatingControls);
    controls->setContentsMargins(12, 9, 12, 9);
    controls->setSpacing(5);

    m_previous = toolbarButton(QStringLiteral("‹"), QStringLiteral("Previous page"),
                               m_floatingControls);
    m_next = toolbarButton(QStringLiteral("›"), QStringLiteral("Next page"),
                           m_floatingControls);
    m_previous->setFixedSize(29, 29);
    m_next->setFixedSize(29, 29);

    m_pageInput = new QSpinBox(m_floatingControls);
    m_pageInput->setObjectName(QStringLiteral("floatingPdfPageInput"));
    m_pageInput->setRange(1, 1);
    m_pageInput->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_pageInput->setAlignment(Qt::AlignCenter);
    m_pageInput->setFixedSize(45, 29);
    m_pageInput->setToolTip(QStringLiteral("Jump to page"));

    m_pageTotal = new QLabel(QStringLiteral("/ 0"), m_floatingControls);
    m_pageTotal->setObjectName(QStringLiteral("floatingPdfText"));
    m_pageTotal->setMinimumWidth(35);

    auto addDivider = [this, controls]() {
        auto *divider = new QFrame(m_floatingControls);
        divider->setObjectName(QStringLiteral("floatingPdfDivider"));
        divider->setFixedSize(1, 20);
        controls->addSpacing(8);
        controls->addWidget(divider);
        controls->addSpacing(8);
    };

    controls->addWidget(m_previous);
    controls->addWidget(m_pageInput);
    controls->addWidget(m_pageTotal);
    controls->addWidget(m_next);
    addDivider();

    m_zoomOut = toolbarButton(QStringLiteral("−"), QStringLiteral("Zoom out"),
                              m_floatingControls);
    m_zoomIn = toolbarButton(QStringLiteral("+"), QStringLiteral("Zoom in"),
                             m_floatingControls);
    m_zoomOut->setFixedSize(29, 29);
    m_zoomIn->setFixedSize(29, 29);
    m_zoomLabel = new QLabel(QStringLiteral("Fit width"), m_floatingControls);
    m_zoomLabel->setObjectName(QStringLiteral("floatingPdfText"));
    m_zoomLabel->setFixedWidth(65);
    m_zoomLabel->setAlignment(Qt::AlignCenter);

    controls->addWidget(m_zoomOut);
    controls->addWidget(m_zoomLabel);
    controls->addWidget(m_zoomIn);
    addDivider();

    m_fitWidth = toolbarButton(QStringLiteral("↔"),
                               QStringLiteral("Fit page width"), m_floatingControls);
    m_fitPage = toolbarButton(QStringLiteral("⛶"),
                              QStringLiteral("Fit entire page"), m_floatingControls);
    m_fitWidth->setFixedSize(29, 29);
    m_fitPage->setFixedSize(29, 29);
    controls->addWidget(m_fitWidth);
    controls->addWidget(m_fitPage);

    m_floatingControls->adjustSize();
    m_floatingControls->hide();

    m_controlsHideTimer = new QTimer(this);
    m_controlsHideTimer->setSingleShot(true);
    connect(m_controlsHideTimer, &QTimer::timeout, this, [this]() {
        if (!m_floatingControls || !m_floatingControls->isVisible())
            return;
        // A user interacting with the controls should never lose them.
        const QPoint pointer = m_floatingControls->mapFromGlobal(QCursor::pos());
        if (m_floatingControls->rect().contains(pointer)) {
            m_controlsHideTimer->start(1200);
            return;
        }
        m_floatingControls->hide();
    });

    // Coalesce rapid Ctrl+wheel events into one PDF cache update.
    // Do not cover the viewport: ordinary scrolling must never be blocked
    // while Qt renders pages at the new zoom level.
    m_zoomTimer = new QTimer(this);
    m_zoomTimer->setSingleShot(true);
    m_zoomTimer->setInterval(45);
    connect(m_zoomTimer, &QTimer::timeout,
            this, [this]() { applyPendingZoom(); });

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
        m_zoomTimer->stop();
        m_view->setZoomMode(QPdfView::ZoomMode::FitToWidth);
        updateZoomLabel();
    });
    connect(m_fitPage, &QPushButton::clicked, this, [this]() {
        m_zoomTimer->stop();
        m_view->setZoomMode(QPdfView::ZoomMode::FitInView);
        updateZoomLabel();
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
        showFloatingControls();
        return;
    }

    if (m_floatingControls)
        m_floatingControls->hide();

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
    if (watched == m_view->viewport()) {
        if (event->type() == QEvent::Resize) {
            positionFloatingControls();
        } else if (event->type() == QEvent::MouseMove ||
                   event->type() == QEvent::Enter) {
            // Reveal controls when the pointer approaches the bottom edge.
            const QPoint pointer = m_view->viewport()->mapFromGlobal(QCursor::pos());
            if (pointer.y() >= m_view->viewport()->height() - 110 &&
                m_document->status() == QPdfDocument::Status::Ready)
                showFloatingControls();
        } else if (event->type() == QEvent::Wheel) {
            auto *wheel = static_cast<QWheelEvent *>(event);
            if (wheel->modifiers().testFlag(Qt::ControlModifier)) {
                if (m_document->status() == QPdfDocument::Status::Ready) {
                    qreal steps = wheel->angleDelta().y() / 120.0;
                    if (qFuzzyIsNull(steps))
                        steps = wheel->pixelDelta().y() / 100.0;
                    if (!qFuzzyIsNull(steps)) {
                        queueZoom(std::pow(1.15, steps));
                        showFloatingControls();
                    }
                }
                wheel->accept();
                return true;
            }
        }
    }
    // The PDF viewer keeps handling regular wheel events and scrolling.
    return QWidget::eventFilter(watched, event);
}

void PdfViewerPage::positionFloatingControls()
{
    if (!m_floatingControls || !m_view)
        return;

    const QRect viewportRect = m_view->viewport()->rect();
    const int width = std::min(m_floatingControls->sizeHint().width(),
                               std::max(1, viewportRect.width() - 16));
    const int height = m_floatingControls->sizeHint().height();
    if (viewportRect.width() < 1 || viewportRect.height() < 1)
        return;

    const int x = (viewportRect.width() - width) / 2;
    const int y = std::max(8, viewportRect.height() - height - 20);
    m_floatingControls->setGeometry(x, y, width, height);
    m_floatingControls->raise();
}

void PdfViewerPage::showFloatingControls()
{
    if (!m_floatingControls || m_document->status() != QPdfDocument::Status::Ready)
        return;
    positionFloatingControls();
    m_floatingControls->show();
    m_floatingControls->raise();
    m_controlsHideTimer->start(2600);
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

void PdfViewerPage::queueZoom(qreal multiplier)
{
    if (m_document->status() != QPdfDocument::Status::Ready)
        return;

    // Accumulate rapid events without covering the document or stalling
    // normal scrolling. The actual PDF redraw is triggered just once.
    const qreal base = m_zoomTimer->isActive()
        ? m_pendingZoom : effectiveZoomFactor();
    m_pendingZoom = std::clamp(base * multiplier, 0.25, 4.0);
    m_zoomTimer->start();
    updateZoomLabel();
}

void PdfViewerPage::applyPendingZoom()
{
    if (m_document->status() != QPdfDocument::Status::Ready)
        return;

    m_view->setZoomMode(QPdfView::ZoomMode::Custom);
    m_view->setZoomFactor(m_pendingZoom);
    updateZoomLabel();
}

void PdfViewerPage::changeZoom(qreal multiplier)
{
    queueZoom(multiplier);
}

void PdfViewerPage::updateZoomLabel()
{
    if (m_zoomTimer && m_zoomTimer->isActive()) {
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
