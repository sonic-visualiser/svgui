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

#include "SingleColourLayer.h"
#include "base/ColourDatabase.h"
#include "view/View.h"

#include <iostream>

#include <QApplication>

SingleColourLayer::ColourIndexPool 
SingleColourLayer::m_usedColourIndices;

SingleColourLayer::SingleColourLayer() :
    m_colour(0)
{
    setDefaultColourFor(0);
}

bool
SingleColourLayer::hasLightBackground() const
{
    bool dark = ColourDatabase::getInstance()->useDarkBackground(m_colour);
    return !dark;
}

Layer::PropertyList
SingleColourLayer::getProperties() const
{
    PropertyList list = Layer::getProperties();
    list.push_back("Colour");
    return list;
}

QString
SingleColourLayer::getPropertyLabel(const PropertyName &name) const
{
    if (name == "Colour") return tr("Colour");
    return "";
}

Layer::PropertyType
SingleColourLayer::getPropertyType(const PropertyName &name) const
{
    if (name == "Colour") return ColourProperty;
    return InvalidProperty;
}

QString
SingleColourLayer::getPropertyGroupName(const PropertyName &) const
{
    return QString();
}

int
SingleColourLayer::getPropertyRangeAndValue(const PropertyName &name,
                                        int *min, int *max, int *deflt) const
{
    int val = 0;

    int garbage0, garbage1, garbage2;
    if (!min) min = &garbage0;
    if (!max) max = &garbage1;
    if (!deflt) deflt = &garbage2;

    if (name == "Colour") {

        ColourDatabase::getInstance()->getColourPropertyRange(min, max);
        *deflt = 0; //!!!

        val = m_colour;

    } else {
	val = Layer::getPropertyRangeAndValue(name, min, max, deflt);
    }

    return val;
}

QString
SingleColourLayer::getPropertyValueLabel(const PropertyName &name,
				    int value) const
{
    if (name == "Colour") {
        return Layer::getPropertyValueLabel(name, value);
    }
    return tr("<unknown>");
}

RangeMapper *
SingleColourLayer::getNewPropertyRangeMapper(const PropertyName &) const
{
    return 0;
}

void
SingleColourLayer::setProperty(const PropertyName &name, int value)
{
    if (name == "Colour") {
        setBaseColour(value);
    }
}

void
SingleColourLayer::setDefaultColourFor(View *v)
{
    bool dark = false;
    if (v) {
        ColourIndexPool::iterator i = m_usedColourIndices.find(m_colour);
        if (i != m_usedColourIndices.end()) m_usedColourIndices.erase(i);
        dark = !v->hasLightBackground();
    } else {
        QColor bg = QApplication::palette().color(QPalette::Window);
        if (bg.red() + bg.green() + bg.blue() < 384) dark = true;
    }

    m_colour = -1;
    ColourDatabase *cdb = ColourDatabase::getInstance();

    int hint = -1;
    bool impose = false;
    if (v) {
        // We don't want to call this if !v because that probably
        // means we're being called from the constructor, and this is
        // a virtual function
        hint = getDefaultColourHint(dark, impose);
        std::cerr << "hint = " << hint << ", impose = " << impose << std::endl;
    }

    if (hint >= 0 && impose) {
        m_colour = hint;
        m_usedColourIndices.insert(m_colour);
        return;
    }

    for (int i = 0; i < cdb->getColourCount(); ++i) {
        int index = i;
        if (hint > 0) index = (index + hint) % cdb->getColourCount();
        if (cdb->useDarkBackground(index) != dark) continue;
        if (m_colour < 0) m_colour = index;
        if (m_usedColourIndices.find(index) == m_usedColourIndices.end()) {
            m_colour = index;
            break;
        }
    }

    if (m_colour < 0) m_colour = 0;
    m_usedColourIndices.insert(m_colour);
}

void
SingleColourLayer::setBaseColour(int colour)
{
    if (m_colour == colour) return;
    ColourIndexPool::iterator i = m_usedColourIndices.find(m_colour);
    if (i != m_usedColourIndices.end()) m_usedColourIndices.erase(i);
    m_colour = colour;
    m_usedColourIndices.insert(m_colour);
    flagBaseColourChanged();
    emit layerParametersChanged();
}

int
SingleColourLayer::getBaseColour() const
{
    return m_colour;
}

QColor
SingleColourLayer::getBaseQColor() const
{
    return ColourDatabase::getInstance()->getColour(m_colour);
}

QColor
SingleColourLayer::getBackgroundQColor(View *v) const
{
    return v->getBackground();
}

QColor
SingleColourLayer::getForegroundQColor(View *v) const
{
    return v->getForeground();
}

std::vector<QColor>
SingleColourLayer::getPartialShades(View *v) const
{
    std::vector<QColor> s;
    QColor base = getBaseQColor();
    QColor bg = getBackgroundQColor(v);
    for (int i = 0; i < 3; ++i) {
        int red = base.red() + ((bg.red() - base.red()) * (i + 1)) / 4;
        int green = base.green() + ((bg.green() - base.green()) * (i + 1)) / 4;
        int blue = base.blue() + ((bg.blue() - base.blue()) * (i + 1)) / 4;
        s.push_back(QColor(red, green, blue));
    }
    return s;
}

QString
SingleColourLayer::toXmlString(QString indent, QString extraAttributes) const
{
    QString s;
    
    QString colourName, colourSpec, darkbg;
    ColourDatabase::getInstance()->getStringValues
        (m_colour, colourName, colourSpec, darkbg);

    s += QString("colourName=\"%1\" "
                 "colour=\"%2\" "
                 "darkBackground=\"%3\" ")
	.arg(colourName)
        .arg(colourSpec)
        .arg(darkbg);

    return Layer::toXmlString(indent, extraAttributes + " " + s);
}

void
SingleColourLayer::setProperties(const QXmlAttributes &attributes)
{
    QString colourName = attributes.value("colourName");
    QString colourSpec = attributes.value("colour");
    QString darkbg = attributes.value("darkBackground");
    m_colour = ColourDatabase::getInstance()->putStringValues
        (colourName, colourSpec, darkbg);
}

