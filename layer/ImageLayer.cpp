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

#include "ImageLayer.h"

#include "data/model/Model.h"
#include "base/RealTime.h"
#include "base/Profiler.h"
#include "view/View.h"

#include "data/model/ImageModel.h"

#include "widgets/ImageDialog.h"

#include <QPainter>
#include <QMouseEvent>
#include <QInputDialog>

#include <iostream>
#include <cmath>

ImageLayer::ImageMap
ImageLayer::m_images;

ImageLayer::ImageLayer() :
    Layer(),
    m_model(0),
    m_editing(false),
    m_originalPoint(0, "", ""),
    m_editingPoint(0, "", ""),
    m_editingCommand(0)
{
    
}

void
ImageLayer::setModel(ImageModel *model)
{
    if (m_model == model) return;
    m_model = model;

    connect(m_model, SIGNAL(modelChanged()), this, SIGNAL(modelChanged()));
    connect(m_model, SIGNAL(modelChanged(size_t, size_t)),
	    this, SIGNAL(modelChanged(size_t, size_t)));

    connect(m_model, SIGNAL(completionChanged()),
	    this, SIGNAL(modelCompletionChanged()));

//    std::cerr << "ImageLayer::setModel(" << model << ")" << std::endl;

    emit modelReplaced();
}

Layer::PropertyList
ImageLayer::getProperties() const
{
    return Layer::getProperties();
}

QString
ImageLayer::getPropertyLabel(const PropertyName &name) const
{
    return "";
}

Layer::PropertyType
ImageLayer::getPropertyType(const PropertyName &name) const
{
    return Layer::getPropertyType(name);
}

int
ImageLayer::getPropertyRangeAndValue(const PropertyName &name,
				    int *min, int *max, int *deflt) const
{
    return Layer::getPropertyRangeAndValue(name, min, max, deflt);
}

QString
ImageLayer::getPropertyValueLabel(const PropertyName &name,
				 int value) const
{
    return Layer::getPropertyValueLabel(name, value);
}

void
ImageLayer::setProperty(const PropertyName &name, int value)
{
    Layer::setProperty(name, value);
}

bool
ImageLayer::getValueExtents(float &, float &, bool &, QString &) const
{
    return false;
}

bool
ImageLayer::isLayerScrollable(const View *v) const
{
    QPoint discard;
    return !v->shouldIlluminateLocalFeatures(this, discard);
}


ImageModel::PointList
ImageLayer::getLocalPoints(View *v, int x, int y) const
{
    if (!m_model) return ImageModel::PointList();

    std::cerr << "ImageLayer::getLocalPoints(" << x << "," << y << "):";

    long frame0 = v->getFrameForX(-150);
    long frame1 = v->getFrameForX(v->width() + 150);
    
    ImageModel::PointList points(m_model->getPoints(frame0, frame1));

    ImageModel::PointList rv;

    //!!! need to store drawn size as well as original size for each
    //image, but for now:

    for (ImageModel::PointList::iterator i = points.begin();
	 i != points.end(); ++i) {

	const ImageModel::Point &p(*i);

	int px = v->getXForFrame(p.frame);

        if (x >= px && x < px + 100) {
            rv.insert(p);
        }
    }

    std::cerr << rv.size() << " point(s)" << std::endl;

    return rv;
}

QString
ImageLayer::getFeatureDescription(View *v, QPoint &pos) const
{
    int x = pos.x();

    if (!m_model || !m_model->getSampleRate()) return "";

    ImageModel::PointList points = getLocalPoints(v, x, pos.y());

    if (points.empty()) {
	if (!m_model->isReady()) {
	    return tr("In progress");
	} else {
	    return "";
	}
    }

    long useFrame = points.begin()->frame;

    RealTime rt = RealTime::frame2RealTime(useFrame, m_model->getSampleRate());

    QString text;
/*    
    if (points.begin()->label == "") {
	text = QString(tr("Time:\t%1\nHeight:\t%2\nLabel:\t%3"))
	    .arg(rt.toText(true).c_str())
	    .arg(points.begin()->height)
	    .arg(points.begin()->label);
    }

    pos = QPoint(v->getXForFrame(useFrame),
		 getYForHeight(v, points.begin()->height));
*/
    return text;
}


//!!! too much overlap with TimeValueLayer/TimeInstantLayer/TextLayer

