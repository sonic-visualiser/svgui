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

#ifndef SV_IMAGE_REGION_FINDER_H
#define SV_IMAGE_REGION_FINDER_H

#include <QColor>
#include <QRect>

class QImage;

namespace sv {

class ImageRegionFinder
{
public:
    ImageRegionFinder();
    virtual ~ImageRegionFinder();

    QRect findRegionExtents(QImage *image, QPoint origin) const;

protected:
    bool similar(QRgb a, QRgb b) const;
};

} // end namespace sv

#endif

