/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006-2016 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "Colour3DPlotRenderer.h"
#include "RenderTimer.h"

#include "data/model/DenseThreeDimensionalModel.h"
#include "data/model/Dense3DModelPeakCache.h"
#include "data/model/FFTModel.h"

#include "LayerGeometryProvider.h"

#include <vector>

using namespace std;

Colour3DPlotRenderer::RenderResult
Colour3DPlotRenderer::render(QPainter &paint, QRect rect)
{
    return render(paint, rect, false);
}

Colour3DPlotRenderer::RenderResult
Colour3DPlotRenderer::renderTimeConstrained(QPainter &paint, QRect rect)
{
    return render(paint, rect, true);
}

Colour3DPlotRenderer::RenderResult
Colour3DPlotRenderer::render(QPainter &paint, QRect rect, bool timeConstrained)
{
    LayerGeometryProvider *v = m_sources.geometryProvider;
    if (!v) {
	throw std::logic_error("no LayerGeometryProvider provided");
    }

    sv_frame_t startFrame = v->getStartFrame();
    int zoomLevel = v->getZoomLevel();
    
    int x0 = v->getXForViewX(rect.x());
    int x1 = v->getXForViewX(rect.x() + rect.width());
    if (x0 < 0) x0 = 0;
    if (x1 > v->getPaintWidth()) x1 = v->getPaintWidth();

    m_cache.resize(v->getPaintSize());
    m_cache.setZoomLevel(v->getZoomLevel());

    if (m_cache.isValid()) { // some part of the cache is valid

        if (v->getXForFrame(m_cache.getStartFrame()) ==
            v->getXForFrame(startFrame) &&
            m_cache.getValidLeft() <= x0 &&
            m_cache.getValidRight() >= x1) {
                
            // cache is valid for the complete requested area
            paint.drawImage(rect, m_cache.getImage(), rect);
            return { rect, {} };

        } else {
            // cache doesn't begin at the right frame or doesn't
            // contain the complete view, but might be scrollable or
            // partially usable
            m_cache.scrollTo(startFrame);

            // if we are not time-constrained, then we want to paint
            // the whole area in one go, and we're not going to
            // provide the more complex logic to handle that if there
            // are discontiguous areas involved. So if the only valid
            // part of cache is in the middle, just make the whole
            // thing invalid and start again.
            if (!timeConstrained) {
                if (m_cache.getValidLeft() > x0 &&
                    m_cache.getValidRight() < x1) {
                    m_cache.invalidate();
                }
            }
        }
    }

    bool rightToLeft = false;

    if (!m_cache.isValid() && timeConstrained) {
        // When rendering the whole thing in a context where we
        // might not be able to complete the work, start from
        // somewhere near the middle so that the region of
        // interest appears first

        //!!! (perhaps we should avoid doing this if past repaints
        //!!! have been fast enough to do the whole in one shot)
        if (x0 == 0 && x1 == v->getPaintWidth()) {
            x0 = int(x1 * 0.3);
        }
    }

    if (m_cache.isValid()) {
        // When rendering only a part of the cache, we need to make
        // sure that the part we're rendering is adjacent to (or
        // overlapping) a valid area of cache, if we have one. The
        // alternative is to ditch the valid area of cache and render
        // only the requested area, but that's risky because this can
        // happen when just waving the pointer over a small part of
        // the view -- if we lose the partly-built cache every time
        // the user does that, we'll never finish building it.
        int left = x0;
        int width = x1 - x0;
        bool isLeftOfValidArea = false;
        m_cache.adjustToTouchValidArea(left, width, isLeftOfValidArea);
        x0 = left;
        x1 = x0 + width;

        // That call also told us whether we should be painting
        // sub-regions of our target region in right-to-left order in
        // order to ensure contiguity
        rightToLeft = isLeftOfValidArea;
    }
    
    int repaintWidth = x1 - x0;

    QRect rendered = renderToCache(x0, repaintWidth, timeConstrained);

    QRect pr = rect & m_cache.getValidArea();
    paint.drawImage(pr.x(), pr.y(), m_cache.getImage(),
                    pr.x(), pr.y(), pr.width(), pr.height());

    if (!timeConstrained && (pr != rect)) {
        //!!! on a first cut, there is a risk that this will happen
        //!!! when we are at start/end of model -- trap, report, and
        //!!! then fix
        throw std::logic_error("internal error: failed to render entire requested rect even when not time-constrained");
    }
    
    return { pr, {} };

    //!!! todo: timing/incomplete paint

    //!!! todo: peak frequency style

    //!!! todo: transparent style from Colour3DPlot

    //!!! todo: bin boundary alignment when in BinResolution

    //!!! todo: view magnitudes / normalise visible area

    //!!! todo: alter documentation for view mag stuff (cached paints
    //!!! do not update MagnitudeRange)

    //!!! todo, here or in caller: illuminateLocalFeatures

    //!!! fft model scaling?
    
    //!!! should we own the Dense3DModelPeakCache here? or should it persist
}

