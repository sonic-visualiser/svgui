/* -*- c-basic-offset: 4 -*-  vi:set ts=8 sts=4 sw=4: */

/*
    A waveform viewer and audio annotation editor.
    Chris Cannam, Queen Mary University of London, 2005
    
    This is experimental software.  Not for distribution.
*/

#include "LayerFactory.h"

#include "WaveformLayer.h"
#include "SpectrogramLayer.h"
#include "TimeRulerLayer.h"
#include "TimeInstantLayer.h"
#include "TimeValueLayer.h"
#include "Colour3DPlotLayer.h"

#include "model/RangeSummarisableTimeValueModel.h"
#include "model/DenseTimeValueModel.h"
#include "model/SparseOneDimensionalModel.h"
#include "model/SparseTimeValueModel.h"
#include "model/DenseThreeDimensionalModel.h"

LayerFactory *
LayerFactory::m_instance = new LayerFactory;

LayerFactory *
LayerFactory::instance()
{
    return m_instance;
}

LayerFactory::~LayerFactory()
{
}

QString
LayerFactory::getLayerPresentationName(LayerType type)
{
    switch (type) {
    case Waveform:     return Layer::tr("Waveform");
    case Spectrogram:  return Layer::tr("Spectrogram");
    case TimeRuler:    return Layer::tr("Ruler");
    case TimeInstants: return Layer::tr("Time Instants");
    case TimeValues:   return Layer::tr("Time Values");
    case Colour3DPlot: return Layer::tr("Colour 3D Plot");

    case MelodicRangeSpectrogram:
	// The user can change all the parameters of this after the
	// fact -- there's nothing permanently melodic-range about it
	// that should be encoded in its name
	return Layer::tr("Spectrogram");
    }

    return Layer::tr("Layer");
}

LayerFactory::LayerTypeSet
LayerFactory::getValidLayerTypes(Model *model)
{
    LayerTypeSet types;

    if (dynamic_cast<DenseThreeDimensionalModel *>(model)) {
	types.insert(Colour3DPlot);
    }

    if (dynamic_cast<DenseTimeValueModel *>(model)) {
	types.insert(Spectrogram);
	types.insert(MelodicRangeSpectrogram);
    }

    if (dynamic_cast<RangeSummarisableTimeValueModel *>(model)) {
	types.insert(Waveform);
    }

    if (dynamic_cast<SparseOneDimensionalModel *>(model)) {
	types.insert(TimeInstants);
    }

    if (dynamic_cast<SparseTimeValueModel *>(model)) {
	types.insert(TimeValues);
    }

    // We don't count TimeRuler here as it doesn't actually display
    // the data, although it can be backed by any model

    return types;
}

LayerFactory::LayerType
LayerFactory::getLayerType(Layer *layer)
{
    if (dynamic_cast<WaveformLayer *>(layer)) return Waveform;
    if (dynamic_cast<SpectrogramLayer *>(layer)) return Spectrogram;
    if (dynamic_cast<TimeRulerLayer *>(layer)) return TimeRuler;
    if (dynamic_cast<TimeInstantLayer *>(layer)) return TimeInstants;
    if (dynamic_cast<TimeValueLayer *>(layer)) return TimeValues;
    if (dynamic_cast<Colour3DPlotLayer *>(layer)) return Colour3DPlot;
    return UnknownLayer;
}

void
LayerFactory::setModel(Layer *layer, Model *model)
{
    if (trySetModel<WaveformLayer, RangeSummarisableTimeValueModel>(layer, model))
	return;

    if (trySetModel<SpectrogramLayer, DenseTimeValueModel>(layer, model))
	return;

    if (trySetModel<TimeRulerLayer, Model>(layer, model))
	return;

    if (trySetModel<TimeInstantLayer, SparseOneDimensionalModel>(layer, model))
	return;

    if (trySetModel<TimeValueLayer, SparseTimeValueModel>(layer, model))
	return;

    if (trySetModel<Colour3DPlotLayer, DenseThreeDimensionalModel>(layer, model))
	return;

    if (trySetModel<SpectrogramLayer, DenseTimeValueModel>(layer, model))
	return;
}

Layer *
LayerFactory::createLayer(LayerType type, View *view,
			  Model *model, int channel)
{
    Layer *layer = 0;

    switch (type) {

    case Waveform:
	layer = new WaveformLayer(view);
	static_cast<WaveformLayer *>(layer)->setChannel(channel);
	break;

    case Spectrogram:
	layer = new SpectrogramLayer(view);
	static_cast<SpectrogramLayer *>(layer)->setChannel(channel);
	break;

    case TimeRuler:
	layer = new TimeRulerLayer(view);
	break;

    case TimeInstants:
	layer = new TimeInstantLayer(view);
	break;

    case TimeValues:
	layer = new TimeValueLayer(view);
	break;

    case Colour3DPlot:
	layer = new Colour3DPlotLayer(view);
	break;

    case MelodicRangeSpectrogram: 
	layer = new SpectrogramLayer(view, SpectrogramLayer::MelodicRange);
	static_cast<SpectrogramLayer *>(layer)->setChannel(channel);
	break;
    }

    if (!layer) {
	std::cerr << "LayerFactory::createLayer: Unknown layer type " 
		  << type << std::endl;
    } else {
	setModel(layer, model);
	std::cerr << "LayerFactory::createLayer: Setting object name "
		  << getLayerPresentationName(type).toStdString() << " on " << layer << std::endl;
	layer->setObjectName(getLayerPresentationName(type));
    }

    return layer;
}

