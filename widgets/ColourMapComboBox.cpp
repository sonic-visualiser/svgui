/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2007-2016 QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "ColourMapComboBox.h"

#include "layer/ColourMapper.h"

#include "base/Debug.h"

#include <QFontMetrics>

#include <iostream>

using namespace std;

ColourMapComboBox::ColourMapComboBox(QWidget *parent) :
    NotifyingComboBox(parent)
{
    setEditable(false);
    rebuild();

    connect(this, SIGNAL(activated(int)), this, SLOT(comboActivated(int)));

    if (count() < 20 && count() > maxVisibleItems()) {
	setMaxVisibleItems(count());
    }
}

void
ColourMapComboBox::comboActivated(int index)
{
    emit colourMapChanged(index);
}

void
ColourMapComboBox::rebuild()
{
    blockSignals(true);

    int ix = currentIndex();
    
    clear();

    int size = (QFontMetrics(QFont()).height() * 2) / 3;
    if (size < 12) size = 12;

    for (int i = 0; i < ColourMapper::getColourMapCount(); ++i) {
	QString name = ColourMapper::getColourMapName(i);
//	addItem(db->getExamplePixmap(i, QSize(size, size)), name);
        addItem(name);
    }

    setCurrentIndex(ix);
    
    blockSignals(false);
}

