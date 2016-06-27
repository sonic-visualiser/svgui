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

#include "base/ColumnOp.h"

class DenseThreeDimensionalModel;
class Dense3DModelPeakCache;
class FFTModel;

// We will have one of these per view, for each layer

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
    
    struct Parameters {

	Parameters() :
	    source(0), peaks(0), fft(0),
	    colourScale(ColourScale::Parameters()),
	    normalization(ColumnOp::NoNormalization),
	    binDisplay(AllBins), binScale(LinearBinScale),
	    alwaysOpaque(false), interpolate(false), invertVertical(false) { }
	
	DenseThreeDimensionalModel *source;    // always
	Dense3DModelPeakCache *peaks;	       // optionally
	FFTModel *fft;			       // optionally

	ColourScale colourScale;       // complete ColourScale object by value
	ColumnOp::Normalization normalization;
	BinDisplay binDisplay;
	BinScale binScale;
	bool alwaysOpaque;
	bool interpolate;
	bool invertVertical;
    };

    Colour3DPlotRenderer(Parameters parameters) :
	m_params(parameters)
    { }

private:
    Parameters m_params;

    //!!! we do not have the ScrollableImageCache here; in
    //!!! SpectrogramLayer terms we render onto the draw buffer

    //!!! fft model scaling?
    
    //!!! should we own the Dense3DModelPeakCache here? or should it persist
};

#endif

