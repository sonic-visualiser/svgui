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

#ifndef COLOUR_3D_PLOT_RENDERER_H
#define COLOUR_3D_PLOT_RENDERER_H

#include "ColourScale.h"
#include "ScrollableImageCache.h"

#include "base/ColumnOp.h"
#include "base/MagnitudeRange.h"

#include <QRect>
#include <QPainter>
#include <QImage>

class LayerGeometryProvider;
class VerticalBinLayer;
class DenseThreeDimensionalModel;
class Dense3DModelPeakCache;
class FFTModel;

class Colour3DPlotRenderer
{
public:
    enum BinDisplay {
	AllBins,
	PeakBins,
	PeakFrequencies
    };

    enum BinScale {
	LinearBinScale,
	LogBinScale
    };

    struct Sources {
        Sources() : verticalBinLayer(0), source(0), peaks(0), fft(0) { }
        
        // These must all outlive this class
        const VerticalBinLayer *verticalBinLayer;  // always
	const DenseThreeDimensionalModel *source;  // always
	const Dense3DModelPeakCache *peaks;        // optionally
	const FFTModel *fft;                       // optionally
    };        

    struct Parameters {
	Parameters() :
	    colourScale(ColourScale::Parameters()),
	    normalization(ColumnOp::NoNormalization),
	    binDisplay(AllBins),
            binScale(LinearBinScale),
	    alwaysOpaque(false),
            interpolate(false), //!!! separate out x-interpolate and y-interpolate? the spectrogram actually does (or used to)
            invertVertical(false) { }

	ColourScale colourScale;       // complete ColourScale object by value
	ColumnOp::Normalization normalization;
	BinDisplay binDisplay;
	BinScale binScale;
	bool alwaysOpaque;
	bool interpolate;
	bool invertVertical;
    };
    
    Colour3DPlotRenderer(Sources sources, Parameters parameters) :
        m_sources(sources),
	m_params(parameters)
    { }

    struct RenderResult {
        /**
         * The rect that was actually rendered. May be equal to the
         * rect that was requested to render, or may be smaller if
         * time ran out and the complete flag was not set.
         */
        QRect rendered;

        /**
         * The magnitude range of the data in the rendered area.
         */
        MagnitudeRange range;
    };

    /**
     * Render the requested area using the given painter, obtaining
     * geometry (e.g. start frame) from the given
     * LayerGeometryProvider.
     *
     * The whole of the supplied rect will be rendered and the
     * returned QRect will be equal to the supplied QRect. (See
     * renderTimeConstrained for an alternative that may render only
     * part of the rect in cases where obtaining source data is slow
     * and retaining responsiveness is important.)
     *
     * Note that Colour3DPlotRenderer retains internal cache state
     * related to the size and position of the supplied
     * LayerGeometryProvider. Although it is valid to call render()
     * successively on the same Colour3DPlotRenderer with different
     * LayerGeometryProviders, it will be much faster to use a
     * dedicated Colour3DPlotRenderer for each LayerGeometryProvider.
     *
     * If the model to render from is not ready, this will throw a
     * std::logic_error exception. The model must be ready and the
     * layer requesting the render must not be dormant in its view, so
     * that the LayerGeometryProvider returns valid results; it is the
     * caller's responsibility to ensure these.
     */
    RenderResult render(LayerGeometryProvider *v, QPainter &paint, QRect rect);
    
    /**
     * Render the requested area using the given painter, obtaining
     * geometry (e.g. start frame) from the stored
     * LayerGeometryProvider.
     *
     * As much of the rect will be rendered as can be managed given
     * internal time constraints (using a RenderTimer object
     * internally). The returned QRect (the rendered field in the
     * RenderResult struct) will contain the area that was
     * rendered. Note that we always render the full requested height,
     * it's only width that is time-constrained.
     *
     * Note that Colour3DPlotRenderer retains internal cache state
     * related to the size and position of the supplied
     * LayerGeometryProvider. Although it is valid to call render()
     * successively on the same Colour3DPlotRenderer with different
     * LayerGeometryProviders, it will be much faster to use a
     * dedicated Colour3DPlotRenderer for each LayerGeometryProvider.
     *
     * If the model to render from is not ready, this will throw a
     * std::logic_error exception. The model must be ready and the
     * layer requesting the render must not be dormant in its view, so
     * that the LayerGeometryProvider returns valid results; it is the
     * caller's responsibility to ensure these.
     */
    RenderResult renderTimeConstrained(LayerGeometryProvider *v,
                                       QPainter &paint, QRect rect);

    /**
     * Return the area of the largest rectangle within the entire area
     * of the cache that is unavailable in the cache. This is only
     * valid in relation to a preceding render() call which is
     * presumed to have set the area, start frame, and zoom level for
     * the cache. It could be used to establish a suitable region for
     * a subsequent paint request (because if an area is not in the
     * cache, it cannot have been rendered since the cache was
     * cleared).
     *
     * Returns an empty QRect if the cache is entirely valid.
     */
    QRect getLargestUncachedRect();
    
private:
    Sources m_sources;
    Parameters m_params;

    // Draw buffer is the target of each partial repaint. It is always
    // at view height (not model height) and is cleared and repainted
    // on each fragment render. The only reason it's stored as a data
    // member is to avoid reallocation.
    QImage m_drawBuffer;

    // Image cache is our persistent record of the visible area. It is
    // always the same size as the view (i.e. the paint size reported
    // by the LayerGeometryProvider) and is scrolled and partially
    // repainted internally as appropriate. A render request is
    // carried out by repainting to cache (via the draw buffer) any
    // area that is being requested but is not valid in the cache, and
    // then repainting from cache to the requested painter.
    ScrollableImageCache m_cache;

    bool useBinResolutionForDrawBuffer(LayerGeometryProvider *) const;

    RenderResult render(LayerGeometryProvider *v,
                        QPainter &paint, QRect rect, bool timeConstrained);
    void renderToCachePixelResolution(LayerGeometryProvider *v, int x0,
                                      int repaintWidth, bool rightToLeft,
                                      bool timeConstrained);
    void renderToCacheBinResolution(LayerGeometryProvider *v, int x0,
                                    int repaintWidth);

    int renderDrawBuffer(int w, int h,
                         const std::vector<int> &binforx,
                         const std::vector<double> &binfory,
                         bool usePeaksCache,
                         bool rightToLeft,
                         bool timeConstrained);

    int renderDrawBufferPeakFrequencies(LayerGeometryProvider *v,
                                        int w, int h,
                                        const std::vector<int> &binforx,
                                        const std::vector<double> &binfory,
                                        bool rightToLeft,
                                        bool timeConstrained);
    
    void recreateDrawBuffer(int w, int h);
    void clearDrawBuffer(int w, int h);
};

#endif

