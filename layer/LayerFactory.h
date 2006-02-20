/* -*- c-basic-offset: 4 -*-  vi:set ts=8 sts=4 sw=4: */

/*
    A waveform viewer and audio annotation editor.
    Chris Cannam, Queen Mary University of London, 2005-2006
    
    This is experimental software.  Not for distribution.
*/

#ifndef _LAYER_FACTORY_H_
#define _LAYER_FACTORY_H_

#include <QString>
#include <set>

class Layer;
class View;
class Model;

class LayerFactory
{
public:
    enum LayerType {

	// Standard layers
	Waveform,
	Spectrogram,
	TimeRuler,
	TimeInstants,
	TimeValues,
	Notes,
	Text,
	Colour3DPlot,

	// Layers with different initial parameters
	MelodicRangeSpectrogram,

	// Not-a-layer-type
	UnknownLayer = 255
    };

    static LayerFactory *instance();
    
    virtual ~LayerFactory();

    typedef std::set<LayerType> LayerTypeSet;
    LayerTypeSet getValidLayerTypes(Model *model);
    LayerTypeSet getValidEmptyLayerTypes();

    LayerType getLayerType(const Layer *);

    Layer *createLayer(LayerType type, View *view,
		       Model *model = 0, int channel = -1);

    QString getLayerPresentationName(LayerType type);

    void setModel(Layer *layer, Model *model);
    Model *createEmptyModel(LayerType type, Model *baseModel);

    QString getLayerIconName(LayerType);
    QString getLayerTypeName(LayerType);
    LayerType getLayerTypeForName(QString);

protected:
    template <typename LayerClass, typename ModelClass>
    bool trySetModel(Layer *layerBase, Model *modelBase) {
	LayerClass *layer = dynamic_cast<LayerClass *>(layerBase);
	if (!layer) return false;
	ModelClass *model = dynamic_cast<ModelClass *>(modelBase);
	if (!model) return false;
	layer->setModel(model);
	return true;
    }

    static LayerFactory *m_instance;
};

#endif