bool
ImageLayer::snapToFeatureFrame(View *v, int &frame,
			      size_t &resolution,
			      SnapType snap) const
{
    if (!m_model) {
	return Layer::snapToFeatureFrame(v, frame, resolution, snap);
    }

    resolution = m_model->getResolution();
    ImageModel::PointList points;

    if (snap == SnapNeighbouring) {
	
	points = getLocalPoints(v, v->getXForFrame(frame), -1);
	if (points.empty()) return false;
	frame = points.begin()->frame;
	return true;
    }    

    points = m_model->getPoints(frame, frame);
    int snapped = frame;
    bool found = false;

    for (ImageModel::PointList::const_iterator i = points.begin();
	 i != points.end(); ++i) {

	if (snap == SnapRight) {

	    if (i->frame > frame) {
		snapped = i->frame;
		found = true;
		break;
	    }

	} else if (snap == SnapLeft) {

	    if (i->frame <= frame) {
		snapped = i->frame;
		found = true; // don't break, as the next may be better
	    } else {
		break;
	    }

	} else { // nearest

	    ImageModel::PointList::const_iterator j = i;
	    ++j;

	    if (j == points.end()) {

		snapped = i->frame;
		found = true;
		break;

	    } else if (j->frame >= frame) {

		if (j->frame - frame < frame - i->frame) {
		    snapped = j->frame;
		} else {
		    snapped = i->frame;
		}
		found = true;
		break;
	    }
	}
    }

    frame = snapped;
    return found;
}

void
ImageLayer::paint(View *v, QPainter &paint, QRect rect) const
{
    if (!m_model || !m_model->isOK()) return;

    int sampleRate = m_model->getSampleRate();
    if (!sampleRate) return;

//    Profiler profiler("ImageLayer::paint", true);

    int x0 = rect.left(), x1 = rect.right();
    long frame0 = v->getFrameForX(x0);
    long frame1 = v->getFrameForX(x1);

    ImageModel::PointList points(m_model->getPoints(frame0, frame1));
    if (points.empty()) return;

    QColor penColour;
    penColour = v->getForeground();

//    std::cerr << "ImageLayer::paint: resolution is "
//	      << m_model->getResolution() << " frames" << std::endl;

    QPoint localPos;
    long illuminateFrame = -1;

    if (v->shouldIlluminateLocalFeatures(this, localPos)) {
	ImageModel::PointList localPoints = getLocalPoints(v, localPos.x(),
							  localPos.y());
	if (!localPoints.empty()) illuminateFrame = localPoints.begin()->frame;
    }

    paint.save();
    paint.setClipRect(rect.x(), 0, rect.width(), v->height());
    
    for (ImageModel::PointList::const_iterator i = points.begin();
	 i != points.end(); ++i) {

	const ImageModel::Point &p(*i);

	int x = v->getXForFrame(p.frame);

        int nx = x + 2000;
        ImageModel::PointList::const_iterator j = i;
        ++j;
        if (j != points.end()) {
            int jx = v->getXForFrame(j->frame);
            if (jx < nx) nx = jx;
        }
/*
	if (illuminateFrame == p.frame) {
	    paint.setBrush(penColour);
            paint.setPen(v->getBackground());
	} else {
	    paint.setPen(penColour);
	    paint.setBrush(brushColour);
	}
*/
	QString label = p.label;
        QString imageName = p.image;

        int nw = nx - x;
        if (nw < 10) nw = 20;

        int top = 10;
        if (v->height() < 50) top = 5;
        
        int bottom = top;

        QRect labelRect;
        if (label != "") {
            float aspect = getImageAspect(imageName);
            int iw = lrintf((v->height() - v->height()/4 - top - bottom)
                            * aspect);
            labelRect = paint.fontMetrics().boundingRect
                (QRect(0, 0, iw, v->height()/4),
                 Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, label);
            bottom += labelRect.height() + 5;
        }

        QImage image = getImage(v,
                                imageName,
                                QSize(nw, v->height() - top - bottom));

        if (image.isNull()) {
            image = QImage(":icons/emptypage.png");
        }

	paint.setRenderHint(QPainter::Antialiasing, false);

        int boxWidth = image.width();

        if (label != "") {
            boxWidth = std::max(boxWidth, labelRect.width());
        }

	paint.drawRect(x-1, top-1, boxWidth+1, v->height() - 2 * top +1);

        paint.setRenderHint(QPainter::Antialiasing, true);

        paint.drawImage(x + (boxWidth - image.width())/2, top, image);

        paint.drawText(QRect(x, v->height() - bottom + 5,
                             boxWidth, labelRect.height()),
                       Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
		       label);
    }

    paint.restore();
    paint.setRenderHint(QPainter::Antialiasing, false);
}

void
ImageLayer::setLayerDormant(const View *v, bool dormant)
{
    if (dormant) {
        // Delete the images named in the view's scaled map from the
        // general image map as well.  They can always be re-loaded
        // if it turns out another view still needs them.
        for (ImageMap::iterator i = m_scaled[v].begin();
             i != m_scaled[v].end(); ++i) {
            m_images.erase(i->first);
        }
        m_scaled.erase(v);
    }
}

//!!! how to reap no-longer-used images?

