/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "Layer.h"
#include "view/View.h"
#include "data/model/Model.h"

#include <iostream>

#include <QMutexLocker>

#include "LayerFactory.h"
#include "base/PlayParameterRepository.h"

Layer::Layer()
{
}

Layer::~Layer()
{
//    std::cerr << "Layer::~Layer(" << this << ")" << std::endl;
}

QString
Layer::getPropertyContainerIconName() const
{
    return LayerFactory::getInstance()->getLayerIconName
	(LayerFactory::getInstance()->getLayerType(this));
}

QString
Layer::getLayerPresentationName() const
{
//    QString layerName = objectName();

    LayerFactory *factory = LayerFactory::getInstance();
    QString layerName = factory->getLayerPresentationName
        (factory->getLayerType(this));

    QString modelName;
    if (getModel()) modelName = getModel()->objectName();
	    
    QString text;
    if (modelName != "") {
	text = QString("%1: %2").arg(modelName).arg(layerName);
    } else {
	text = layerName;
    }
	
    return text;
}

void
Layer::setObjectName(const QString &name)
{
    QObject::setObjectName(name);
    emit layerNameChanged();
}

QString
Layer::toXmlString(QString indent, QString extraAttributes) const
{
    QString s;
    
    s += indent;

    s += QString("<layer id=\"%2\" type=\"%1\" name=\"%3\" model=\"%4\" %5/>\n")
	.arg(encodeEntities(LayerFactory::getInstance()->getLayerTypeName
                            (LayerFactory::getInstance()->getLayerType(this))))
	.arg(getObjectExportId(this))
	.arg(encodeEntities(objectName()))
	.arg(getObjectExportId(getModel()))
	.arg(extraAttributes);

    return s;
}

PlayParameters *
Layer::getPlayParameters() 
{
//    std::cerr << "Layer (" << this << ", " << objectName().toStdString() << ")::getPlayParameters: model is "<< getModel() << std::endl;
    const Model *model = getModel();
    if (model) {
	return PlayParameterRepository::getInstance()->getPlayParameters(model);
    }
    return 0;
}

void
Layer::setLayerDormant(const View *v, bool dormant)
{
    const void *vv = (const void *)v;
    QMutexLocker locker(&m_dormancyMutex);
    m_dormancy[vv] = dormant;
}

bool
Layer::isLayerDormant(const View *v) const
{
    const void *vv = (const void *)v;
    QMutexLocker locker(&m_dormancyMutex);
    if (m_dormancy.find(vv) == m_dormancy.end()) return false;
    return m_dormancy.find(vv)->second;
}

void
Layer::showLayer(View *view, bool show)
{
    setLayerDormant(view, !show);
    emit layerParametersChanged();
}


#ifdef INCLUDE_MOCFILES
#include "Layer.moc.cpp"
#endif

