#pragma once

#include "PptxDocument.h"

#include <QWidget>

class QEvent;
class QLabel;
class QListWidget;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QTimer;
class QResizeEvent;
class QWidget;

// A local, read-only PowerPoint slide viewer. Each tab owns its presentation.
class PresentationViewerPage final : public QWidget
{
public:
    explicit PresentationViewerPage(const QString &filePath, QWidget *parent = nullptr);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void goToSlide(int index);
    void updateSlide();
    void changeZoom(qreal factor);
    void fitPage();
    void fitWidth();
    void addNextThumbnail();
    QSize targetSize() const;

    PptxDocument m_document;
    QListWidget *m_thumbnails = nullptr;
    QScrollArea *m_scroll = nullptr;
    QLabel *m_slide = nullptr;
    QLabel *m_pageTotal = nullptr;
    QLineEdit *m_pageNumber = nullptr;
    QLabel *m_zoomLabel = nullptr;
    QLabel *m_warning = nullptr;
    QPushButton *m_previous = nullptr;
    QPushButton *m_next = nullptr;
    QPushButton *m_zoomIn = nullptr;
    QPushButton *m_zoomOut = nullptr;
    QTimer *m_resizeTimer = nullptr;
    int m_current = 0;
    int m_nextThumbnail = 0;
    qreal m_zoom = 1.0;
    enum class ZoomMode { FitPage, FitWidth, Custom };
    ZoomMode m_zoomMode = ZoomMode::FitPage;
};
