#pragma once

#include <QPdfDocument>
#include <QWidget>

class QEvent;
class QLabel;
class QPushButton;
class QPdfView;
class QSpinBox;
class QStackedWidget;

// Read-only PDF viewer: each tab owns its own document and controls.
class PdfViewerPage final : public QWidget
{
public:
    explicit PdfViewerPage(const QString &filePath, QWidget *parent = nullptr);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void handleStatus(QPdfDocument::Status status);
    void updatePageControls();
    void updateZoomLabel();
    void jumpToPage(int pageIndex);
    void changeZoom(qreal multiplier);
    qreal effectiveZoomFactor() const;

    QPdfDocument *m_document = nullptr;
    QPdfView *m_view = nullptr;
    QStackedWidget *m_stack = nullptr;
    QWidget *m_messagePage = nullptr;
    QLabel *m_message = nullptr;
    QSpinBox *m_pageInput = nullptr;
    QLabel *m_pageTotal = nullptr;
    QLabel *m_zoomLabel = nullptr;
    QPushButton *m_previous = nullptr;
    QPushButton *m_next = nullptr;
    QPushButton *m_zoomOut = nullptr;
    QPushButton *m_zoomIn = nullptr;
    QPushButton *m_fitWidth = nullptr;
    QPushButton *m_fitPage = nullptr;
};
