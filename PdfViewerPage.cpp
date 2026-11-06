#include "PdfViewerPage.h"

#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPdfPageNavigator>
#include <QPdfView>
#include <QPointF>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

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
        m_view->setZoomMode(QPdfView::ZoomMode::FitToWidth);
        updateZoomLabel();
    });
    connect(m_fitPage, &QPushButton::clicked, this, [this]() {
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

void PdfViewerPage::changeZoom(qreal multiplier)
{
    const qreal base = m_view->zoomMode() == QPdfView::ZoomMode::Custom
                           ? m_view->zoomFactor() : 1.0;
    const qreal next = std::clamp(base * multiplier, 0.25, 4.0);
    m_view->setZoomMode(QPdfView::ZoomMode::Custom);
    m_view->setZoomFactor(next);
    updateZoomLabel();
}

void PdfViewerPage::updateZoomLabel()
{
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
