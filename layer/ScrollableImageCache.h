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

#ifndef SCROLLABLE_IMAGE_CACHE_H
#define SCROLLABLE_IMAGE_CACHE_H

#include "base/BaseTypes.h"

#include "view/LayerGeometryProvider.h"

#include <QImage>
#include <QRect>
#include <QPainter>

/**
 * A cached image for a view that scrolls horizontally, primarily the
 * spectrogram. The cache object holds an image, reports the size of
 * the image (likely the same as the underlying view, but it's the
 * caller's responsibility to set the size appropriately), can scroll
 * the image, and can report and update which contiguous horizontal
 * range of the image is valid.
 *
 * The only way to *update* the valid area in a cache is to draw to it
 * using the drawImage call.
 */
class ScrollableImageCache
{
public:
    ScrollableImageCache(const LayerGeometryProvider *v = 0) :
	m_v(v),
	m_left(0),
	m_width(0),
	m_startFrame(0),
	m_zoomLevel(0)
    {}

    void invalidate() {
	m_width = 0;
    }
    
    bool isValid() const {
	return m_width > 0;
    }

    bool spans(int left, int right) const {
	return (getValidLeft() <= left &&
		getValidRight() >= right);
    }
    
    QSize getSize() const {
	return m_image.size();
    }
    
    void resize(QSize newSize) {
	m_image = QImage(newSize, QImage::Format_ARGB32_Premultiplied);
	invalidate();
    }
	
    int getValidLeft() const {
	return m_left;
    }
    
    int getValidWidth() const {
	return m_width;
    }

    int getValidRight() const {
	return m_left + m_width;
    }

    QRect getValidArea() const {
	return QRect(m_left, 0, m_width, m_image.height());
    }
    
    int getZoomLevel() const {
	return m_zoomLevel;
    }

    sv_frame_t getStartFrame() const {
	return m_startFrame;
    }
    
    void setZoomLevel(int zoom) {
	m_zoomLevel = zoom;
	invalidate();
    }
    
    const QImage &getImage() const {
	return m_image;
    }
    
    void scrollTo(sv_frame_t newStartFrame) {

	if (!m_v) throw std::logic_error("ScrollableImageCache: not associated with a LayerGeometryProvider");
	
	int dx = (m_v->getXForFrame(m_startFrame) -
		  m_v->getXForFrame(newStartFrame));

	m_startFrame = newStartFrame;
	
	if (!isValid()) {
	    return;
	}

	int w = m_image.width();

	if (dx == 0) {
	    // haven't moved
	    return;
	}

	if (dx <= -w || dx >= w) {
	    // scrolled entirely off
	    invalidate();
	    return;
	}
	
	// dx is in range, cache is scrollable

	int dxp = dx;
	if (dxp < 0) dxp = -dxp;

	int copylen = (w - dxp) * int(sizeof(QRgb));
	for (int y = 0; y < m_image.height(); ++y) {
	    QRgb *line = (QRgb *)m_image.scanLine(y);
	    if (dx < 0) {
		memmove(line, line + dxp, copylen);
	    } else {
		memmove(line + dxp, line, copylen);
	    }
	}
	
	// update valid area
        
	int px = m_left;
	int pw = m_width;
	
	px += dx;
	
	if (dx < 0) {
	    // we scrolled left
	    if (px < 0) {
		pw += px;
		px = 0;
		if (pw < 0) {
		    pw = 0;
		}
	    }
	} else {
	    // we scrolled right
	    if (px + pw > w) {
		pw = w - px;
		if (pw < 0) {
		    pw = 0;
		}
	    }
	}

	m_left = px;
	m_width = pw;
    }

    void resizeToTouchValidArea(int &left, int &width,
				bool &isLeftOfValidArea) const {
	if (left < m_left) {
	    isLeftOfValidArea = true;
	    if (left + width < m_left + m_width) {
		width = m_left - left;
	    }
	} else {
	    isLeftOfValidArea = false;
	    width = left + width - (m_left + m_width);
	    left = m_left + m_width;
	    if (width < 0) width = 0;
	}
    }
    
    void drawImage(int left,
		   int width,
		   QImage image,
		   int imageLeft,
		   int imageWidth) {

	if (image.height() != m_image.height()) {
	    throw std::logic_error("Image height must match cache height in ScrollableImageCache::drawImage");
	}
	if (left < 0 || left + width > m_image.width()) {
	    throw std::logic_error("Drawing area out of bounds in ScrollableImageCache::drawImage");
	}
	
	QPainter painter(&m_image);
	painter.drawImage(QRect(left, 0, width, m_image.height()),
			  image,
			  QRect(imageLeft, 0, imageWidth, image.height()));
	painter.end();

	if (!isValid()) {
	    m_left = left;
	    m_width = width;
	    return;
	}
	
	if (left < m_left) {
	    if (left + width > m_left + m_width) {
		// new image completely contains the old valid area --
		// use the new area as is
		m_left = left;
		m_width = width;
	    } else if (left + width < m_left) {
		// new image completely off left of old valid area --
		// we can't extend the valid area because the bit in
		// between is not valid, so must use the new area only
		m_left = left;
		m_width = width;
	    } else {
		// new image overlaps old valid area on left side --
		// use new left edge, and extend width to existing
		// right edge
		m_width = (m_left + m_width) - left;
		m_left = left;
	    }
	} else {
	    if (left > m_left + m_width) {
		// new image completely off right of old valid area --
		// we can't extend the valid area because the bit in
		// between is not valid, so must use the new area only
		m_left = left;
		m_width = width;
	    } else if (left + width > m_left + m_width) {
		// new image overlaps old valid area on right side --
		// use existing left edge, and extend width to new
		// right edge
		m_width = (left + width) - m_left;
		// (m_left unchanged)
	    } else {
		// new image completely contained within old valid
		// area -- leave the old area unchanged
	    }
	}
    }
    
private:
    const LayerGeometryProvider *m_v;
    QImage m_image;
    int m_left;  // of valid region
    int m_width; // of valid region
    sv_frame_t m_startFrame;
    int m_zoomLevel;
};

#endif
