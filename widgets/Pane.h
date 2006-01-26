
/* -*- c-basic-offset: 4 -*-  vi:set ts=8 sts=4 sw=4: */

/*
    A waveform viewer and audio annotation editor.
    Chris Cannam, Queen Mary University of London, 2005-2006
    
    This is experimental software.  Not for distribution.
*/

#ifndef _PANE_H_
#define _PANE_H_

#include <QFrame>
#include <QPoint>

#include "base/ZoomConstraint.h"
#include "base/View.h"
#include "base/Selection.h"

class QWidget;
class QPaintEvent;
class Layer;

class Pane : public View
{
    Q_OBJECT

public:
    Pane(QWidget *parent = 0);
    virtual QString getPropertyContainerIconName() const { return "pane"; }

    virtual bool shouldIlluminateLocalFeatures(const Layer *layer, QPoint &pos);

    void setCentreLineVisible(bool visible);
    bool getCentreLineVisible() const { return m_centreLineVisible; }

    virtual QString toXmlString(QString indent = "",
				QString extraAttributes = "") const;

signals:
    void paneInteractedWith();

public slots:
    virtual void toolModeChanged();

protected:
    virtual void paintEvent(QPaintEvent *e);
    virtual void mousePressEvent(QMouseEvent *e);
    virtual void mouseReleaseEvent(QMouseEvent *e);
    virtual void mouseMoveEvent(QMouseEvent *e);
    virtual void mouseDoubleClickEvent(QMouseEvent *e);
    virtual void leaveEvent(QEvent *e);
    virtual void wheelEvent(QWheelEvent *e);

    Selection getSelectionAt(int x, bool &closeToLeft, bool &closeToRight);

    bool m_identifyFeatures;
    QPoint m_identifyPoint;
    QPoint m_clickPos;
    QPoint m_mousePos;
    bool m_clickedInRange;
    bool m_shiftPressed;
    bool m_ctrlPressed;
    bool m_navigating;
    bool m_resizing;
    size_t m_dragCentreFrame;
    bool m_centreLineVisible;
    size_t m_selectionStartFrame;
};

#endif

