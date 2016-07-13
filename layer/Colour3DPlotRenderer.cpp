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
#include "VerticalBinLayer.h"

#include <vector>

//#define DEBUG_SPECTROGRAM_REPAINT 1

using namespace std;

Colour3DPlotRenderer::RenderResult
Colour3DPlotRenderer::render(LayerGeometryProvider *v, QPainter &paint, QRect rect)
{
    return render(v, paint, rect, false);
}

Colour3DPlotRenderer::RenderResult
Colour3DPlotRenderer::renderTimeConstrained(LayerGeometryProvider *v,
                                            QPainter &paint, QRect rect)
{
    return render(v, paint, rect, true);
}

QRect
Colour3DPlotRenderer::getLargestUncachedRect()
{
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

Colour3DPlotRenderer::RenderResult
Colour3DPlotRenderer::render(LayerGeometryProvider *v,
                             QPainter &paint, QRect rect, bool timeConstrained)
{
    sv_frame_t startFrame = v->getStartFrame();
    
    int x0 = v->getXForViewX(rect.x());
    int x1 = v->getXForViewX(rect.x() + rect.width());
    if (x0 < 0) x0 = 0;
    if (x1 > v->getPaintWidth()) x1 = v->getPaintWidth();

    m_cache.resize(v->getPaintSize());
    m_cache.setZoomLevel(v->getZoomLevel());

    cerr << "cache start " << m_cache.getStartFrame()
         << " valid left " << m_cache.getValidLeft()
         << " valid right " << m_cache.getValidRight()
         << endl;
    cerr << " view start " << startFrame
         << " x0 " << x0
         << " x1 " << x1
         << endl;

    bool bufferIsBinResolution = useBinResolutionForDrawBuffer(v);
    
    if (bufferIsBinResolution) {
        // Rendering should be fast in this situation because we are
        // quite well zoomed-in, and the sums are easier this
        // way. Calculating boundaries later will be fiddly for
        // partial paints otherwise.
        timeConstrained = false;
    }
    
    if (m_cache.isValid()) { // some part of the cache is valid

        if (v->getXForFrame(m_cache.getStartFrame()) ==
            v->getXForFrame(startFrame) &&
            m_cache.getValidLeft() <= x0 &&
            m_cache.getValidRight() >= x1) {

            cerr << "cache hit" << endl;
            
            // cache is valid for the complete requested area
            paint.drawImage(rect, m_cache.getImage(), rect);
            return { rect, {} };

        } else {
            cerr << "cache partial hit" << endl;
            
            // cache doesn't begin at the right frame or doesn't
            // contain the complete view, but might be scrollable or
            // partially usable
            m_cache.scrollTo(v, startFrame);

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
        // cache completely invalid
        m_cache.setStartFrame(startFrame);
    }

    bool rightToLeft = false;

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
            cerr << "cache somewhat valid" << endl;
            
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
    
    // Note, we always paint the full height.  Smaller heights can be
    // used when painting direct from cache (outside this function),
    // but we want to ensure the cache is coherent without having to
    // worry about vertical matching of required and valid areas as
    // well as horizontal. That's why this function didn't take any
    // y/height parameters.

    if (bufferIsBinResolution && (m_params.binDisplay != PeakFrequencies)) {
        renderToCacheBinResolution(v, x0, x1 - x0);
    } else {
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
    
    return { pr, {} };

    //!!! todo: timing/incomplete paint

    //!!! todo: peak frequency style

    //!!! todo: transparent style from Colour3DPlot

    //!!! todo: view magnitudes / normalise visible area

    //!!! todo: alter documentation for view mag stuff (cached paints
    //!!! do not update MagnitudeRange)

    //!!! todo, here or in caller: illuminateLocalFeatures

    //!!! fft model scaling?
    
    //!!! should we own the Dense3DModelPeakCache here? or should it persist
}

bool
Colour3DPlotRenderer::useBinResolutionForDrawBuffer(LayerGeometryProvider *v) const
{
    const DenseThreeDimensionalModel *model = m_sources.source;
    if (!model) return false;
    int binResolution = model->getResolution();
    int zoomLevel = v->getZoomLevel();
    return (binResolution > zoomLevel);
}

void
Colour3DPlotRenderer::renderToCachePixelResolution(LayerGeometryProvider *v,
                                                   int x0, int repaintWidth,
                                                   bool rightToLeft,
                                                   bool timeConstrained)
{
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
            ColourScale::PhaseColourScale) {
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

    if (m_params.binDisplay == PeakFrequencies) {
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
}

void
Colour3DPlotRenderer::renderToCacheBinResolution(LayerGeometryProvider *v,
                                                 int x0, int repaintWidth)
{
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
    cerr << "modelWidth " << modelWidth << endl;
            
    for (int x = start; x != finish; x += step) {

        // x is the on-canvas pixel coord; sx (later) will be the
        // source column index
        
        ++columnCount;
        
        if (binforx[x] < 0) continue;

        int sx0 = binforx[x] / divisor;
        int sx1 = sx0;
        if (x+1 < w) sx1 = binforx[x+1] / divisor;
        if (sx0 < 0) sx0 = sx1 - 1;
        if (sx0 < 0) continue;
        if (sx1 <= sx0) sx1 = sx0 + 1;

        vector<float> pixelPeakColumn;
        
        for (int sx = sx0; sx < sx1; ++sx) {

#ifdef DEBUG_SPECTROGRAM_REPAINT
            cerr << "sx = " << sx << endl;
#endif

            if (sx < 0 || sx >= modelWidth) {
                continue;
            }

            if (sx != psx) {

                // order:
                // get column -> scale -> record extents ->
                // normalise -> peak pick -> apply display gain ->
                // distribute/interpolate

                ColumnOp::Column fullColumn = sourceModel->getColumn(sx);

//                cerr << "x " << x << ", sx " << sx << ", col height " << fullColumn.size()
//                     << ", minbin " << minbin << ", maxbin " << maxbin << endl;
                
                ColumnOp::Column column =
                    vector<float>(fullColumn.data() + minbin,
                                  fullColumn.data() + maxbin + 1);

//!!! fft scale                if (m_colourScale != PhaseColourScale) {
//                    column = ColumnOp::fftScale(column, m_fftSize);
//                }

//!!! extents                recordColumnExtents(column,
//                                    sx,
//                                    overallMag,
//                                    overallMagChanged);

//                if (m_colourScale != PhaseColourScale) {
                    column = ColumnOp::normalize(column, m_params.normalization);
//                }

                if (m_params.binDisplay == PeakBins) {
                    column = ColumnOp::peakPick(column);
                }

                preparedColumn =
                    ColumnOp::distribute(column, //!!! gain? ColumnOp::applyGain(column, m_gain),
                                         h,
                                         binfory,
                                         minbin,
                                         m_params.interpolate);
                
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
                m_drawBuffer.setPixel
                    (x,
                     h-y-1,
                     m_params.colourScale.getPixel(pixelPeakColumn[y]));
            }
        }

        double fractionComplete = double(columnCount) / double(w);
        if (timer.outOfTime(fractionComplete)) {
            return columnCount;
        }
    }

    return columnCount;
}

int
Colour3DPlotRenderer::renderDrawBufferPeakFrequencies(LayerGeometryProvider *v,
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

    bool logarithmic = (m_params.binScale == LogBinScale);
    
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
        
        for (int sx = sx0; sx < sx1; ++sx) {

            if (sx < 0 || sx >= modelWidth) {
                continue;
            }

            if (sx != psx) {

                ColumnOp::Column fullColumn = fft->getColumn(sx);
                
                ColumnOp::Column column =
                    vector<float>(fullColumn.data() + minbin,
                                  fullColumn.data() + maxbin + 1);

//!!! fft scale                if (m_colourScale != ColourScale::PhaseColourScale) {
//                    column = ColumnOp::fftScale(column, getFFTSize());
//                }

//!!! extents                recordColumnExtents(column,
//                                    sx,
//                                    overallMag,
//                                    overallMagChanged);

//!!!                if (m_colourScale != ColourScale::PhaseColourScale) {
                    column = ColumnOp::normalize(column, m_params.normalization);
//!!!                }

                    preparedColumn = column;
//!!! gain?                preparedColumn = ColumnOp::applyGain(column, m_params.gain);
                
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
        //!!! todo: colour rotation (here 0)
        m_drawBuffer.setColor
            ((unsigned char)pixel,
             m_params.colourScale.getColourForPixel(pixel, 0).rgb());
    }

    m_drawBuffer.fill(0);
}

void
Colour3DPlotRenderer::clearDrawBuffer(int w, int h)
{
    if (m_drawBuffer.width() < w || m_drawBuffer.height() != h) {
        recreateDrawBuffer(w, h);
    } else {
        m_drawBuffer.fill(0);
    }
}