QRect
Colour3DPlotRenderer::renderToCache(int x0, int repaintWidth, bool timeConstrained)
{
    // Draw to the draw buffer, and then scale-copy from there.

    DenseThreeDimensionalModel *model = m_sources.source;
    if (!model || !model->isOK() || !model->isReady()) {
	throw std::logic_error("no source model provided, or model not ready");
    }

    LayerGeometryProvider *v = m_sources.geometryProvider; // already checked

    // The draw buffer contains a fragment at either our pixel
    // resolution (if there is more than one time-bin per pixel) or
    // time-bin resolution (if a time-bin spans more than one pixel).
    // We need to ensure that it starts and ends at points where a
    // time-bin boundary occurs at an exact pixel boundary, and with a
    // certain amount of overlap across existing pixels so that we can
    // scale and draw from it without smoothing errors at the edges.

    // If (getFrameForX(x) / increment) * increment ==
    // getFrameForX(x), then x is a time-bin boundary.  We want two
    // such boundaries at either side of the draw buffer -- one which
    // we draw up to, and one which we subsequently crop at.

    bool bufferIsBinResolution = false;
    int binResolution = model->getResolution();
    int zoomLevel = v->getZoomLevel();
    if (binResolution > zoomLevel) bufferIsBinResolution = true;

    sv_frame_t leftBoundaryFrame = -1, leftCropFrame = -1;
    sv_frame_t rightBoundaryFrame = -1, rightCropFrame = -1;

    int drawWidth;

    if (bufferIsBinResolution) {
        for (int x = x0; ; --x) {
            sv_frame_t f = v->getFrameForX(x);
            if ((f / binResolution) * binResolution == f) {
                if (leftCropFrame == -1) leftCropFrame = f;
                else if (x < x0 - 2) {
                    leftBoundaryFrame = f;
                    break;
                }
            }
        }
        for (int x = x0 + repaintWidth; ; ++x) {
            sv_frame_t f = v->getFrameForX(x);
            if ((f / binResolution) * binResolution == f) {
                if (rightCropFrame == -1) rightCropFrame = f;
                else if (x > x0 + repaintWidth + 2) {
                    rightBoundaryFrame = f;
                    break;
                }
            }
        }
        drawWidth = int((rightBoundaryFrame - leftBoundaryFrame) / binResolution);
    } else {
        drawWidth = repaintWidth;
    }
    
    // We always paint the full height.  Smaller heights can be used
    // when painting direct from cache (outside this function), but we
    // want to ensure the cache is coherent without having to worry
    // about vertical matching of required and valid areas as well as
    // horizontal. That's why this function didn't take any y/height
    // parameters.
    int h = v->getPaintHeight();

    clearDrawBuffer(drawWidth, h);

    vector<int> binforx(drawWidth);
    vector<double> binfory(h);
    
    bool usePeaksCache = false;
    int binsPerPeak = 1;

    if (bufferIsBinResolution) {
        for (int x = 0; x < drawWidth; ++x) {
            binforx[x] = int(leftBoundaryFrame / binResolution) + x;
        }
    } else {
        for (int x = 0; x < drawWidth; ++x) {
            sv_frame_t f0 = v->getFrameForX(x);
            double s0 = double(f0 - model->getStartFrame()) / binResolution;
            binforx[x] = int(s0 + 0.0001);
        }

        if (m_sources.peaks) { // peaks cache exists

            binsPerPeak = m_sources.peaks->getColumnsPerPeak();
            usePeaksCache = (binResolution * binsPerPeak) < zoomLevel;

            if (m_params.colourScale.getScale() ==
                ColourScale::PhaseColourScale) {
                usePeaksCache = false;
            }
        }
    }

    int attainedDrawWidth = drawWidth;

    //!!! todo: all
    
}
 
void
Colour3DPlotRenderer::clearDrawBuffer(int w, int h)
{
    if (m_drawBuffer.width() < w || m_drawBuffer.height() != h) {

        m_drawBuffer = QImage(w, h, QImage::Format_Indexed8);

        for (int pixel = 0; pixel < 256; ++pixel) {
            //!!! todo: colour rotation (here 0)
            m_drawBuffer.setColor
                ((unsigned char)pixel,
                 m_params.colourScale.getColourForPixel(pixel, 0).rgb());
        }
    }

    m_drawBuffer.fill(0);
}


