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

#ifndef COLOUR_3D_PLOT_EXPORTER_H
#define COLOUR_3D_PLOT_EXPORTER_H

#include "Colour3DPlotRenderer.h"

class Colour3DPlotExporter : public Model
{
    Q_OBJECT
    
public:
    struct Sources {
        // These must all outlive this class, or else discardSources()
        // must be called
        const VerticalBinLayer *verticalBinLayer; // always
        ModelId source; // always; a DenseThreeDimensionalModel
        ModelId fft; // optionally; an FFTModel; used for phase/peak-freq modes
        const LayerGeometryProvider *provider; // optionally
    };

    struct Parameters {
        Parameters() :
            binDisplay(BinDisplay::AllBins) { }

        /** Selection of bins to include in the export. */
        BinDisplay binDisplay;
    };
    
    Colour3DPlotExporter(Sources sources, Parameters parameters);
    ~Colour3DPlotExporter();

    void discardSources();

    QString toDelimitedDataString(QString, DataExportOptions,
                                  sv_frame_t, sv_frame_t) const override;

    
    // Further Model methods that we just delegate

    bool isOK() const override {
        if (auto model = ModelById::get(m_sources.source)) {
            return model->isOK();
        }
        return false;
    }
        
    sv_frame_t getStartFrame() const override { 
        if (auto model = ModelById::get(m_sources.source)) {
            return model->getStartFrame();
        }
        return 0;
    }
    
    sv_frame_t getTrueEndFrame() const override { 
        if (auto model = ModelById::get(m_sources.source)) {
            return model->getTrueEndFrame();
        }
        return 0;
    }
    
    sv_samplerate_t getSampleRate() const override { 
        if (auto model = ModelById::get(m_sources.source)) {
            return model->getSampleRate();
        }
        return 0;
    }

    QString getTypeName() const override {
        if (auto model = ModelById::get(m_sources.source)) {
            return model->getTypeName();
        }
        return "(exporter)"; // internal fallback, no translation needed
    }

    int getCompletion() const override {
        if (auto model = ModelById::get(m_sources.source)) {
            return model->getCompletion();
        }
        return 0;
    }
    
private:
    Sources m_sources;
    Parameters m_params;
};

#endif
