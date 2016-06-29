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

#include "view/LayerGeometryProvider.h"

Colour3DPlotRenderer::RenderResult
Colour3DPlotRenderer::render(QPainter &paint,
			     QRect rect,
			     bool complete)
{
    LayerGeometryProvider *v = m_sources.geometryProvider;
    if (!v) {
	throw std::logic_error("no LayerGeometryProvider provided");
    }

    DenseThreeDimensionalModel *model = m_sources.source;
    if (!model || !model->isOK() || !model->isReady()) {
	throw std::logic_error("no source model provided, or model not ready");
    }
	
    sv_frame_t startFrame = v->getStartFrame();

    
    //!!! todo: timing/incomplete paint

    //!!! todo: peak frequency style

    //!!! todo: transparent style from Colour3DPlot

    //!!! todo: bin boundary alignment when in BinResolution

    return { rect, {} };
}

