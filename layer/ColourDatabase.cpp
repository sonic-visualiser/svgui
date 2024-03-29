/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2007 QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "ColourDatabase.h"
#include "base/XmlExportable.h"

#include <QPainter>

//#define DEBUG_COLOUR_DATABASE 1

namespace sv {

ColourDatabase
ColourDatabase::m_instance;

ColourDatabase *
ColourDatabase::getInstance()
{
    return &m_instance;
}

ColourDatabase::ColourDatabase()
{
}

int
ColourDatabase::getColourCount() const
{
    return int(m_colours.size());
}

QString
ColourDatabase::getColourName(int c) const
{
    if (!in_range_for(m_colours, c)) return "";
    return m_colours[c].name;
}

QColor
ColourDatabase::getColour(int c) const
{
    if (!in_range_for(m_colours, c)) return Qt::black;
    return m_colours[c].colour;
}

QColor
ColourDatabase::getColour(QString name) const
{
    for (auto &c: m_colours) {
        if (c.name == name) return c.colour;
    }

    return Qt::black;
}

int
ColourDatabase::getColourIndex(QString name) const
{
    int index = 0;
    for (auto &c: m_colours) {
        if (c.name == name) return index;
        ++index;
    }

    return -1;
}

int
ColourDatabase::getColourIndex(QColor col) const
{
    int index = 0;
    for (auto &c: m_colours) {
        if (c.colour == col) return index;
        ++index;
    }

    return -1;
}

int
ColourDatabase::getNearbyColourIndex(QColor col, WithBackgroundMode mode) const
{
    int index = -1;
    int closestIndex = -1;
    double closestDistance = 0;

    for (auto &c: m_colours) {

        ++index;

        if (mode == WithDarkBackground && !c.darkbg) {
#ifdef DEBUG_COLOUR_DATABASE
            SVDEBUG << "getNearbyColourIndex: dark background requested, skipping " << c.colour.name() << endl;
#endif
            continue;
        }
        if (mode == WithLightBackground && c.darkbg) {
#ifdef DEBUG_COLOUR_DATABASE
            SVDEBUG << "getNearbyColourIndex: light background requested, skipping " << c.colour.name() << endl;
#endif
            continue;
        }

        // This distance formula is "one of the better low-cost
        // approximations" according to
        // https://en.wikipedia.org/w/index.php?title=Color_difference&oldid=936888327
        
        double r1 = col.red(), r2 = c.colour.red();
        double g1 = col.green(), g2 = c.colour.green();
        double b1 = col.blue(), b2 = c.colour.blue();

        double rav = (r1 + r2) / 2.0;
        double rterm = (2.0 + rav / 256.0) * (r1 - r2) * (r1 - r2);
        double gterm = 4.0 * (g1 - g2) * (g1 - g2);
        double bterm = (2.0 + (255 - rav) / 256.0) * (b1 - b2) * (b1 - b2);
        
        double distance = sqrt(rterm + gterm + bterm);

#ifdef DEBUG_COLOUR_DATABASE
        SVDEBUG << "getNearbyColourIndex: comparing " << c.colour.name()
                << " to " << col.name() << ": distance = " << distance << endl;
#endif
        if (closestIndex < 0 || distance < closestDistance) {
            closestIndex = index;
            closestDistance = distance;
#ifdef DEBUG_COLOUR_DATABASE
            SVDEBUG << "(this is the best so far)" << endl;
#endif
        }
    }

#ifdef DEBUG_COLOUR_DATABASE
    SVDEBUG << "returning " << closestIndex << endl;
#endif
    return closestIndex;
}

QColor
ColourDatabase::getContrastingColour(int c) const
{
    QColor col = getColour(c);
    QColor contrasting = Qt::red;
    bool dark = (col.red() < 240 && col.green() < 240 && col.blue() < 240);
    if (dark) {
        if (col.red() > col.blue()) {
            if (col.green() > col.blue()) {
                contrasting = Qt::blue;
            } else {
                contrasting = Qt::yellow;
            }
        } else {
            if (col.green() > col.blue()) {
                contrasting = Qt::yellow;
            } else {
                contrasting = Qt::red;
            }
        }
    } else {
        if (col.red() > 230 && col.green() > 230 && col.blue() > 230) {
            contrasting = QColor(30, 150, 255);
        } else {
            contrasting = QColor(255, 188, 80);
        }
    }
#ifdef DEBUG_COLOUR_DATABASE
    SVDEBUG << "getContrastingColour(" << col.name() << "): dark = " << dark
            << ", returning " << contrasting.name() << endl;
#endif
    return contrasting;
}

bool
ColourDatabase::useDarkBackground(int c) const
{
    if (!in_range_for(m_colours, c)) return false;
    return m_colours[c].darkbg;
}

void
ColourDatabase::setUseDarkBackground(int c, bool dark)
{
    if (!in_range_for(m_colours, c)) return;
    if (m_colours[c].darkbg != dark) {
        m_colours[c].darkbg = dark;
        emit colourDatabaseChanged();
    }
}

int
ColourDatabase::addColour(QColor c, QString name)
{
    int index = 0;

    for (ColourList::iterator i = m_colours.begin();
         i != m_colours.end(); ++i) {
        if (i->name == name) {
            i->colour = c;
            return index;
        }
        ++index;
    }

    ColourRec rec;
    rec.colour = c;
    rec.name = name;
    rec.darkbg = false;
    m_colours.push_back(rec);
    emit colourDatabaseChanged();
    return index;
}

void
ColourDatabase::removeColour(QString name)
{
    for (ColourList::iterator i = m_colours.begin();
         i != m_colours.end(); ++i) {
        if (i->name == name) {
            m_colours.erase(i);
            return;
        }
    }
}

void
ColourDatabase::getStringValues(int index,
                                QString &colourName,
                                QString &colourSpec,
                                QString &darkbg) const
{
    colourName = "";
    colourSpec = "";
    if (!in_range_for(m_colours, index)) return;

    colourName = getColourName(index);
    QColor c = getColour(index);
    colourSpec = XmlExportable::encodeColour(c.red(), c.green(), c.blue());
    darkbg = useDarkBackground(index) ? "true" : "false";
}

int
ColourDatabase::putStringValues(QString colourName,
                                QString colourSpec,
                                QString darkbg)
{
    int index = -1;
    if (colourSpec != "") {
        QColor colour(colourSpec);
        index = getColourIndex(colour);
        if (index < 0) {
            index = addColour(colour,
                              colourName == "" ? colourSpec : colourName);
        }
    } else if (colourName != "") {
        index = getColourIndex(colourName);
    }
    if (index >= 0) {
        setUseDarkBackground(index, darkbg == "true");
    }
    return index;
}

void
ColourDatabase::getColourPropertyRange(int *min, int *max) const
{
    ColourDatabase *db = getInstance();
    if (min) *min = 0;
    if (max) {
        *max = 0;
        if (db->getColourCount() > 0) *max = db->getColourCount()-1;
    }
}

QPixmap
ColourDatabase::getExamplePixmap(int index, QSize size) const
{
    QPixmap pmap(size);
    pmap.fill(useDarkBackground(index) ? Qt::black : Qt::white);
    QPainter paint(&pmap);
    QColor colour(getColour(index));
    paint.setPen(colour);
    paint.setBrush(colour);
    int margin = 2;
    if (size.width() < 4 || size.height() < 4) margin = 0;
    else if (size.width() < 8 || size.height() < 8) margin = 1;
    paint.drawRect(margin, margin,
                   size.width() - margin*2 - 1, size.height() - margin*2 - 1);
    return pmap;
}

} // end namespace sv

