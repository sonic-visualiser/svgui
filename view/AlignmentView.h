/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006-2014 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef ALIGNMENT_VIEW_H
#define ALIGNMENT_VIEW_H

#include "View.h"

class AlignmentView : public View
{
    Q_OBJECT

public:
    AlignmentView(QWidget *parent = 0);
    virtual QString getPropertyContainerIconName() const { return "alignment"; }
    
    void setViewAbove(View *view);
    void setViewBelow(View *view);

public slots:
    virtual void globalCentreFrameChanged(int);
    virtual void viewCentreFrameChanged(View *, int);
    virtual void viewAboveZoomLevelChanged(int, bool);
    virtual void viewBelowZoomLevelChanged(int, bool);
    virtual void viewManagerPlaybackFrameChanged(int);

protected:
    virtual void paintEvent(QPaintEvent *e);
    virtual bool shouldLabelSelections() const { return false; }

    std::vector<int> getKeyFrames();
    std::vector<int> getDefaultKeyFrames();

    View *m_above;
    View *m_below;
};

#endif