float
ImageLayer::getImageAspect(QString name) const
{
    if (m_images.find(name) == m_images.end()) {
        m_images[name] = QImage(name);
    }
    if (m_images[name].isNull()) return 1.f;
    return float(m_images[name].width()) / float(m_images[name].height());
}

QImage 
ImageLayer::getImage(View *v, QString name, QSize maxSize) const
{
    bool need = false;

//    std::cerr << "ImageLayer::getImage(" << v << ", " << name.toStdString() << ", ("
//              << maxSize.width() << "x" << maxSize.height() << "))" << std::endl;

    if (!m_scaled[v][name].isNull() &&
        (m_scaled[v][name].width() == maxSize.width() ||
         m_scaled[v][name].height() == maxSize.height())) {
//        std::cerr << "cache hit" << std::endl;
        return m_scaled[v][name];
    }

    if (m_images.find(name) == m_images.end()) {
        m_images[name] = QImage(name);
    }

    if (m_images[name].isNull()) {
//        std::cerr << "null image" << std::endl;
        m_scaled[v][name] = QImage();
    } else {
        m_scaled[v][name] =
            m_images[name].scaled(maxSize,
                                  Qt::KeepAspectRatio,
                                  Qt::SmoothTransformation);
    }

    return m_scaled[v][name];
}

void
ImageLayer::drawStart(View *v, QMouseEvent *e)
{
//    std::cerr << "ImageLayer::drawStart(" << e->x() << "," << e->y() << ")" << std::endl;

    if (!m_model) {
	std::cerr << "ImageLayer::drawStart: no model" << std::endl;
	return;
    }

    long frame = v->getFrameForX(e->x());
    if (frame < 0) frame = 0;
    frame = frame / m_model->getResolution() * m_model->getResolution();

    m_editingPoint = ImageModel::Point(frame, "", "");
    m_originalPoint = m_editingPoint;

    if (m_editingCommand) m_editingCommand->finish();
    m_editingCommand = new ImageModel::EditCommand(m_model, "Add Image");
    m_editingCommand->addPoint(m_editingPoint);

    m_editing = true;
}

void
ImageLayer::drawDrag(View *v, QMouseEvent *e)
{
//    std::cerr << "ImageLayer::drawDrag(" << e->x() << "," << e->y() << ")" << std::endl;

    if (!m_model || !m_editing) return;

    long frame = v->getFrameForX(e->x());
    if (frame < 0) frame = 0;
    frame = frame / m_model->getResolution() * m_model->getResolution();

    m_editingCommand->deletePoint(m_editingPoint);
    m_editingPoint.frame = frame;
    m_editingCommand->addPoint(m_editingPoint);
}

void
ImageLayer::drawEnd(View *v, QMouseEvent *)
{
//    std::cerr << "ImageLayer::drawEnd(" << e->x() << "," << e->y() << ")" << std::endl;
    if (!m_model || !m_editing) return;

    bool ok = false;

    ImageDialog dialog(tr("Select image"), "", tr("<no label>"));
    if (dialog.exec() == QDialog::Accepted) {
	ImageModel::ChangeImageCommand *command =
	    new ImageModel::ChangeImageCommand
            (m_model, m_editingPoint, dialog.getImage(), dialog.getLabel());
	m_editingCommand->addCommand(command);
    }

    m_editingCommand->finish();
    m_editingCommand = 0;
    m_editing = false;
}

void
ImageLayer::editStart(View *v, QMouseEvent *e)
{
//    std::cerr << "ImageLayer::editStart(" << e->x() << "," << e->y() << ")" << std::endl;

    if (!m_model) return;

    ImageModel::PointList points = getLocalPoints(v, e->x(), e->y());
    if (points.empty()) return;

    m_editOrigin = e->pos();
    m_editingPoint = *points.begin();
    m_originalPoint = m_editingPoint;

    if (m_editingCommand) {
	m_editingCommand->finish();
	m_editingCommand = 0;
    }

    m_editing = true;
}

void
ImageLayer::editDrag(View *v, QMouseEvent *e)
{
    if (!m_model || !m_editing) return;

    long frameDiff = v->getFrameForX(e->x()) - v->getFrameForX(m_editOrigin.x());
    long frame = m_originalPoint.frame + frameDiff;

    if (frame < 0) frame = 0;
    frame = (frame / m_model->getResolution()) * m_model->getResolution();

    if (!m_editingCommand) {
	m_editingCommand = new ImageModel::EditCommand(m_model, tr("Move Image"));
    }

    m_editingCommand->deletePoint(m_editingPoint);
    m_editingPoint.frame = frame;
    m_editingCommand->addPoint(m_editingPoint);
}

