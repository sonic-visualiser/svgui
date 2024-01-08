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

#ifndef SV_MENU_TITLE_H
#define SV_MENU_TITLE_H

#include "view/ViewManager.h"

#include <QStyle>
#include <QWidgetAction>
#include <QLabel>
#include <QApplication>
#include <QMenu>

namespace sv {

class MenuTitle
{
public:
    static void addTitle(QMenu *m, QString text) {

#ifdef Q_OS_LINUX
        static int leftIndent = 
            (ViewManager::scalePixelSize(8) +
             qApp->style()->pixelMetric(QStyle::PM_SmallIconSize));
#else
#ifdef Q_OS_WIN
        static int leftIndent =
            (9 + qApp->style()->pixelMetric(QStyle::PM_SmallIconSize));
#else
        static int leftIndent = 16;
#endif
#endif
        
        QWidgetAction *wa = new QWidgetAction(m);
        QLabel *title = new QLabel;
        title->setText(QObject::tr("<b>%1</b>")
                       .arg(XmlExportable::encodeEntities(text)));
        title->setMargin(ViewManager::scalePixelSize(3));
        title->setIndent(leftIndent);
        wa->setDefaultWidget(title);
        m->addAction(wa);
        m->addSeparator();
    }
};

} // end namespace sv

#endif
