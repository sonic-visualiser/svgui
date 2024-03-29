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
#include "widgets/CommandHistory.h"

#include <iostream>

#include <QMutexLocker>
#include <QMouseEvent>
#include <QTextStream>

#include <QDomDocument>
#include <QDomElement>
#include <QDomNamedNodeMap>
#include <QDomAttr>

#include "LayerFactory.h"
#include "base/PlayParameterRepository.h"

#include <cmath>

namespace sv {

Layer::Layer() :
    m_haveDraggingRect(false),
    m_haveCurrentMeasureRect(false)
{
}

Layer::~Layer()
{
//    SVDEBUG << "Layer::~Layer(" << this << ")" << endl;
}

void
Layer::connectSignals(ModelId modelId)
{
    auto model = ModelById::get(modelId);
    if (!model) return;
    
    connect(model.get(), SIGNAL(modelChanged(ModelId)),
            this, SIGNAL(modelChanged(ModelId)));

    connect(model.get(), SIGNAL(modelChangedWithin(ModelId, sv_frame_t, sv_frame_t)),
            this, SIGNAL(modelChangedWithin(ModelId, sv_frame_t, sv_frame_t)));

    connect(model.get(), SIGNAL(completionChanged(ModelId)),
            this, SIGNAL(modelCompletionChanged(ModelId)));

    connect(model.get(), SIGNAL(alignmentCompletionChanged(ModelId)),
            this, SIGNAL(modelAlignmentCompletionChanged(ModelId)));
}

ModelId
Layer::getSourceModel() const
{
    ModelId sourceId;
    auto model = ModelById::get(getModel());
    while (model && !model->getSourceModel().isNone()) {
        sourceId = model->getSourceModel();
        model = ModelById::get(sourceId);
    }
    return sourceId;
}

QString
Layer::getPropertyContainerIconName() const
{
    return LayerFactory::getInstance()->getLayerIconName
        (LayerFactory::getInstance()->getLayerType(this));
}

void
Layer::setPresentationName(QString name)
{
    m_presentationName = name;
}

bool
Layer::isPresentationNameSet() const
{
    return (m_presentationName != "");
}

QString
Layer::getLayerPresentationName() const
{
    if (m_presentationName != "") return m_presentationName;

    LayerFactory *factory = LayerFactory::getInstance();
    QString layerName = factory->getLayerPresentationName
        (factory->getLayerType(this));

    QString modelName;
    auto model = ModelById::get(getModel());
    if (model) modelName = model->objectName();
            
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

std::shared_ptr<PlayParameters>
Layer::getPlayParameters() 
{
    return PlayParameterRepository::getInstance()->getPlayParameters
        (getModel().untyped);
}

void
Layer::setLayerDormant(const LayerGeometryProvider *v, bool dormant)
{
    const void *vv = (const void *)v;
    QMutexLocker locker(&m_dormancyMutex);
    m_dormancy[vv] = dormant;
}

bool
Layer::isLayerDormant(const LayerGeometryProvider *v) const
{
    const void *vv = (const void *)v;
    QMutexLocker locker(&m_dormancyMutex);
    if (m_dormancy.find(vv) == m_dormancy.end()) return false;
    return m_dormancy.find(vv)->second;
}

void
Layer::showLayer(LayerGeometryProvider *view, bool show)
{
    setLayerDormant(view, !show);
    emit layerParametersChanged();
}

bool
Layer::getXScaleValue(const LayerGeometryProvider *v, int x, double &value, QString &unit) const
{
    if (!hasTimeXAxis()) return false;

    auto model = ModelById::get(getModel());
    if (!model) return false;

    value = double(v->getFrameForX(x)) / model->getSampleRate();
    unit = "s";
    return true;
}

bool
Layer::getYScaleDifference(const LayerGeometryProvider *v, int y0, int y1,
                           double &diff, QString &unit) const
{
    double v0, v1;
    if (!getYScaleValue(v, y0, v0, unit) ||
        !getYScaleValue(v, y1, v1, unit)) {
        diff = 0.f;
        return false;
    }
    diff = fabs(v1 - v0);
    return true;
}

sv_frame_t
Layer::alignToReference(LayerGeometryProvider *v, sv_frame_t frame) const
{
    auto model = ModelById::get(getModel());
    if (model && !model->getAlignmentReference().isNone()) {
        return model->alignToReference(frame);
    } else {
        return v->getView()->alignToReference(frame);
    }
}

sv_frame_t
Layer::alignFromReference(LayerGeometryProvider *v, sv_frame_t frame) const
{
    auto model = ModelById::get(getModel());
    if (model && !model->getAlignmentReference().isNone()) {
        return model->alignFromReference(frame);
    } else {
        return v->getView()->alignFromReference(frame);
    }
}

bool
Layer::clipboardHasDifferentAlignment(LayerGeometryProvider *v, const Clipboard &clip) const
{
    // Notes on pasting to an aligned layer:
    // 
    // Each point may have a reference frame that may differ from the
    // point's given frame (in its source model).  If it has no
    // reference frame, we have to assume the source model was not
    // aligned or was the reference model: when cutting or copying
    // points from a layer, we must always set their reference frame
    // correctly if we are aligned.
    // 
    // When pasting:
    // - if point's reference and aligned frames differ:
    //   - if this layer is aligned:
    //     - if point's aligned frame matches this layer's aligned version
    //       of point's reference frame:
    //       - we can paste at reference frame or our frame
    //     - else
    //       - we can paste at reference frame, result of aligning reference
    //         frame in our model, or literal source frame
    //   - else
    //     - we can paste at reference (our) frame, or literal source frame
    // - else
    //   - if this layer is aligned:
    //     - we can paste at reference (point's only available) frame,
    //       or result of aligning reference frame in our model
    //   - else
    //     - we can only paste at reference frame
    // 
    // Which of these alternatives are useful?
    //
    // Example: we paste between two tracks that are aligned to the
    // same reference, and the points are at 10s and 20s in the source
    // track, corresponding to 5s and 10s in the reference but 20s and
    // 30s in the target track.
    // 
    // The obvious default is to paste at 20s and 30s; if we aren't
    // doing that, would it be better to paste at 5s and 10s or at 10s
    // and 20s?  We probably don't ever want to do the former, do we?
    // We either want to be literal all the way through, or aligned
    // all the way through.

    for (EventVector::const_iterator i = clip.getPoints().begin();
         i != clip.getPoints().end(); ++i) {

        // In principle, we want to know whether the aligned version
        // of the reference frame in our layer is the same as the
        // source frame contained in the clipboard point.  However,
        // because of rounding during alignment, that won't
        // necessarily be the case even if the clipboard point came
        // from our layer!  What we need to check is whether, if we
        // aligned the clipboard point's frame back to the reference
        // using this layer's alignment, we would obtain the same
        // reference frame as that for the clipboard point.

        // What if the clipboard point has no reference frame?  Then
        // we have to treat it as having its own frame as the
        // reference (i.e. having been copied from the reference
        // model).
        
        sv_frame_t sourceFrame = i->getFrame();
        sv_frame_t referenceFrame = sourceFrame;
        if (i->hasReferenceFrame()) {
            referenceFrame = i->getReferenceFrame();
        }
        sv_frame_t myMappedFrame = alignToReference(v, sourceFrame);

//        cerr << "sourceFrame = " << sourceFrame << ", referenceFrame = " << referenceFrame << " (have = " << i->hasReferenceFrame() << "), myMappedFrame = " << myMappedFrame << endl;

        if (myMappedFrame != referenceFrame) return true;
    }

    return false;
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

void
Layer::MeasureRect::toXml(QTextStream &stream, QString indent) const
{
    stream << indent;
    stream << QString("<measurement ");

    if (haveFrames) {
        stream << QString("startFrame=\"%1\" endFrame=\"%2\" ")
            .arg(startFrame).arg(endFrame);
    } else {
        stream << QString("startX=\"%1\" endX=\"%2\" ")
            .arg(pixrect.x()).arg(pixrect.x() << pixrect.width());
    }

    stream << QString("startY=\"%1\" endY=\"%2\"/>\n")
        .arg(startY).arg(endY);
}

void
Layer::addMeasurementRect(const LayerAttributes &attributes)
{
    MeasureRect rect;
    QString fs = attributes.value("startFrame");
    int x0 = 0, x1 = 0;
    if (fs != "") {
        rect.startFrame = fs.toInt();
        rect.endFrame = attributes.value("endFrame").toInt();
        rect.haveFrames = true;
    } else {
        x0 = attributes.value("startX").toInt();
        x1 = attributes.value("endX").toInt();
        rect.haveFrames = false;
    }
    rect.startY = attributes.value("startY").toDouble();
    rect.endY = attributes.value("endY").toDouble();
    rect.pixrect = QRect(x0, 0, x1 - x0, 0);
    addMeasureRectToSet(rect);
}

QString
Layer::AddMeasurementRectCommand::getName() const
{
    return tr("Make Measurement");
}

void
Layer::AddMeasurementRectCommand::execute()
{
    m_layer->addMeasureRectToSet(m_rect);
}

void
Layer::AddMeasurementRectCommand::unexecute()
{
    m_layer->deleteMeasureRectFromSet(m_rect);
}

QString
Layer::DeleteMeasurementRectCommand::getName() const
{
    return tr("Delete Measurement");
}

void
Layer::DeleteMeasurementRectCommand::execute()
{
    m_layer->deleteMeasureRectFromSet(m_rect);
}

void
Layer::DeleteMeasurementRectCommand::unexecute()
{
    m_layer->addMeasureRectToSet(m_rect);
}

void
Layer::measureStart(LayerGeometryProvider *v, QMouseEvent *e)
{
    m_draggingRect.haveFrames = hasTimeXAxis();

    // NB if haveFrames, then pixrect x and width will be rewritten on
    // every paint according to the current locations of the
    // definitive frame values. So we should set the start frame value
    // once on measureStart, and then not modify it on drag (to avoid
    // drift from repeated conversion back and forth).
    
    m_draggingRect.pixrect = QRect(e->position().x(), e->position().y(), 0, 0);

    if (m_draggingRect.haveFrames) {
        m_draggingRect.startFrame = v->getFrameForX(e->position().x());
        m_draggingRect.endFrame = v->getFrameForX(e->position().x());
    }

    setMeasureRectYCoord(v, m_draggingRect, true, e->position().y());
    setMeasureRectYCoord(v, m_draggingRect, false, e->position().y());

    m_haveDraggingRect = true;
}

void
Layer::measureDrag(LayerGeometryProvider *v, QMouseEvent *e)
{
    if (!m_haveDraggingRect) return;

    m_draggingRect.pixrect.setHeight(e->position().y() - m_draggingRect.pixrect.y());

    if (m_draggingRect.haveFrames) {
        m_draggingRect.endFrame = v->getFrameForX(e->position().x());
    } else {
        m_draggingRect.pixrect.setWidth(e->position().x() - m_draggingRect.pixrect.x());
    }

    setMeasureRectYCoord(v, m_draggingRect, false, e->position().y());
}

void
Layer::measureEnd(LayerGeometryProvider *v, QMouseEvent *e)
{
    if (!m_haveDraggingRect) return;
    measureDrag(v, e);

    if (!m_draggingRect.pixrect.isNull()) {
        CommandHistory::getInstance()->addCommand
            (new AddMeasurementRectCommand(this, m_draggingRect));
    }

    m_haveDraggingRect = false;
}

void
Layer::measureDoubleClick(LayerGeometryProvider *, QMouseEvent *)
{
    // nothing, in the base class
}

void
Layer::deleteCurrentMeasureRect()
{
    if (!m_haveCurrentMeasureRect) return;
    
    MeasureRectSet::const_iterator focusRectItr =
        findFocusedMeasureRect(m_currentMeasureRectPoint);

    if (focusRectItr == m_measureRects.end()) return;

    CommandHistory::getInstance()->addCommand
        (new DeleteMeasurementRectCommand(this, *focusRectItr));
}

void
Layer::paintMeasurementRects(LayerGeometryProvider *v, QPainter &paint,
                             bool showFocus, QPoint focusPoint) const
{
    updateMeasurePixrects(v);

    MeasureRectSet::const_iterator focusRectItr = m_measureRects.end();

    if (m_haveDraggingRect) {

        paintMeasurementRect(v, paint, m_draggingRect, true);

    } else if (showFocus) {

        focusRectItr = findFocusedMeasureRect(focusPoint);
    }

    m_haveCurrentMeasureRect = false;

    for (MeasureRectSet::const_iterator i = m_measureRects.begin(); 
         i != m_measureRects.end(); ++i) {

        bool focused = (i == focusRectItr);
        paintMeasurementRect(v, paint, *i, focused);

        if (focused) {
            m_haveCurrentMeasureRect = true;
            m_currentMeasureRectPoint = focusPoint;
        }
    }
}

bool
Layer::nearestMeasurementRectChanged(LayerGeometryProvider *v, QPoint prev, QPoint now) const
{
    updateMeasurePixrects(v);
    
    MeasureRectSet::const_iterator i0 = findFocusedMeasureRect(prev);
    MeasureRectSet::const_iterator i1 = findFocusedMeasureRect(now);

    return (i0 != i1);
}

void
Layer::updateMeasurePixrects(LayerGeometryProvider *v) const
{
    sv_frame_t sf = v->getStartFrame();
    sv_frame_t ef = v->getEndFrame();

    for (MeasureRectSet::const_iterator i = m_measureRects.begin(); 
         i != m_measureRects.end(); ++i) {

        // This logic depends on the fact that if one measure rect in
        // a layer has frame values, they all will.  That is in fact
        // the case, because haveFrames is based on whether the layer
        // hasTimeXAxis() or not.  Measure rect ordering in the rect
        // set wouldn't work correctly either, if haveFrames could
        // vary.

        if (i->haveFrames) {
            if (i->startFrame >= ef) break;
            if (i->endFrame <= sf) continue;
        }

        int x0 = i->pixrect.x();
        int x1 = x0 + i->pixrect.width();

        if (i->haveFrames) {
            if (i->startFrame >= v->getStartFrame()) {
                x0 = v->getXForFrame(i->startFrame);
            }
            if (i->endFrame <= int(v->getEndFrame())) {
                x1 = v->getXForFrame(i->endFrame);
            }
        }
        
        i->pixrect = QRect(x0, i->pixrect.y(), x1 - x0, i->pixrect.height());

        updateMeasureRectYCoords(v, *i);
    }
}

void
Layer::updateMeasureRectYCoords(LayerGeometryProvider *v, const MeasureRect &r) const
{
    int y0 = int(lrint(r.startY * v->getPaintHeight()));
    int y1 = int(lrint(r.endY * v->getPaintHeight()));
    r.pixrect = QRect(r.pixrect.x(), y0, r.pixrect.width(), y1 - y0);
}

void
Layer::setMeasureRectYCoord(LayerGeometryProvider *v, MeasureRect &r, bool start, int y) const
{
    if (start) {
        r.startY = double(y) / double(v->getPaintHeight());
        r.endY = r.startY;
    } else {
        r.endY = double(y) / double(v->getPaintHeight());
    }
}

void
Layer::setMeasureRectFromPixrect(LayerGeometryProvider *v, MeasureRect &r, QRect pixrect) const
{
    r.pixrect = pixrect;
    r.haveFrames = hasTimeXAxis();
    if (r.haveFrames) {
        r.startFrame = v->getFrameForX(pixrect.x());
        r.endFrame = v->getFrameForX(pixrect.x() + pixrect.width());
    }
    setMeasureRectYCoord(v, r, true, pixrect.y());
    setMeasureRectYCoord(v, r, false, pixrect.y() + pixrect.height());
}

Layer::MeasureRectSet::const_iterator
Layer::findFocusedMeasureRect(QPoint focusPoint) const
{
    double frDist = 0;
    MeasureRectSet::const_iterator focusRectItr = m_measureRects.end();

    for (MeasureRectSet::const_iterator i = m_measureRects.begin(); 
         i != m_measureRects.end(); ++i) {

        if (!i->pixrect.adjusted(-2, -2, 2, 2).contains(focusPoint)) continue;

        int cx = i->pixrect.x() + i->pixrect.width()/2;
        int cy = i->pixrect.y() + i->pixrect.height()/2;
        int xd = focusPoint.x() - cx;
        int yd = focusPoint.y() - cy;
        
        double d = sqrt(double(xd * xd + yd * yd));
        
        if (focusRectItr == m_measureRects.end() || d < frDist) {
            focusRectItr = i;
            frDist = d;
        }
    }

    return focusRectItr;
}

void
Layer::paintMeasurementRect(LayerGeometryProvider *v, QPainter &paint,
                            const MeasureRect &r, bool focus) const
{
    if (r.haveFrames) {
        
        int x0 = -1;
        int x1 = v->getPaintWidth() + 1;
        
        if (r.startFrame >= v->getStartFrame()) {
            x0 = v->getXForFrame(r.startFrame);
        }
        if (r.endFrame <= v->getEndFrame()) {
            x1 = v->getXForFrame(r.endFrame);
        }
        
        QRect pr = QRect(x0, r.pixrect.y(), x1 - x0, r.pixrect.height());
        r.pixrect = pr;
    }

    v->drawMeasurementRect(paint, this, r.pixrect.normalized(), focus);
}

bool
Layer::valueExtentsMatchMine(LayerGeometryProvider *v) const
{
    double min, min_;
    double max, max_;
    bool logarithmic, logarithmic_;
    QString unit;

    if (!getValueExtents(min_, max_, logarithmic_, unit)) {
        return false;
    }

    if (!v->getVisibleExtentsForUnit(unit, min, max, logarithmic)) {
        return false;
    }

    if (min != min_ ||
        max != max_ ||
        logarithmic != logarithmic_) {
        return false;
    }

    return true;
}

void
Layer::toXml(QTextStream &stream,
             QString indent, QString extraAttributes) const
{
    stream << indent;

    if (m_presentationName != "") {
        extraAttributes = QString("%1 presentationName=\"%2\"")
            .arg(extraAttributes).arg(encodeEntities(m_presentationName));
    }

    int modelExportId = -1;
    auto model = ModelById::get(getModel());
    if (model) modelExportId = model->getExportId();
    
    stream << QString("<layer id=\"%2\" type=\"%1\" name=\"%3\" model=\"%4\" %5")
        .arg(encodeEntities(LayerFactory::getInstance()->getLayerTypeName
                            (LayerFactory::getInstance()->getLayerType(this))))
        .arg(getExportId())
        .arg(encodeEntities(objectName()))
        .arg(modelExportId)
        .arg(extraAttributes);

    if (m_measureRects.empty()) {
        stream << QString("/>\n");
        return;
    }

    stream << QString(">\n");

    for (MeasureRectSet::const_iterator i = m_measureRects.begin();
         i != m_measureRects.end(); ++i) {
        i->toXml(stream, indent + "  ");
    }

    stream << QString("</layer>\n");
}

void
Layer::toBriefXml(QTextStream &stream,
                  QString indent, QString extraAttributes) const
{
    stream << indent;

    if (m_presentationName != "") {
        extraAttributes = QString("%1 presentationName=\"%2\"")
            .arg(extraAttributes).arg(encodeEntities(m_presentationName));
    }

    int modelExportId = -1;
    auto model = ModelById::get(getModel());
    if (model) modelExportId = model->getExportId();

    stream << QString("<layer id=\"%2\" type=\"%1\" name=\"%3\" model=\"%4\" %5/>\n")
        .arg(encodeEntities(LayerFactory::getInstance()->getLayerTypeName
                            (LayerFactory::getInstance()->getLayerType(this))))
        .arg(getExportId())
        .arg(encodeEntities(objectName()))
        .arg(modelExportId)
        .arg(extraAttributes);
}

} // end namespace sv

