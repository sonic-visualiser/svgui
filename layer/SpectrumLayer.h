
/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef _SPECTRUM_LAYER_H_
#define _SPECTRUM_LAYER_H_

#include "Layer.h"

#include "base/Window.h"

#include "data/model/DenseTimeValueModel.h"

#include <QColor>

class FFTModel;

class SpectrumLayer : public Layer
{
    Q_OBJECT

public:
    SpectrumLayer();
    ~SpectrumLayer();
    
    void setModel(DenseTimeValueModel *model);
    virtual const Model *getModel() const { return m_model; }
    virtual void paint(View *v, QPainter &paint, QRect rect) const;

    virtual PropertyList getProperties() const;
    virtual QString getPropertyLabel(const PropertyName &) const;
    virtual PropertyType getPropertyType(const PropertyName &) const;
    virtual QString getPropertyGroupName(const PropertyName &) const;
    virtual int getPropertyRangeAndValue(const PropertyName &,
					   int *min, int *max) const;
    virtual QString getPropertyValueLabel(const PropertyName &,
					  int value) const;
    virtual void setProperty(const PropertyName &, int value);
    virtual void setProperties(const QXmlAttributes &);

    virtual bool getValueExtents(float &min, float &max,
                                 bool &logarithmic, QString &unit) const;

    virtual bool isLayerScrollable(const View *v) const { return false; }

    enum ChannelMode { SeparateChannels, MixChannels, OverlayChannels };

    void setChannelMode(ChannelMode);
    ChannelMode getChannelCount() const { return m_channelMode; }

    void setChannel(int);
    int getChannel() const { return m_channel; }

    enum EnergyScale { LinearScale, MeterScale, dBScale };

    void setBaseColour(QColor);
    QColor getBaseColour() const { return m_colour; }

    void setEnergyScale(EnergyScale);
    EnergyScale getEnergyScale() const { return m_energyScale; }

    void setWindowSize(size_t);
    size_t getWindowSize() const { return m_windowSize; }
    
    void setWindowHopLevel(size_t level);
    size_t getWindowHopLevel() const { return m_windowHopLevel; }

    void setWindowType(WindowType type);
    WindowType getWindowType() const { return m_windowType; }

    void setGain(float gain);
    float getGain() const;

    void setNormalize(bool n);
    bool getNormalize() const;

    virtual QString toXmlString(QString indent = "",
				QString extraAttributes = "") const;

protected slots:
    void preferenceChanged(PropertyContainer::PropertyName name);

protected:
    DenseTimeValueModel    *m_model;
    std::vector<FFTModel *> m_fft;
    ChannelMode             m_channelMode;
    int                     m_channel;
    bool                    m_channelSet;
    QColor                  m_colour;
    EnergyScale             m_energyScale;
    bool                    m_normalize;
    float                   m_gain;
    size_t                  m_windowSize;
    WindowType              m_windowType;
    size_t                  m_windowHopLevel;

    void setupFFTs();

    size_t getWindowIncrement() const {
        if (m_windowHopLevel == 0) return m_windowSize;
        else if (m_windowHopLevel == 1) return (m_windowSize * 3) / 4;
        else return m_windowSize / (1 << (m_windowHopLevel - 1));
    }
};

#endif
