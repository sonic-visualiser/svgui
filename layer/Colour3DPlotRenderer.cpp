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

#include "base/Profiler.h"

#include "data/model/DenseThreeDimensionalModel.h"
#include "data/model/Dense3DModelPeakCache.h"
#include "data/model/FFTModel.h"

#include "LayerGeometryProvider.h"
#include "VerticalBinLayer.h"
#include "PaintAssistant.h"

#include "view/ViewManager.h" // for main model sample rate. Pity

#include <vector>

#define DEBUG_COLOUR_PLOT_REPAINT 1

using namespace std;

Colour3DPlotRenderer::RenderResult
Colour3DPlotRenderer::render(const LayerGeometryProvider *v, QPainter &paint, QRect rect)
{
    return render(v, paint, rect, false);
}

Colour3DPlotRenderer::RenderResult
Colour3DPlotRenderer::renderTimeConstrained(const LayerGeometryProvider *v,
                                            QPainter &paint, QRect rect)
{
    return render(v, paint, rect, true);
}

QRect
Colour3DPlotRenderer::getLargestUncachedRect(const LayerGeometryProvider *v)
{
    RenderType renderType = decideRenderType(v);

    if (renderType == DirectTranslucent) {
        return QRect(); // never cached
    }

    int h = m_cache.getSize().height();

    QRect areaLeft(0, 0, m_cache.getValidLeft(), h);
    QRect areaRight(m_cache.getValidRight(), 0,
                    m_cache.getSize().width() - m_cache.getValidRight(), h);

    if (areaRight.width() > areaLeft.width()) {
        return areaRight;
    } else {
        return areaLeft;
    }
}

bool
Colour3DPlotRenderer::geometryChanged(const LayerGeometryProvider *v)
{
    RenderType renderType = decideRenderType(v);

    if (renderType == DirectTranslucent) {
        return true; // never cached
    }

    if (m_cache.getSize() == v->getPaintSize() &&
        m_cache.getZoomLevel() == v->getZoomLevel() &&
        m_cache.getStartFrame() == v->getStartFrame()) {
        return false;
    } else {
        return true;
    }
}

