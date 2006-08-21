
/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef _PANE_H_
#define _PANE_H_

#include <QFrame>
#include <QPoint>

#include "base/ZoomConstraint.h"
#include "View.h"
#include "base/Selection.h"

class QWidget;
class QPaintEvent;
class Layer;
class Thumbwheel;

class Pane : public View
{
    Q_OBJECT

public:
    Pane(QWidget *parent = 0);
    virtual QString getPropertyContainerIconName() const { return "pane"; }

    virtual bool shouldIlluminateLocalFeatures(const Layer *layer,
					       QPoint &pos) const;
    virtual bool shouldIlluminateLocalSelection(QPoint &pos,
						bool &closeToLeft,
						bool &closeToRight) const;

    void setCentreLineVisible(bool visible);
    bool getCentreLineVisible() const { return m_centreLineVisible; }

    virtual QString toXmlString(QString indent = "",
				QString extraAttributes = "") const;

signals:
    void paneInteractedWith();
    void rightButtonMenuRequested(QPoint position);

public slots:
    virtual void toolModeChanged();
    virtual void zoomWheelsEnabledChanged();
    virtual void zoomLevelChanged();

    virtual void horizontalThumbwheelMoved(int value);
    virtual void verticalThumbwheelMoved(int value);
    virtual void verticalZoomChanged();

    virtual void propertyContainerSelected(View *, PropertyContainer *pc);

protected:
    virtual void paintEvent(QPaintEvent *e);
    virtual void mousePressEvent(QMouseEvent *e);
    virtual void mouseReleaseEvent(QMouseEvent *e);
    virtual void mouseMoveEvent(QMouseEvent *e);
    virtual void mouseDoubleClickEvent(QMouseEvent *e);
    virtual void leaveEvent(QEvent *e);
    virtual void wheelEvent(QWheelEvent *e);
    virtual void resizeEvent(QResizeEvent *e);

    Selection getSelectionAt(int x, bool &closeToLeft, bool &closeToRight) const;

    bool editSelectionStart(QMouseEvent *e);
    bool editSelectionDrag(QMouseEvent *e);
    bool editSelectionEnd(QMouseEvent *e);
    bool selectionIsBeingEdited() const;

    void updateHeadsUpDisplay();

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
    float m_dragStartMinValue;
    bool m_centreLineVisible;
    size_t m_selectionStartFrame;
    Selection m_editingSelection;
    int m_editingSelectionEdge;

    QWidget *m_headsUpDisplay;
    Thumbwheel *m_hthumb;
    Thumbwheel *m_vthumb;
};

#endif

