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

#include "Colour3DPlotExporter.h"

#include "data/model/EditableDenseThreeDimensionalModel.h"
#include "data/model/FFTModel.h"

#include "VerticalBinLayer.h"

QString
Colour3DPlotExporter::toDelimitedDataString(QString delimiter,
                                            DataExportOptions options,
                                            sv_frame_t startFrame,
                                            sv_frame_t duration) const
{
    QMutexLocker locker(&m_mutex);

    BinDisplay binDisplay = m_params.binDisplay;

    auto model =
        ModelById::getAs<DenseThreeDimensionalModel>(m_sources.source);
    auto fftModel =
        ModelById::getAs<FFTModel>(m_sources.fft);

    auto layer = m_sources.verticalBinLayer;
    auto provider = m_sources.provider;

    if (!model || !layer) {
        SVCERR << "ERROR: Colour3DPlotExporter::toDelimitedDataString: Source model and layer required" << endl;
        return {};
    }

    int minbin = 0;
    int sh = model->getHeight();
    int nbins = sh;

    //!!! todo: consider what to do about the actual Colour 3D Plot
    //!!! Layer. In the existing application, this is exported full
    //!!! height. If we switch to using this code, we will be
    //!!! exporting only the displayed height. This is backward
    //!!! incompatible, but also not directly interpretable without
    //!!! any guide in the exported file as to what the bin indices
    //!!! are. Perhaps we should have a flag to export full height,
    //!!! and default to using it.

    //!!! todo: what about the other export types besides
    //!!! delimited-data-string ?
    
    if (provider) {

        minbin = layer->getIBinForY(provider, provider->getPaintHeight());
        if (minbin >= sh) minbin = sh - 1;
        if (minbin < 0) minbin = 0;
    
        nbins = layer->getIBinForY(provider, 0) - minbin + 1;
        if (minbin + nbins > sh) nbins = sh - minbin;
    }

    int w = model->getWidth();

    QString s;
    
    for (int i = 0; i < w; ++i) {
        sv_frame_t fr = model->getStartFrame() + i * model->getResolution();
        if (fr < startFrame || fr >= startFrame + duration) {
            continue;
        }
        QStringList list;

        //...

        s += list.join(delimiter) + "\n";
    }

    return s;
    

    //!!! For reference, this is the body of
    //!!! EditableDenseThreeDimensionalModel::toDelimitedDataString
    /*
    QString s;
    for (int i = 0; in_range_for(m_data, i); ++i) {
        sv_frame_t fr = m_startFrame + i * m_resolution;
        if (fr >= startFrame && fr < startFrame + duration) {
            QStringList list;
            for (int j = 0; in_range_for(m_data.at(i), j); ++j) {
                list << QString("%1").arg(m_data.at(i).at(j));
            }
            s += list.join(delimiter) + "\n";
        }
    }
    return s;
    */    
}

