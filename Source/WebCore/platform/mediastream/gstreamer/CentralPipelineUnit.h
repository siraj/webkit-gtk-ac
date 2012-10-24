/*
 *  Copyright (C) 2012 Collabora Ltd. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#ifndef CentralPipelineUnit_h
#define CentralPipelineUnit_h

#if ENABLE(MEDIA_STREAM)

#include <gst/gst.h>
#include <wtf/HashMap.h>
#include <wtf/gobject/GRefPtr.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

typedef void (*SourceInsertedCallback)(GstElement* source, GstPad* pad, gpointer userData);

class SourceFactory;
class SourceExtensionFactory;

class CentralPipelineUnit {
public:
    CentralPipelineUnit();
    virtual ~CentralPipelineUnit();

    void registerSourceFactory(SourceFactory*, const String& sourceId);
    void deregisterSourceFactory(const String& sourceId);
    bool connectToSource(const String& sourceId, GstElement* sink, GstPad* sinkPad = 0);
    bool disconnectFromSource(const String& sourceId, GstElement* sink, GstPad* sinkPad = 0);

    GstElement* pipeline() const;

private:
    class Source {
    public:
        Source() : m_sourceElement(0), m_sourcePad(0), m_sourceFactory(0), m_removeWhenNotUsed(true) { }
        Source(GstElement* sourceElement, GstPad* sourcePad, SourceFactory* sourceFactory, bool removeWhenNotUsed) : m_sourceElement(sourceElement), m_sourcePad(sourcePad), m_sourceFactory(sourceFactory), m_removeWhenNotUsed(removeWhenNotUsed) { }

        GRefPtr<GstElement> m_sourceElement;
        GRefPtr<GstPad> m_sourcePad;
        SourceFactory* m_sourceFactory;
        bool m_removeWhenNotUsed;
    };

    typedef HashMap<String, Source> PipelineMap;

    typedef HashMap<String, String> SourceIdLookupMap;
    typedef HashMap<String, SourceFactory*> SourceFactoryMap;
    typedef struct {
        SourceExtensionFactory* sourceExtensionFactory;
        String sourceToExtend;
    } SourceExtensionFactoryInfo;
    typedef HashMap<String, SourceExtensionFactoryInfo> SourceExtensionFactoryMap;

    typedef struct {
        GRefPtr<GstElement> extension;
        String sourceToExtend;
    } SourceExtensionInfo;
    typedef HashMap<String, SourceExtensionInfo> SourceExtensionMap;

    typedef enum {
        ConnectToSource,
        DisconnectFromSource,
        RemoveSource,
        InsertSource
    } TaskType;

    typedef struct {
        String sourceId;
        TaskType type;
        GstElement* sink;
        GstElement* source;
        SourceFactory* sourceFactory;
        SourceInsertedCallback callback;
        gpointer userData;
        GstPad* pad;
        bool sourceFactoryRemoved;
    } PipelineTask;
    typedef Vector<PipelineTask*> PipelineTaskVector;

    bool connectAndGetSourceElement(const String& sourceId, GstElement** sourceElement, GstPad** sourcePad);
    void disconnectSourceFromPipeline(GstElement* source, GstPad* sourcePad);
    void disconnectSourceWithMultiplePadsFromPipeline(GstElement* source);
    guint disconnectSinkFromPipeline(GstElement* sink);
    void removeSourceExtensionFromBehind(GstElement* sourceExtension);

    gint disconnectSinkFromTee(GstElement* tee, GstElement* sink, GstPad*);

    void removeSourceFactoryForSource(GstElement* source);

    GstElement* m_pipeline;
    PipelineMap m_pipelineMap;
    SourceIdLookupMap m_sourceIdLookupMap;
    SourceFactoryMap m_sourceFactoryMap;
};

CentralPipelineUnit& centralPipelineUnit();

}

#endif // ENABLE(MEDIA_STREAM)

#endif // CentralPipelineUnit_h
