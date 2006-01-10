/* -*- c-basic-offset: 4 -*-  vi:set ts=8 sts=4 sw=4: */

/*
    A waveform viewer and audio annotation editor.
    Chris Cannam, Queen Mary University of London, 2005
    
    This is experimental software.  Not for distribution.
*/

#ifndef _PAN_WIDGET_H_
#define _PAN_WIDGET_H_

#include "base/View.h"

#include <QPoint>

class QWidget;
class QPaintEvent;
class Layer;
class View;

#include <map>

class Panner : public View
{
    Q_OBJECT

public:
    Panner(QWidget *parent = 0);

    void registerView(View *widget);
    void unregisterView(View *widget);

    virtual QString getPropertyContainerIconName() const { return "panner"; }

public slots:
    virtual void modelChanged(size_t startFrame, size_t endFrame);
    virtual void modelReplaced();

    virtual void viewManagerCentreFrameChanged(void *, unsigned long, bool);
    virtual void viewManagerZoomLevelChanged(void *, unsigned long, bool);
    virtual void viewManagerPlaybackFrameChanged(unsigned long);

protected:
    virtual void paintEvent(QPaintEvent *e);
    virtual void mousePressEvent(QMouseEvent *e);
    virtual void mouseReleaseEvent(QMouseEvent *e);
    virtual void mouseMoveEvent(QMouseEvent *e);

    QPoint m_clickPos;
    QPoint m_mousePos;
    bool m_clickedInRange;
    size_t m_dragCentreFrame;
    
    typedef std::pair<size_t, int> WidgetRec; // centre, zoom (-1 = invalid)
    typedef std::map<void *, WidgetRec> WidgetMap;
    WidgetMap m_widgets;
};

#endif