Colour3DPlotRenderer::RenderResult
Colour3DPlotRenderer::render(const LayerGeometryProvider *v,
                             QPainter &paint, QRect rect, bool timeConstrained)
{
    RenderType renderType = decideRenderType(v);

    if (renderType != DrawBufferPixelResolution) {
        // Rendering should be fast in bin-resolution and direct draw
        // cases because we are quite well zoomed-in, and the sums are
        // easier this way. Calculating boundaries later will be
        // fiddly for partial paints otherwise.
        timeConstrained = false;
    }

    int x0 = v->getXForViewX(rect.x());
    int x1 = v->getXForViewX(rect.x() + rect.width());
    if (x0 < 0) x0 = 0;
    if (x1 > v->getPaintWidth()) x1 = v->getPaintWidth();

    sv_frame_t startFrame = v->getStartFrame();
    
    m_cache.resize(v->getPaintSize());
    m_cache.setZoomLevel(v->getZoomLevel());

    m_magCache.resize(v->getPaintSize().width());
    m_magCache.setZoomLevel(v->getZoomLevel());
    
    if (renderType == DirectTranslucent) {
        MagnitudeRange range = renderDirectTranslucent(v, paint, rect);
        return { rect, range };
    }
    
#ifdef DEBUG_COLOUR_PLOT_REPAINT
    cerr << "cache start " << m_cache.getStartFrame()
         << " valid left " << m_cache.getValidLeft()
         << " valid right " << m_cache.getValidRight()
         << endl;
    cerr << " view start " << startFrame
         << " x0 " << x0
         << " x1 " << x1
         << endl;
#endif
    
    if (m_cache.isValid()) { // some part of the cache is valid

        if (v->getXForFrame(m_cache.getStartFrame()) ==
            v->getXForFrame(startFrame) &&
            m_cache.getValidLeft() <= x0 &&
            m_cache.getValidRight() >= x1) {

#ifdef DEBUG_COLOUR_PLOT_REPAINT
            cerr << "cache hit" << endl;
#endif
            
            // cache is valid for the complete requested area
            paint.drawImage(rect, m_cache.getImage(), rect);

            MagnitudeRange range = m_magCache.getRange(x0, x1 - x0);

            return { rect, range };

        } else {
#ifdef DEBUG_COLOUR_PLOT_REPAINT
            cerr << "cache partial hit" << endl;
#endif
            
            // cache doesn't begin at the right frame or doesn't
            // contain the complete view, but might be scrollable or
            // partially usable
            m_cache.scrollTo(v, startFrame);
            m_magCache.scrollTo(v, startFrame);

            // if we are not time-constrained, then we want to paint
            // the whole area in one go; we don't return a partial
            // paint. To avoid providing the more complex logic to
            // handle painting discontiguous areas, if the only valid
            // part of cache is in the middle, just make the whole
            // thing invalid and start again.
            if (!timeConstrained) {
                if (m_cache.getValidLeft() > x0 &&
                    m_cache.getValidRight() < x1) {
                    m_cache.invalidate();
                }
            }
        }
    } else {
        // cache is completely invalid
        m_cache.setStartFrame(startFrame);
        m_magCache.setStartFrame(startFrame);
    }

    bool rightToLeft = false;

    int reqx0 = x0;
    int reqx1 = x1;
    
    if (!m_cache.isValid() && timeConstrained) {
        // When rendering the whole area, in a context where we might
        // not be able to complete the work, start from somewhere near
        // the middle so that the region of interest appears first

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
    
    // Note, we always paint the full height to cache. We want to
    // ensure the cache is coherent without having to worry about
    // vertical matching of required and valid areas as well as
    // horizontal.

    if (renderType == DrawBufferBinResolution) {

        renderToCacheBinResolution(v, x0, x1 - x0);

    } else { // must be DrawBufferPixelResolution, handled DirectTranslucent earlier

        renderToCachePixelResolution(v, x0, x1 - x0, rightToLeft, timeConstrained);
    }

    QRect pr = rect & m_cache.getValidArea();
    paint.drawImage(pr.x(), pr.y(), m_cache.getImage(),
                    pr.x(), pr.y(), pr.width(), pr.height());

    if (!timeConstrained && (pr != rect)) {
        //!!! on a first cut, there is a risk that this will happen
        //!!! when we are at start/end of model -- trap, report, and
        //!!! then fix
        throw std::logic_error("internal error: failed to render entire requested rect even when not time-constrained");
    }

    MagnitudeRange range = m_magCache.getRange(reqx0, reqx1 - reqx0);
    
    return { pr, range };

    //!!! todo, here or in caller: illuminateLocalFeatures

    //!!! todo: handle vertical range other than full range of column
    
    //!!! fft model scaling?
}

Colour3DPlotRenderer::RenderType
Colour3DPlotRenderer::decideRenderType(const LayerGeometryProvider *v) const
{
    const DenseThreeDimensionalModel *model = m_sources.source;
    if (!model || !v || !(v->getViewManager())) {
        return DrawBufferPixelResolution; // or anything
    }

    int binResolution = model->getResolution();
    int zoomLevel = v->getZoomLevel();
    sv_samplerate_t modelRate = model->getSampleRate();

    double rateRatio = v->getViewManager()->getMainModelSampleRate() / modelRate;
    double relativeBinResolution = binResolution * rateRatio;

    if (m_params.binDisplay == BinDisplay::PeakFrequencies) {
        // no alternative works here
        return DrawBufferPixelResolution;
    }

    if (!m_params.alwaysOpaque && !m_params.interpolate) {

        // consider translucent option -- only if not smoothing & not
        // explicitly requested opaque & sufficiently zoomed-in
        
        if (model->getHeight() * 3 < v->getPaintHeight() &&
            relativeBinResolution >= 3 * zoomLevel) {
            return DirectTranslucent;
        }
    }

    if (relativeBinResolution > zoomLevel) {
        return DrawBufferBinResolution;
    } else {
        return DrawBufferPixelResolution;
    }
}

MagnitudeRange
Colour3DPlotRenderer::renderDirectTranslucent(const LayerGeometryProvider *v,
                                              QPainter &paint,
                                              QRect rect)
{
    Profiler profiler("Colour3DPlotRenderer::renderDirectTranslucent");

    MagnitudeRange magRange;
    
    QPoint illuminatePos;
    bool illuminate = v->shouldIlluminateLocalFeatures
        (m_sources.verticalBinLayer, illuminatePos);

    const DenseThreeDimensionalModel *model = m_sources.source;
    
    int x0 = rect.left();
    int x1 = rect.right() + 1;

    int h = v->getPaintHeight();

    sv_frame_t modelStart = model->getStartFrame();
    sv_frame_t modelEnd = model->getEndFrame();
    int modelResolution = model->getResolution();

    double rateRatio =
        v->getViewManager()->getMainModelSampleRate() / model->getSampleRate();

    // the s-prefix values are source, i.e. model, column and bin numbers
    int sx0 = int((double(v->getFrameForX(x0)) / rateRatio - double(modelStart))
                  / modelResolution);
    int sx1 = int((double(v->getFrameForX(x1)) / rateRatio - double(modelStart))
                  / modelResolution);

    int sh = model->getHeight();

    const int buflen = 40;
    char labelbuf[buflen];

    int minbin = 0; //!!!
    int maxbin = sh - 1; //!!!
    
    int psx = -1;

    vector<float> preparedColumn;

    int modelWidth = model->getWidth();

    for (int sx = sx0; sx <= sx1; ++sx) {

        if (sx < 0 || sx >= modelWidth) {
            continue;
        }

        if (sx != psx) {

            //!!! this is in common with renderDrawBuffer - pull it out

            // order:
            // get column -> scale -> record extents ->
            // normalise -> peak pick -> apply display gain
            
            ColumnOp::Column fullColumn = model->getColumn(sx);
                
            ColumnOp::Column column =
                vector<float>(fullColumn.data() + minbin,
                              fullColumn.data() + maxbin + 1);

            magRange.sample(column);

//!!! fft scale                if (m_colourScale != ColourScaleType::Phase) {
//                    column = ColumnOp::fftScale(column, m_fftSize);
//                }

//                if (m_colourScale != ColourScaleType::Phase) {
            preparedColumn = ColumnOp::normalize(column, m_params.normalization);
//                }

            if (m_params.binDisplay == BinDisplay::PeakBins) {
                preparedColumn = ColumnOp::peakPick(preparedColumn);
            }

            // Display gain belongs to the colour scale and is
            // applied by the colour scale object when mapping it

            psx = sx;
        }

	sv_frame_t fx = sx * modelResolution + modelStart;

	if (fx + modelResolution <= modelStart || fx > modelEnd) continue;

        int rx0 = v->getXForFrame(int(double(fx) * rateRatio));
	int rx1 = v->getXForFrame(int(double(fx + modelResolution + 1) * rateRatio));

	int rw = rx1 - rx0;
	if (rw < 1) rw = 1;

	bool showLabel = (rw > 10 &&
			  paint.fontMetrics().width("0.000000") < rw - 3 &&
			  paint.fontMetrics().height() < (h / sh));
        
	for (int sy = minbin; sy <= maxbin; ++sy) {

            int ry0 = m_sources.verticalBinLayer->getIYForBin(v, sy);
            int ry1 = m_sources.verticalBinLayer->getIYForBin(v, sy + 1);

            if (m_params.invertVertical) {
                ry0 = h - ry0 - 1;
                ry1 = h - ry1 - 1;
            }
                    
            QRect r(rx0, ry1, rw, ry0 - ry1);

            float value = preparedColumn[sy - minbin];
            QColor colour = m_params.colourScale.getColour(value,
                                                           m_params.colourRotation);

            if (rw == 1) {
                paint.setPen(colour);
                paint.setBrush(Qt::NoBrush);
                paint.drawLine(r.x(), r.y(), r.x(), r.y() + r.height() - 1);
                continue;
            }

	    QColor pen(255, 255, 255, 80);
	    QColor brush(colour);

            if (rw > 3 && r.height() > 3) {
                brush.setAlpha(160);
            }

	    paint.setPen(Qt::NoPen);
	    paint.setBrush(brush);

	    if (illuminate) {
		if (r.contains(illuminatePos)) {
		    paint.setPen(v->getForeground());
		}
	    }
            
#ifdef DEBUG_COLOUR_3D_PLOT_LAYER_PAINT
//            cerr << "rect " << r.x() << "," << r.y() << " "
//                      << r.width() << "x" << r.height() << endl;
#endif

	    paint.drawRect(r);

	    if (showLabel) {
                double value = model->getValueAt(sx, sy);
                snprintf(labelbuf, buflen, "%06f", value);
                QString text(labelbuf);
                PaintAssistant::drawVisibleText
                    (v,
                     paint,
                     rx0 + 2,
                     ry0 - h / sh - 1 + 2 + paint.fontMetrics().ascent(),
                     text,
                     PaintAssistant::OutlinedText);
	    }
	}
    }

    return magRange;
}

void
Colour3DPlotRenderer::renderToCachePixelResolution(const LayerGeometryProvider *v,
                                                   int x0, int repaintWidth,
                                                   bool rightToLeft,
                                                   bool timeConstrained)
{
    Profiler profiler("Colour3DPlotRenderer::renderToCachePixelResolution");
    cerr << "renderToCachePixelResolution" << endl;
    
    // Draw to the draw buffer, and then copy from there. The draw
    // buffer is at the same resolution as the target in the cache, so
    // no extra scaling needed.

    const DenseThreeDimensionalModel *model = m_sources.source;
    if (!model || !model->isOK() || !model->isReady()) {
	throw std::logic_error("no source model provided, or model not ready");
    }

    int h = v->getPaintHeight();

    clearDrawBuffer(repaintWidth, h);

    vector<int> binforx(repaintWidth);
    vector<double> binfory(h);
    
    bool usePeaksCache = false;
    int binsPerPeak = 1;
    int zoomLevel = v->getZoomLevel();
    int binResolution = model->getResolution();

    for (int x = 0; x < repaintWidth; ++x) {
        sv_frame_t f0 = v->getFrameForX(x0 + x);
        double s0 = double(f0 - model->getStartFrame()) / binResolution;
        binforx[x] = int(s0 + 0.0001);
    }

    if (m_sources.peaks) { // peaks cache exists

        binsPerPeak = m_sources.peaks->getColumnsPerPeak();
        usePeaksCache = (binResolution * binsPerPeak) < zoomLevel;
        
        if (m_params.colourScale.getScale() ==
            ColourScaleType::Phase) {
            usePeaksCache = false;
        }
    }

    cerr << "[PIX] zoomLevel = " << zoomLevel
         << ", binResolution " << binResolution 
         << ", binsPerPeak " << binsPerPeak
         << ", peak cache " << m_sources.peaks
         << ", usePeaksCache = " << usePeaksCache
         << endl;
    
    for (int y = 0; y < h; ++y) {
        binfory[y] = m_sources.verticalBinLayer->getBinForY(v, h - y - 1);
    }

    int attainedWidth;

    if (m_params.binDisplay == BinDisplay::PeakFrequencies) {
        attainedWidth = renderDrawBufferPeakFrequencies(v,
                                                        repaintWidth,
                                                        h,
                                                        binforx,
                                                        binfory,
                                                        rightToLeft,
                                                        timeConstrained);

    } else {
        attainedWidth = renderDrawBuffer(repaintWidth,
                                         h,
                                         binforx,
                                         binfory,
                                         usePeaksCache,
                                         rightToLeft,
                                         timeConstrained);
    }

    if (attainedWidth == 0) return;

    // draw buffer is pixel resolution, no scaling factors or padding involved
    
    int paintedLeft = x0;
    if (rightToLeft) {
        paintedLeft += (repaintWidth - attainedWidth);
    }

    m_cache.drawImage(paintedLeft, attainedWidth,
                      m_drawBuffer,
                      paintedLeft - x0, attainedWidth);

    for (int i = 0; in_range_for(m_magRanges, i); ++i) {
        m_magCache.sampleColumn(i, m_magRanges.at(i));
    }
}

void
Colour3DPlotRenderer::renderToCacheBinResolution(const LayerGeometryProvider *v,
                                                 int x0, int repaintWidth)
{
    Profiler profiler("Colour3DPlotRenderer::renderToCacheBinResolution");
    cerr << "renderToCacheBinResolution" << endl;
    
    // Draw to the draw buffer, and then scale-copy from there. Draw
    // buffer is at bin resolution, i.e. buffer x == source column
    // number. We use toolkit smooth scaling for interpolation.

    const DenseThreeDimensionalModel *model = m_sources.source;
    if (!model || !model->isOK() || !model->isReady()) {
	throw std::logic_error("no source model provided, or model not ready");
    }

    // The draw buffer will contain a fragment at bin resolution. We
    // need to ensure that it starts and ends at points where a
    // time-bin boundary occurs at an exact pixel boundary, and with a
    // certain amount of overlap across existing pixels so that we can
    // scale and draw from it without smoothing errors at the edges.

    // If (getFrameForX(x) / increment) * increment ==
    // getFrameForX(x), then x is a time-bin boundary.  We want two
    // such boundaries at either side of the draw buffer -- one which
    // we draw up to, and one which we subsequently crop at.

    sv_frame_t leftBoundaryFrame = -1, leftCropFrame = -1;
    sv_frame_t rightBoundaryFrame = -1, rightCropFrame = -1;

    int drawBufferWidth;
    int binResolution = model->getResolution();

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
    drawBufferWidth = int
        ((rightBoundaryFrame - leftBoundaryFrame) / binResolution);
    
    int h = v->getPaintHeight();

    // For our purposes here, the draw buffer needs to be exactly our
    // target size (so we recreate always rather than just clear it)
    
    recreateDrawBuffer(drawBufferWidth, h);

    vector<int> binforx(drawBufferWidth);
    vector<double> binfory(h);
    
    for (int x = 0; x < drawBufferWidth; ++x) {
        binforx[x] = int(leftBoundaryFrame / binResolution) + x;
    }

    cerr << "[BIN] binResolution " << binResolution 
         << endl;
    
    for (int y = 0; y < h; ++y) {
        binfory[y] = m_sources.verticalBinLayer->getBinForY(v, h - y - 1);
    }

    int attainedWidth = renderDrawBuffer(drawBufferWidth,
                                         h,
                                         binforx,
                                         binfory,
                                         false,
                                         false,
                                         false);

    if (attainedWidth == 0) return;

    int scaledLeft = v->getXForFrame(leftBoundaryFrame);
    int scaledRight = v->getXForFrame(rightBoundaryFrame);

    cerr << "scaling draw buffer from width " << m_drawBuffer.width()
         << " to " << (scaledRight - scaledLeft) << " (nb drawBufferWidth = "
         << drawBufferWidth << ")" << endl;
    
    QImage scaled = m_drawBuffer.scaled
        (scaledRight - scaledLeft, h,
         Qt::IgnoreAspectRatio, (m_params.interpolate ?
                                 Qt::SmoothTransformation :
                                 Qt::FastTransformation));
            
    int scaledLeftCrop = v->getXForFrame(leftCropFrame);
    int scaledRightCrop = v->getXForFrame(rightCropFrame);
    
    int targetLeft = scaledLeftCrop;
    if (targetLeft < 0) {
        targetLeft = 0;
    }
    
    int targetWidth = scaledRightCrop - targetLeft;
    if (targetLeft + targetWidth > m_cache.getSize().width()) {
        targetWidth = m_cache.getSize().width() - targetLeft;
    }
    
    int sourceLeft = targetLeft - scaledLeft;
    if (sourceLeft < 0) {
        sourceLeft = 0;
    }
    
    int sourceWidth = targetWidth;
    
    cerr << "repaintWidth = " << repaintWidth
         << ", targetWidth = " << targetWidth << endl;
    
    if (targetWidth > 0) {
        m_cache.drawImage(targetLeft, targetWidth,
                          scaled,
                          sourceLeft, sourceWidth);
    }
    
    for (int i = 0; i < targetWidth; ++i) {
        int sourceIx = int((double(i) / targetWidth) * sourceWidth);
        if (in_range_for(m_magRanges, sourceIx)) {
            m_magCache.sampleColumn(i, m_magRanges.at(sourceIx));
        }
    }
}

int
Colour3DPlotRenderer::renderDrawBuffer(int w, int h,
                                       const vector<int> &binforx,
                                       const vector<double> &binfory,
                                       bool usePeaksCache,
                                       bool rightToLeft,
                                       bool timeConstrained)
{
    // Callers must have checked that the appropriate subset of
    // Sources data members are set for the supplied flags (e.g. that
    // peaks model exists if usePeaksCache)
    
    RenderTimer timer(timeConstrained ?
                      RenderTimer::FastRender :
                      RenderTimer::NoTimeout);
    
    int minbin = int(binfory[0] + 0.0001);
    int maxbin = int(binfory[h-1]);
    if (minbin < 0) minbin = 0;
    if (maxbin < 0) maxbin = minbin+1;

    int divisor = 1;
    const DenseThreeDimensionalModel *sourceModel = m_sources.source;
    if (usePeaksCache) {
        divisor = m_sources.peaks->getColumnsPerPeak();
        sourceModel = m_sources.peaks;
    }

    int psx = -1;

    int start = 0;
    int finish = w;
    int step = 1;

    if (rightToLeft) {
        start = w-1;
        finish = -1;
        step = -1;
    }

    int columnCount = 0;
    
    vector<float> preparedColumn;

    int modelWidth = sourceModel->getWidth();

    cerr << "modelWidth " << modelWidth << ", divisor " << divisor << endl;

    for (int x = start; x != finish; x += step) {

        // x is the on-canvas pixel coord; sx (later) will be the
        // source column index
        
        ++columnCount;

#ifdef DEBUG_COLOUR_PLOT_REPAINT
        cerr << "x = " << x << ", binforx[x] = " << binforx[x] << endl;
#endif
        
        if (binforx[x] < 0) continue;

        int sx0 = binforx[x] / divisor;
        int sx1 = sx0;
        if (x+1 < w) sx1 = binforx[x+1] / divisor;
        if (sx0 < 0) sx0 = sx1 - 1;
        if (sx0 < 0) continue;
        if (sx1 <= sx0) sx1 = sx0 + 1;

        vector<float> pixelPeakColumn;
        MagnitudeRange magRange;
        
        for (int sx = sx0; sx < sx1; ++sx) {

#ifdef DEBUG_COLOUR_PLOT_REPAINT
            cerr << "sx = " << sx << endl;
#endif

            if (sx < 0 || sx >= modelWidth) {
                continue;
            }

            if (sx != psx) {

                // order:
                // get column -> scale -> record extents ->
                // normalise -> peak pick -> distribute/interpolate ->
                // apply display gain

                ColumnOp::Column fullColumn = sourceModel->getColumn(sx);

//                cerr << "x " << x << ", sx " << sx << ", col height " << fullColumn.size()
//                     << ", minbin " << minbin << ", maxbin " << maxbin << endl;
                
                ColumnOp::Column column =
                    vector<float>(fullColumn.data() + minbin,
                                  fullColumn.data() + maxbin + 1);

//!!! fft scale                if (m_colourScale != ColourScaleType::Phase) {
//                    column = ColumnOp::fftScale(column, m_fftSize);
//                }

                magRange.sample(column);

//                if (m_colourScale != ColourScaleType::Phase) {
                    column = ColumnOp::normalize(column, m_params.normalization);
//                }

                if (m_params.binDisplay == BinDisplay::PeakBins) {
                    column = ColumnOp::peakPick(column);
                }

                preparedColumn =
                    ColumnOp::distribute(column,
                                         h,
                                         binfory,
                                         minbin,
                                         m_params.interpolate);

                // Display gain belongs to the colour scale and is
                // applied by the colour scale object when mapping it
                
                psx = sx;
            }

            if (sx == sx0) {
                pixelPeakColumn = preparedColumn;
            } else {
                for (int i = 0; in_range_for(pixelPeakColumn, i); ++i) {
                    pixelPeakColumn[i] = std::max(pixelPeakColumn[i],
                                                  preparedColumn[i]);
                }
            }
        }

        if (!pixelPeakColumn.empty()) {

            for (int y = 0; y < h; ++y) {
                int py;
                if (m_params.invertVertical) {
                    py = y;
                } else {
                    py = h - y - 1;
                }
                m_drawBuffer.setPixel
                    (x,
                     py,
                     m_params.colourScale.getPixel(pixelPeakColumn[y]));
            }
            
            m_magRanges.push_back(magRange);
        }

        double fractionComplete = double(columnCount) / double(w);
        if (timer.outOfTime(fractionComplete)) {
            cerr << "out of time" << endl;
            return columnCount;
        }
    }

    return columnCount;
}

int
Colour3DPlotRenderer::renderDrawBufferPeakFrequencies(const LayerGeometryProvider *v,
                                                      int w, int h,
                                                      const vector<int> &binforx,
                                                      const vector<double> &binfory,
                                                      bool rightToLeft,
                                                      bool timeConstrained)
{
    // Callers must have checked that the appropriate subset of
    // Sources data members are set for the supplied flags (e.g. that
    // fft model exists)
    
    RenderTimer timer(timeConstrained ?
                      RenderTimer::FastRender :
                      RenderTimer::NoTimeout);

    int minbin = int(binfory[0] + 0.0001);
    int maxbin = int(binfory[h-1]);
    if (minbin < 0) minbin = 0;
    if (maxbin < 0) maxbin = minbin+1;

    const FFTModel *fft = m_sources.fft;

    FFTModel::PeakSet peakfreqs;

    int psx = -1;
    
    int start = 0;
    int finish = w;
    int step = 1;

    if (rightToLeft) {
        start = w-1;
        finish = -1;
        step = -1;
    }
    
    int columnCount = 0;
    
    vector<float> preparedColumn;

    int modelWidth = fft->getWidth();
    cerr << "modelWidth " << modelWidth << endl;

    double minFreq = (double(minbin) * fft->getSampleRate()) / fft->getFFTSize();
    double maxFreq = (double(maxbin) * fft->getSampleRate()) / fft->getFFTSize();

    bool logarithmic = (m_params.binScale == BinScale::Log);
    
    for (int x = start; x != finish; x += step) {
        
        // x is the on-canvas pixel coord; sx (later) will be the
        // source column index
        
        ++columnCount;
        
        if (binforx[x] < 0) continue;

        int sx0 = binforx[x];
        int sx1 = sx0;
        if (x+1 < w) sx1 = binforx[x+1];
        if (sx0 < 0) sx0 = sx1 - 1;
        if (sx0 < 0) continue;
        if (sx1 <= sx0) sx1 = sx0 + 1;

        vector<float> pixelPeakColumn;
        MagnitudeRange magRange;
        
        for (int sx = sx0; sx < sx1; ++sx) {

            if (sx < 0 || sx >= modelWidth) {
                continue;
            }

            if (sx != psx) {

                ColumnOp::Column fullColumn = fft->getColumn(sx);
                
                ColumnOp::Column column =
                    vector<float>(fullColumn.data() + minbin,
                                  fullColumn.data() + maxbin + 1);

                magRange.sample(column);
                
//!!! fft scale                if (m_colourScale != ColourScaleType::Phase) {
//                    column = ColumnOp::fftScale(column, getFFTSize());
//                }

//!!!                if (m_colourScale != ColourScaleType::Phase) {
                preparedColumn = ColumnOp::normalize
                    (column, m_params.normalization);
//!!!                }

                psx = sx;
            }

            if (sx == sx0) {
                pixelPeakColumn = preparedColumn;
                peakfreqs = fft->getPeakFrequencies(FFTModel::AllPeaks, sx,
                                                    minbin, maxbin - 1);
            } else {
                for (int i = 0; in_range_for(pixelPeakColumn, i); ++i) {
                    pixelPeakColumn[i] = std::max(pixelPeakColumn[i],
                                                  preparedColumn[i]);
                }
            }
        }

        if (!pixelPeakColumn.empty()) {
            
            for (FFTModel::PeakSet::const_iterator pi = peakfreqs.begin();
                 pi != peakfreqs.end(); ++pi) {

                int bin = pi->first;
                double freq = pi->second;

                if (bin < minbin) continue;
                if (bin > maxbin) break;
            
                double value = pixelPeakColumn[bin - minbin];
            
                double y = v->getYForFrequency
                    (freq, minFreq, maxFreq, logarithmic);
            
                int iy = int(y + 0.5);
                if (iy < 0 || iy >= h) continue;

                m_drawBuffer.setPixel
                    (x,
                     iy,
                     m_params.colourScale.getPixel(value));
            }

            m_magRanges.push_back(magRange);
        }

        double fractionComplete = double(columnCount) / double(w);
        if (timer.outOfTime(fractionComplete)) {
            return columnCount;
        }
    }

    return columnCount;
}

void
Colour3DPlotRenderer::recreateDrawBuffer(int w, int h)
{
    m_drawBuffer = QImage(w, h, QImage::Format_Indexed8);

    for (int pixel = 0; pixel < 256; ++pixel) {
        m_drawBuffer.setColor
            ((unsigned char)pixel,
             m_params.colourScale.getColourForPixel
             (pixel, m_params.colourRotation).rgb());
    }

    m_drawBuffer.fill(0);
    m_magRanges.clear();
}

void
Colour3DPlotRenderer::clearDrawBuffer(int w, int h)
{
    if (m_drawBuffer.width() < w || m_drawBuffer.height() != h) {
        recreateDrawBuffer(w, h);
    } else {
        m_drawBuffer.fill(0);
        m_magRanges.clear();
    }
}


