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
            binDisplay(BinDisplay::AllBins),
            scaleFactor(1.0),
            threshold(0.0),
            gain(1.0),
            normalization(ColumnNormalization::None) { }

        /** Selection of bins to include in the export. If a
         *  LayerGeometryProvider is also included in Sources, then
         *  the set of bins will also be constrained to the vertical
         *  range of that. */
        BinDisplay binDisplay;

        /** Initial scale factor (e.g. for FFT scaling). This factor
         *  is actually applied to exported values, in contrast to the
         *  gain value below based on the ColourScale parameter. */
        double scaleFactor;

        /** Threshold below which every value is mapped to background
         *  pixel 0 in the display, matching the ColourScale object
         *  parameters. This is used for thresholding in
         *  peak-frequency output only. */
        double threshold;

        /** Gain that is applied before thresholding, in the display,
         *  matching the ColourScale object parameters. This is used
         *  only to determined the thresholding level. The exported
         *  values have the scaleFactor applied, but not this gain. */
        double gain;

        /** Type of column normalization. Again, this is only used to
         *  calculate thresholding level. The exported values are
         *  un-normalized. */
        ColumnNormalization normalization;
    };
    
    Colour3DPlotExporter(Sources sources, Parameters parameters);
    ~Colour3DPlotExporter();

    void discardSources();
    
    QString getDelimitedDataHeaderLine(QString, DataExportOptions) const override;
    
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
