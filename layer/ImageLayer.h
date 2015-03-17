/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006-2007 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef _IMAGE_LAYER_H_
#define _IMAGE_LAYER_H_

#include "Layer.h"
#include "data/model/ImageModel.h"

#include <QObject>
#include <QColor>
#include <QImage>
#include <QMutex>

#include <map>

class View;
class QPainter;
class FileSource;

class ImageLayer : public Layer
{
    Q_OBJECT

public:
    ImageLayer();
    virtual ~ImageLayer();

    virtual void paint(LayerGeometryProvider *v, QPainter &paint, QRect rect) const;

    virtual QString getFeatureDescription(LayerGeometryProvider *v, QPoint &) const;

    virtual bool snapToFeatureFrame(LayerGeometryProvider *v, sv_frame_t &frame,
				    int &resolution,
				    SnapType snap) const;

    virtual void drawStart(LayerGeometryProvider *v, QMouseEvent *);
    virtual void drawDrag(LayerGeometryProvider *v, QMouseEvent *);
    virtual void drawEnd(LayerGeometryProvider *v, QMouseEvent *);

    virtual void editStart(LayerGeometryProvider *v, QMouseEvent *);
    virtual void editDrag(LayerGeometryProvider *v, QMouseEvent *);
    virtual void editEnd(LayerGeometryProvider *v, QMouseEvent *);

    virtual void moveSelection(Selection s, sv_frame_t newStartFrame);
    virtual void resizeSelection(Selection s, Selection newSize);
    virtual void deleteSelection(Selection s);

    virtual void copy(LayerGeometryProvider *v, Selection s, Clipboard &to);
    virtual bool paste(LayerGeometryProvider *v, const Clipboard &from, sv_frame_t frameOffset,
                       bool interactive);

    virtual bool editOpen(LayerGeometryProvider *, QMouseEvent *); // on double-click

    virtual const Model *getModel() const { return m_model; }
    void setModel(ImageModel *model);

    virtual PropertyList getProperties() const;
    virtual QString getPropertyLabel(const PropertyName &) const;
    virtual PropertyType getPropertyType(const PropertyName &) const;
    virtual int getPropertyRangeAndValue(const PropertyName &,
                                         int *min, int *max, int *deflt) const;
    virtual QString getPropertyValueLabel(const PropertyName &,
					  int value) const;
    virtual void setProperty(const PropertyName &, int value);

    virtual ColourSignificance getLayerColourSignificance() const {
        return ColourAbsent;
    }

    virtual bool isLayerScrollable(const LayerGeometryProvider *v) const;

    virtual bool isLayerEditable() const { return true; }

    virtual int getCompletion(LayerGeometryProvider *) const { return m_model->getCompletion(); }

    virtual bool getValueExtents(double &min, double &max,
                                 bool &logarithmic, QString &unit) const;

    virtual void toXml(QTextStream &stream, QString indent = "",
                       QString extraAttributes = "") const;

    virtual int getVerticalScaleWidth(LayerGeometryProvider *, bool, QPainter &) const { return 0; }

    virtual void setLayerDormant(const LayerGeometryProvider *v, bool dormant);

    void setProperties(const QXmlAttributes &attributes);

    virtual bool addImage(sv_frame_t frame, QString url); // using a command

protected slots:
    void checkAddSources();
    void fileSourceReady();

protected:
    ImageModel::PointList getLocalPoints(LayerGeometryProvider *v, int x, int y) const;

    bool getImageOriginalSize(QString name, QSize &size) const;
    QImage getImage(LayerGeometryProvider *v, QString name, QSize maxSize) const;

    void drawImage(LayerGeometryProvider *v, QPainter &paint, const ImageModel::Point &p,
                   int x, int nx) const;

    //!!! how to reap no-longer-used images?

    typedef std::map<QString, QImage> ImageMap;
    typedef std::map<const LayerGeometryProvider *, ImageMap> ViewImageMap;
    typedef std::map<QString, FileSource *> FileSourceMap;

    static ImageMap m_images;
    static QMutex m_imageMapMutex;
    mutable ViewImageMap m_scaled;
    mutable FileSourceMap m_fileSources;

    QString getLocalFilename(QString img) const;
    void checkAddSource(QString img) const;

    ImageModel *m_model;
    bool m_editing;
    QPoint m_editOrigin;
    ImageModel::Point m_originalPoint;
    ImageModel::Point m_editingPoint;
    ImageModel::EditCommand *m_editingCommand;

    void finish(ImageModel::EditCommand *command) {
        Command *c = command->finish();
        if (c) CommandHistory::getInstance()->addCommand(c, false);
    }
};

#endif

