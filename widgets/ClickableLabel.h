/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_CLICKABLE_LABEL_H
#define SV_CLICKABLE_LABEL_H

#include <QLabel>

namespace sv {

class ClickableLabel : public QLabel
{
    Q_OBJECT

public:
    ClickableLabel(const QString &text, QWidget *parent = 0) :
        QLabel(text, parent) { }
    ClickableLabel(QWidget *parent = 0) : QLabel(parent) { }
    ~ClickableLabel() { }

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent *) override {
        emit clicked();
    }
};

} // end namespace sv

#endif
