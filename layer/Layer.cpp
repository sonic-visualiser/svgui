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
#include "base/CommandHistory.h"

#include <iostream>

#include <QMutexLocker>
#include <QMouseEvent>

#include "LayerFactory.h"
#include "base/PlayParameterRepository.h"

Layer::Layer() :
    m_haveDraggingRect(false)
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

bool
Layer::getXScaleValue(const View *v, int x, float &value, QString &unit) const
{
    if (!hasTimeXAxis()) return false;

    const Model *m = getModel();
    if (!m) return false;

    value = float(v->getFrameForX(x)) / m->getSampleRate();
    unit = "s";
    return true;
}

bool
Layer::MeasureRect::operator<(const MeasureRect &mr) const
{
    if (haveFrames) {
        if (startFrame == mr.startFrame) {
            if (endFrame != mr.endFrame) {
                return endFrame < mr.endFrame;
            }
        } else {
            return startFrame < mr.startFrame;
        }
    } else {
        if (pixrect.x() == mr.pixrect.x()) {
            if (pixrect.width() != mr.pixrect.width()) {
                return pixrect.width() < mr.pixrect.width();
            }
        } else {
            return pixrect.x() < mr.pixrect.x();
        }
    }

    // the two rects are equal in x and width

    if (pixrect.y() == mr.pixrect.y()) {
        return pixrect.height() < mr.pixrect.height();
    } else {
        return pixrect.y() < mr.pixrect.y();
    }
}

QString
Layer::AddMeasurementRectCommand::getName() const
{
    return tr("Make Measurement");
}

void
Layer::AddMeasurementRectCommand::execute()
{
    m_layer->addMeasureRect(m_rect);
}

void
Layer::AddMeasurementRectCommand::unexecute()
{
    m_layer->deleteMeasureRect(m_rect);
}

void
Layer::measureStart(View *v, QMouseEvent *e)
{
    m_draggingRect.pixrect = QRect(e->x(), e->y(), 0, 0);
    if (hasTimeXAxis()) {
        m_draggingRect.haveFrames = true;
        m_draggingRect.startFrame = v->getFrameForX(e->x());
        m_draggingRect.endFrame = m_draggingRect.startFrame;
    } else {
        m_draggingRect.haveFrames = false;
    }
    m_haveDraggingRect = true;
}

void
Layer::measureDrag(View *v, QMouseEvent *e)
{
    if (!m_haveDraggingRect) return;

    m_draggingRect.pixrect = QRect(m_draggingRect.pixrect.x(),
                                   m_draggingRect.pixrect.y(),
                                   e->x() - m_draggingRect.pixrect.x(),
                                   e->y() - m_draggingRect.pixrect.y())
        .normalized();

    if (hasTimeXAxis()) {
        m_draggingRect.endFrame = v->getFrameForX(e->x());
    }
}

void
Layer::measureEnd(View *v, QMouseEvent *e)
{
    if (!m_haveDraggingRect) return;
    measureDrag(v, e);
    
    CommandHistory::getInstance()->addCommand
        (new AddMeasurementRectCommand(this, m_draggingRect));

    m_haveDraggingRect = false;
}

void
Layer::paintMeasurementRects(View *v, QPainter &paint) const
{
    if (m_haveDraggingRect) {
        paintMeasurementRect(v, paint, m_draggingRect);
    }

    for (MeasureRectSet::const_iterator i = m_measureRects.begin(); 
         i != m_measureRects.end(); ++i) {
        paintMeasurementRect(v, paint, *i);
    }
}

void
Layer::paintMeasurementRect(View *v, QPainter &paint, MeasureRect &r)
{
    if (r.haveFrames) {
        
        int x0 = -1;
        int x1 = v->width() + 1;
        
        if (r.startFrame >= v->getStartFrame()) {
            x0 = v->getXForFrame(r.startFrame);
        }
        if (r.endFrame <= v->getEndFrame()) {
            x1 = v->getXForFrame(r.endFrame);
        }
        
        QRect pr = QRect(x0, r.pixrect.y(),
                         x1 - x0, r.pixrect.height());
        
        r.pixrect = pr;
    }
    
    v->drawMeasurementRect(paint, this, r.pixrect);
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