void
ImageLayer::editEnd(View *, QMouseEvent *)
{
//    std::cerr << "ImageLayer::editEnd(" << e->x() << "," << e->y() << ")" << std::endl;
    if (!m_model || !m_editing) return;

    if (m_editingCommand) {
	m_editingCommand->finish();
    }
    
    m_editingCommand = 0;
    m_editing = false;
}

bool
ImageLayer::editOpen(View *v, QMouseEvent *e)
{
    if (!m_model) return false;

    ImageModel::PointList points = getLocalPoints(v, e->x(), e->y());
    if (points.empty()) return false;

    QString image = points.begin()->image;
    QString label = points.begin()->label;

    ImageDialog dialog(tr("Select image"),
                       image,
                       label);

    if (dialog.exec() == QDialog::Accepted) {
	ImageModel::ChangeImageCommand *command =
	    new ImageModel::ChangeImageCommand
            (m_model, *points.begin(), dialog.getImage(), dialog.getLabel());
        CommandHistory::getInstance()->addCommand(command);
    }

    return true;
}    

void
ImageLayer::moveSelection(Selection s, size_t newStartFrame)
{
    if (!m_model) return;

    ImageModel::EditCommand *command =
	new ImageModel::EditCommand(m_model, tr("Drag Selection"));

    ImageModel::PointList points =
	m_model->getPoints(s.getStartFrame(), s.getEndFrame());

    for (ImageModel::PointList::iterator i = points.begin();
	 i != points.end(); ++i) {

	if (s.contains(i->frame)) {
	    ImageModel::Point newPoint(*i);
	    newPoint.frame = i->frame + newStartFrame - s.getStartFrame();
	    command->deletePoint(*i);
	    command->addPoint(newPoint);
	}
    }

    command->finish();
}

void
ImageLayer::resizeSelection(Selection s, Selection newSize)
{
    if (!m_model) return;

    ImageModel::EditCommand *command =
	new ImageModel::EditCommand(m_model, tr("Resize Selection"));

    ImageModel::PointList points =
	m_model->getPoints(s.getStartFrame(), s.getEndFrame());

    double ratio =
	double(newSize.getEndFrame() - newSize.getStartFrame()) /
	double(s.getEndFrame() - s.getStartFrame());

    for (ImageModel::PointList::iterator i = points.begin();
	 i != points.end(); ++i) {

	if (s.contains(i->frame)) {

	    double target = i->frame;
	    target = newSize.getStartFrame() + 
		double(target - s.getStartFrame()) * ratio;

	    ImageModel::Point newPoint(*i);
	    newPoint.frame = lrint(target);
	    command->deletePoint(*i);
	    command->addPoint(newPoint);
	}
    }

    command->finish();
}

void
ImageLayer::deleteSelection(Selection s)
{
    if (!m_model) return;

    ImageModel::EditCommand *command =
	new ImageModel::EditCommand(m_model, tr("Delete Selection"));

    ImageModel::PointList points =
	m_model->getPoints(s.getStartFrame(), s.getEndFrame());

    for (ImageModel::PointList::iterator i = points.begin();
	 i != points.end(); ++i) {
	if (s.contains(i->frame)) command->deletePoint(*i);
    }

    command->finish();
}

void
ImageLayer::copy(Selection s, Clipboard &to)
{
    if (!m_model) return;

    ImageModel::PointList points =
	m_model->getPoints(s.getStartFrame(), s.getEndFrame());

    for (ImageModel::PointList::iterator i = points.begin();
	 i != points.end(); ++i) {
	if (s.contains(i->frame)) {
            //!!! inadequate
            Clipboard::Point point(i->frame, i->label);
            to.addPoint(point);
        }
    }
}

bool
ImageLayer::paste(const Clipboard &from, int frameOffset, bool /* interactive */)
{
    if (!m_model) return false;

    const Clipboard::PointList &points = from.getPoints();

    ImageModel::EditCommand *command =
	new ImageModel::EditCommand(m_model, tr("Paste"));

    for (Clipboard::PointList::const_iterator i = points.begin();
         i != points.end(); ++i) {
        
        if (!i->haveFrame()) continue;
        size_t frame = 0;
        if (frameOffset > 0 || -frameOffset < i->getFrame()) {
            frame = i->getFrame() + frameOffset;
        }
        ImageModel::Point newPoint(frame);

        //!!! inadequate
        
        if (i->haveLabel()) {
            newPoint.label = i->getLabel();
        } else if (i->haveValue()) {
            newPoint.label = QString("%1").arg(i->getValue());
        } else {
            newPoint.label = tr("New Point");
        }
        
        command->addPoint(newPoint);
    }

    command->finish();
    return true;
}

QString
ImageLayer::toXmlString(QString indent, QString extraAttributes) const
{
    return Layer::toXmlString(indent, extraAttributes);
}

void
ImageLayer::setProperties(const QXmlAttributes &attributes)
{
}

