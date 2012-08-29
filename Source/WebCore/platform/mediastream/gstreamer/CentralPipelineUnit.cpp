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

#include "config.h"

#if ENABLE(MEDIA_STREAM)

#include "CentralPipelineUnit.h"

#include "Logging.h"
#include "SourceFactory.h"

#include <glib/gprintf.h>
#include <stdlib.h>
#include <wtf/MainThread.h>
#include <wtf/StdLibExtras.h>
#include <wtf/gobject/GOwnPtr.h>
#include <wtf/text/CString.h>


namespace WebCore {

static gboolean messageCallback(GstBus*, GstMessage*, gpointer data);
static String generateElementPadId(GstElement*, GstPad*);

#if 0
gboolean timerDebugDot(gpointer data)
{
    GstBin* bin = reinterpret_cast<GstBin*>(data);
    static guint i = 0;
    gchar* binName = gst_element_get_name(bin);
    pid_t pid = getpid();
    gchar* dotName = g_strdup_printf("%u.%u.%s", pid, i++, binName);
    LOG(MediaStream, "Maybe writing dotfile %s", dotName);
    GST_DEBUG_BIN_TO_DOT_FILE(bin, static_cast<GstDebugGraphDetails>(GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE | GST_DEBUG_GRAPH_SHOW_NON_DEFAULT_PARAMS | GST_DEBUG_GRAPH_SHOW_STATES), dotName);
    g_free(dotName);

    return true;
}
#endif

CentralPipelineUnit& centralPipelineUnit()
{
    ASSERT(isMainThread());
    DEFINE_STATIC_LOCAL(CentralPipelineUnit, instance, ());
    return instance;
}

CentralPipelineUnit::CentralPipelineUnit()
{
    GOwnPtr<GError> error;
    gboolean gstInitialized = gst_init_check(0, 0, &error.outPtr());
    g_assert(gstInitialized);

    m_pipeline = gst_pipeline_new("mediastream_pipeline");
    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(m_pipeline));
    if (bus) {
        gst_bus_add_signal_watch(bus);
        gst_bus_set_sync_handler(bus, gst_bus_sync_signal_handler, 0);
        g_signal_connect(bus, "sync-message", G_CALLBACK(messageCallback), this);
        gst_object_unref(bus);
    } else
        gst_object_unref(m_pipeline);
    gst_element_set_state(m_pipeline, GST_STATE_PLAYING);

#if 0
    g_timeout_add_seconds(5, timerDebugDot, m_pipeline);
#endif
}

CentralPipelineUnit::~CentralPipelineUnit()
{
}

void CentralPipelineUnit::registerSourceFactory(SourceFactory* factory, const String& sourceId)
{
    m_sourceFactoryMap.add(sourceId, factory);
}

void CentralPipelineUnit::deregisterSourceFactory(const String& sourceId)
{
    m_sourceFactoryMap.remove(sourceId);
}

bool CentralPipelineUnit::insertSource(GstElement* sourceElement, GstPad* pad, const String& sourceId, SourceInsertedCallback callback, gpointer userData)
{
    LOG(MediaStream, "Inserting source with id=%s", sourceId.ascii().data());

    if (sourceId.isEmpty() || !sourceElement) {
        LOG(MediaStream, "ERROR, Empty source id or invalid source element");
        return false;
    }

    PipelineMap::iterator sourceIt = m_pipelineMap.find(sourceId);
    if (sourceIt != m_pipelineMap.end()) {
        LOG(MediaStream, "Could not find source with id=%s", sourceId.ascii().data());
        return false;
    }

    GstElement* parent = (GstElement*)gst_element_get_parent(sourceElement);
    if (!parent) {
        LOG(MediaStream, "Added source element to pipeline");
        gst_bin_add(GST_BIN(m_pipeline), sourceElement);
    }

    if (pad) {
        LOG(MediaStream, "Pad available. Adding tee and fakesink to it.");
        GstElement* tee = gst_element_factory_make("tee", NULL);
        gst_bin_add(GST_BIN(m_pipeline), tee);
        GstPad* teeSinkPad = gst_element_get_static_pad(tee, "sink");
        gst_pad_link(pad, teeSinkPad);

        // FIXME: How can we do this without the fakesink?
        //        We get link-error if tee is not connected to something.
        GstElement* fakesink = gst_element_factory_make("fakesink", NULL);
        g_object_set(fakesink, "sync", FALSE, "async", FALSE, NULL);
        gst_bin_add(GST_BIN(m_pipeline), fakesink);
        gst_element_link(tee, fakesink);
        LOG(MediaStream, "Trying to sync state");
        gst_element_sync_state_with_parent(sourceElement);
        gst_element_sync_state_with_parent(tee);
        gst_element_sync_state_with_parent(fakesink);
    } else {
        LOG(MediaStream, "Trying to sync state");
        gst_element_sync_state_with_parent(sourceElement);
    }

    m_pipelineMap.add(sourceId, Source(sourceElement, pad, 0, false));
    m_sourceIdLookupMap.add(generateElementPadId(sourceElement, pad), sourceId);

    if (callback)
        callback(sourceElement, pad, userData);

    if (pad)
        gst_pad_set_blocked_async(pad, FALSE, NULL, 0);

    return true;
}

bool CentralPipelineUnit::removeSource(const String& sourceId, bool sourceFactoryRemoved)
{
    LOG(MediaStream, "Removing source with id=%s", sourceId.ascii().data());

    if (sourceId.isEmpty()) {
        LOG(MediaStream, "ERROR, Empty source id");
        return false;
    }

    PipelineMap::iterator sourceIt = m_pipelineMap.find(sourceId);
    if (sourceIt == m_pipelineMap.end()) {
        LOG(MediaStream, "Could not find source with id=%s", sourceId.ascii().data());
        return false;
    }

    Source& source = sourceIt->second;
    GstElement* sourceElement = source.m_sourceElement.get();
    GstPad* sourcePad = source.m_sourcePad.get();

    if (sourceFactoryRemoved)
        removeSourceFactoryForSource(sourceElement);

    if (sourcePad)
        disconnectSourceFromPipeline(sourceElement, sourcePad);
    else
        disconnectSourceWithMultiplePadsFromPipeline(sourceElement); // does not work on sourceExtensions

    return true;
}

bool CentralPipelineUnit::connectAndGetSourceElement(const String& sourceId, GstElement** sourceElement, GstPad** sourcePad)
{
    LOG(MediaStream, "Connecting to source element %s", sourceId.ascii().data());

    CentralPipelineUnit::PipelineMap::iterator sourceIt = m_pipelineMap.find(sourceId);
    if (sourceIt != m_pipelineMap.end()) {
        Source& source = sourceIt->second;
        LOG(MediaStream, "Source element %s already in pipeline, using it.", sourceId.ascii().data());
        *sourceElement = source.m_sourceElement.get();
        *sourcePad = source.m_sourcePad.get();
        return true;
    }

    CentralPipelineUnit::SourceFactoryMap::iterator sourceFactoryIt = m_sourceFactoryMap.find(sourceId);
    if (sourceFactoryIt != m_sourceFactoryMap.end()) {
        LOG(MediaStream, "Found a SourceFactory. Creating source element %s", sourceId.ascii().data());
        SourceFactory* sourceFactory = sourceFactoryIt->second;
        *sourceElement = sourceFactory->createSource(sourceId, sourcePad);
        if (!*sourceElement) {
            LOG(MediaStream, "ERROR, unable to create source element");
            *sourceElement = 0;
            *sourcePad = 0;
            return false;
        }

        if (!*sourcePad) {
            LOG(MediaStream, "SourceFactory could not create a source pad, trying the element static \"src\" pad");
            *sourcePad = gst_element_get_static_pad(*sourceElement, "src");
        }

        if (!*sourcePad) {
            LOG(MediaStream, "ERROR, unable to retrieve element source pad");
            gst_object_unref(*sourceElement);
            *sourceElement = 0;
            return false;
        }

        GstElement* tee = gst_element_factory_make("tee", NULL);
        if (!tee) {
            LOG(MediaStream, "ERROR, Got no tee element");
            gst_object_unref(*sourceElement);
            gst_object_unref(*sourcePad);
            *sourceElement = 0;
            *sourcePad = 0;
            return false;
        }

        gst_bin_add_many(GST_BIN(m_pipeline), *sourceElement, tee, NULL);

        gst_element_sync_state_with_parent(*sourceElement);
        gst_element_sync_state_with_parent(tee);

        GstPad* sinkpad = gst_element_get_static_pad(tee, "sink");
        gst_pad_link(*sourcePad, sinkpad);
    }
    return false;
}

bool CentralPipelineUnit::connectToSource(const String& sourceId, GstElement* sink, GstPad* sinkpad)
{
    LOG(MediaStream, "Connecting to source with id=%s, sink=%p, sinkpad=%p", sourceId.ascii().data(), sink, sinkpad);

    if (!sink || sourceId.isEmpty()) {
        LOG(MediaStream, "ERROR, No sink provided or empty source id");
        return false;
    }

    if (!sinkpad) {
        LOG(MediaStream, "No pad was given as argument, trying the element static \"sink\" pad.");
        sinkpad = gst_element_get_static_pad(sink, "sink");
        if (!sinkpad) {
            LOG(MediaStream, "ERROR, Unable to retrieve element sink pad");
            return false;
        }
    }

    GstElement* sourceElement = 0;
    GstPad* sourcePad = 0;

    bool haveSources = connectAndGetSourceElement(sourceId, &sourceElement, &sourcePad);
    if (!sourceElement) {
        LOG(MediaStream, "ERROR, Unable to get source element");
        return false;
    }

    GstElement* queue = gst_element_factory_make("queue", NULL);
    if (!queue) {
        LOG(MediaStream, "ERROR, Got no queue element");
        return false;
    }

    GstElement* sinkParent = GST_ELEMENT(gst_element_get_parent(sink));
    if (!sinkParent) {
        LOG(MediaStream, "Sink not in pipeline, adding.");
        gst_bin_add(GST_BIN(m_pipeline), sink);
    } else {
        if (sinkParent != m_pipeline) {
            gst_object_unref(queue);
            gst_object_unref(sinkParent);
            LOG(MediaStream, "ERROR, Sink already added to another element. Pipeline is now broken!");
            return false;
        }
        gst_object_unref(sinkParent);
    }

    gst_bin_add(GST_BIN(m_pipeline), queue);

    GstPad* teeSinkPad = gst_pad_get_peer(sourcePad);
    GstElement* tee = gst_pad_get_parent_element(teeSinkPad);
    gst_object_unref(teeSinkPad);

    gst_element_sync_state_with_parent(sink);
    gst_element_sync_state_with_parent(queue);

    GstPad* teeSrcPad = gst_element_get_request_pad(tee, "src%d");
    GstPad* queueSinkPad = gst_element_get_static_pad(queue, "sink");
    GstPad* queueSrcPad = gst_element_get_static_pad(queue, "src");
    gst_pad_link(teeSrcPad, queueSinkPad);
    gst_pad_link(queueSrcPad, sinkpad);
    gst_object_unref(teeSrcPad);
    gst_object_unref(queueSinkPad);
    gst_object_unref(queueSrcPad);
    gst_object_unref(tee);

    if (!haveSources) {
        m_pipelineMap.add(sourceId, Source(sourceElement, sourcePad, 0, true));
        m_sourceIdLookupMap.add(generateElementPadId(sourceElement, sourcePad), sourceId);
    }

    gst_element_set_state(m_pipeline, GST_STATE_PLAYING);

    return true;
}

bool CentralPipelineUnit::disconnectFromSource(const String& sourceId, GstElement* sink, GstPad* sinkpad)
{
    LOG(MediaStream, "Disconnecting from source with id=%s, sink=%p, sinkpad=%p", sourceId.ascii().data(), sink, sinkpad);

    if (!sink || sourceId.isEmpty()) {
        LOG(MediaStream, "ERROR, No sink provided or empty source id");
        return false;
    }

    PipelineMap::iterator sourceIt = m_pipelineMap.find(sourceId);
    if (sourceIt == m_pipelineMap.end()) {
        LOG(MediaStream, "Could not find source with id=%s", sourceId.ascii().data());
        return false;
    }

    if (!sinkpad) {
        LOG(MediaStream, "No pad was given as argument, trying the element static \"sink\" pad.");
        sinkpad = gst_element_get_static_pad(sink, "sink");
        if (!sinkpad) {
            LOG(MediaStream, "ERROR, Unable to retrieve element sink pad");
            return false;
        }
    }

    Source& source = sourceIt->second;
    GstPad* baseSourcePad = source.m_sourcePad.get();

    GstPad* lQueueSrcPad = gst_pad_get_peer(sinkpad);
    GstElement* lQueue = gst_pad_get_parent_element(lQueueSrcPad);
    gst_object_unref(lQueueSrcPad);
    GstPad* lQueueSinkPad = gst_element_get_static_pad(lQueue, "sink");
    gst_object_unref(lQueue);
    GstPad* lTeeSrcPad = gst_pad_get_peer(lQueueSinkPad);
    gst_object_unref(lQueueSinkPad);
    GstElement* lTee = gst_pad_get_parent_element(lTeeSrcPad);
    gst_object_unref(lTeeSrcPad);
    GstPad* lTeeSinkPad = gst_element_get_static_pad(lTee, "sink");
    gst_object_unref(lTee);
    GstPad* sourceSrcPad = gst_pad_get_peer(lTeeSinkPad);
    gst_object_unref(lTeeSinkPad);
    GstElement* sourceElement = gst_pad_get_parent_element(sourceSrcPad);
    gst_object_unref(sourceSrcPad);

    // disconnect pad and remove the sink if it has no more pads.
    guint numSourceTeePads = disconnectSinkFromTee(lTee, sink, sinkpad);
    if (!numSourceTeePads)
        removeSourceExtensionFromBehind(sourceElement);

    gst_object_unref(sourceElement);

    return true;
}

GstElement* CentralPipelineUnit::pipeline() const
{
    return m_pipeline;
}

guint CentralPipelineUnit::disconnectSinkFromPipeline(GstElement* sink)
{
    gst_element_set_state(sink, GST_STATE_NULL);
    gst_bin_remove(GST_BIN(m_pipeline), sink);
    GstPad* unlinkedQueueSrcPad = gst_bin_find_unlinked_pad(GST_BIN(m_pipeline), GST_PAD_SRC);

    GstElement* queue = gst_pad_get_parent_element(unlinkedQueueSrcPad);
    GstPad* queueSinkPad = gst_element_get_static_pad(queue, "sink");
    GstPad* teeSrcPad = gst_pad_get_peer(queueSinkPad);
    GstElement* tee = gst_pad_get_parent_element(teeSrcPad);

    gst_element_set_state(queue, GST_STATE_NULL);
    gst_pad_set_active(teeSrcPad, FALSE);
    gst_bin_remove(GST_BIN(m_pipeline), queue);

    gst_element_release_request_pad(tee, teeSrcPad);
    gst_object_unref(teeSrcPad);
    LOG(MediaStream, "removing stuff done");

    guint srcpads = -1;
    g_object_get(tee, "num-src-pads", &srcpads, NULL);

    gst_object_unref(queueSinkPad);
    gst_object_unref(teeSrcPad);
    gst_object_unref(tee);

    unlinkedQueueSrcPad = 0;
    unlinkedQueueSrcPad = gst_bin_find_unlinked_pad(GST_BIN(m_pipeline), GST_PAD_SRC);

    return srcpads;
}

void CentralPipelineUnit::removeSourceExtensionFromBehind(GstElement* sourceExtension)
{
    // Get right hand side elements
    GstPad* seSrcPad = gst_element_get_static_pad(sourceExtension, "src");
    GstPad* rTeeSinkPad = gst_pad_get_peer(seSrcPad);
    GstElement* rTee = gst_pad_get_parent_element(rTeeSinkPad);
    gst_object_unref(rTeeSinkPad);

    // Get left hand side elements
    GstPad* seSinkPad = gst_element_get_static_pad(sourceExtension, "sink");
    if (!seSinkPad) {
        LOG(MediaStream, "source extension did not have any sink pads. Assuming it's a source. Check if we should remove it.");

        // Need to iterate or something..
        SourceIdLookupMap::iterator sidIt = m_sourceIdLookupMap.find(generateElementPadId(sourceExtension, seSrcPad));
        if (sidIt != m_sourceIdLookupMap.end()) {
            String sourceId = sidIt->second;

            PipelineMap::iterator siIt = m_pipelineMap.find(sourceId);
            if (siIt != m_pipelineMap.end()) {
                Source sourceInfo = siIt->second;

                if (sourceInfo.m_removeWhenNotUsed) {
                    LOG(MediaStream, "Removing source %p with id=%s", sourceExtension, sourceId.ascii().data());
                    gst_element_set_state(sourceExtension, GST_STATE_NULL);
                    gst_element_set_state(rTee, GST_STATE_NULL);
                    gst_bin_remove(GST_BIN(m_pipeline), sourceExtension);
                    gst_bin_remove(GST_BIN(m_pipeline), rTee);

                    m_pipelineMap.remove(sourceId);
                    m_sourceIdLookupMap.remove(generateElementPadId(sourceExtension, sourceInfo.m_sourcePad.get()));
                }
            }
        }

        gst_object_unref(seSrcPad);
        gst_object_unref(rTee);
        return;
    }

    GstPad* lQueueSrcPad = gst_pad_get_peer(seSinkPad);
    gst_object_unref(seSinkPad);
    GstElement* lQueue = gst_pad_get_parent_element(lQueueSrcPad);
    gst_object_unref(lQueueSrcPad);
    GstPad* lQueueSinkPad = gst_element_get_static_pad(lQueue, "sink");
    gst_object_unref(lQueue);
    GstPad* lTeeSrcPad = gst_pad_get_peer(lQueueSinkPad);
    gst_object_unref(lQueueSinkPad);
    GstElement* lTee = gst_pad_get_parent_element(lTeeSrcPad);
    gst_object_unref(lTeeSrcPad);
    GstPad* lTeeSinkPad = gst_element_get_static_pad(lTee, "sink");
    gst_object_unref(lTee);
    GstPad* sourceSrcPad = gst_pad_get_peer(lTeeSinkPad);
    gst_object_unref(lTeeSinkPad);
    GstElement* sourceElement = gst_pad_get_parent_element(sourceSrcPad);
    gst_object_unref(sourceSrcPad);

    // remove source extension
    String sourceExtensionAndPadId = generateElementPadId(sourceExtension, seSrcPad);
    gst_element_set_state(rTee, GST_STATE_NULL);
    gst_bin_remove(GST_BIN(m_pipeline), rTee);
    gst_object_unref(seSrcPad);
    gst_object_unref(rTee);

    guint numSourceTeePads = disconnectSinkFromTee(lTee, sourceExtension, 0);
    LOG(MediaStream, "source-tee source pads: %d", numSourceTeePads);
    if (!numSourceTeePads) {
        LOG(MediaStream, "source-tee of source extension did not have any source pads. See if we should remove it.");
        SourceIdLookupMap::iterator sidIt = m_sourceIdLookupMap.find(generateElementPadId(sourceElement, sourceSrcPad));
        String sourceId = sidIt->second;
        PipelineMap::iterator siIt = m_pipelineMap.find(sourceId);
        Source sourceInfo = siIt->second;
        if (sourceInfo.m_removeWhenNotUsed)
            removeSourceExtensionFromBehind(sourceElement);
    }

    // since disconnectSinkFromTee was used to remove the sourceExtension
    SourceIdLookupMap::iterator sidIt = m_sourceIdLookupMap.find(sourceExtensionAndPadId);
    if (sidIt != m_sourceIdLookupMap.end()) {
        String sourceId = sidIt->second;
        LOG(MediaStream, "Removing sourceExtension with id %s", sourceId.ascii().data());
        m_pipelineMap.remove(sourceId);
        m_sourceIdLookupMap.remove(sourceExtensionAndPadId);
    }

    gst_object_unref(sourceElement);
}

gint CentralPipelineUnit::disconnectSinkFromTee(GstElement* tee, GstElement* sink, GstPad* pad)
{
    guint srcpads = -1;
    GstPad* sinkSinkPad = pad;
    bool doReleaseSinkPad = false;
    if (!pad) {
        sinkSinkPad = gst_element_get_static_pad(sink, "sink");
        doReleaseSinkPad = true;
    }
    GstPad* queueSourcePad = gst_pad_get_peer(sinkSinkPad);
    GstElement* queue = gst_pad_get_parent_element(queueSourcePad);
    GstPad* queueSinkPad = gst_element_get_static_pad(queue, "sink");
    GstPad* teeSourcePad = gst_pad_get_peer(queueSinkPad);

    gst_pad_set_active(teeSourcePad, FALSE);
    gst_bin_remove(GST_BIN(m_pipeline), queue);
    gst_element_release_request_pad(tee, teeSourcePad);

    int nPads = 0;
    gpointer item;
    GstIterator* padsIt = gst_element_iterate_sink_pads(sink);
    bool done = false;
    bool doRemoveSink = true;
    while (!done) {
        GstIteratorResult it = gst_iterator_next(padsIt, &item);
        switch (it) {
        case GST_ITERATOR_OK: {
            nPads++;
            GstPad* pad = (GstPad*)item;
            GstPad* peer = gst_pad_get_peer(pad);
            if (peer) {
                doRemoveSink = false;
                done = true;
                gst_object_unref(peer);
            }
            gst_object_unref(pad);
            break;
        }
        case GST_ITERATOR_RESYNC:
            gst_iterator_resync(padsIt);
            break;
        case GST_ITERATOR_DONE:
            done = true;
            break;
        case GST_ITERATOR_ERROR:
            LOG(MediaStream, "ERROR, Iterating!");
            done = true;
            break;
        default:
            break;
        }
    }

    String sourceAndPadId = generateElementPadId(sink, 0);
    SourceIdLookupMap::iterator sidIt = m_sourceIdLookupMap.find(generateElementPadId(sink, 0));
    if (sidIt != m_sourceIdLookupMap.end()) {
        String sourceId = sidIt->second;
        PipelineMap::iterator siIt = m_pipelineMap.find(sourceId);
        if (siIt != m_pipelineMap.end()) {
            Source sourceInfo = siIt->second;
            doRemoveSink = sourceInfo.m_removeWhenNotUsed;
        }
    }

    if (doRemoveSink) {
        gst_element_set_state(sink, GST_STATE_NULL);
        gst_bin_remove(GST_BIN(m_pipeline), sink);
        LOG(MediaStream, "Sink removed");
    } else
        LOG(MediaStream, "Did not remove the sink");

    gst_iterator_free(padsIt);

    if (doReleaseSinkPad)
        gst_object_unref(sinkSinkPad);
    gst_object_unref(queueSourcePad);
    gst_object_unref(queueSinkPad);
    gst_object_unref(teeSourcePad);

    g_object_get(tee, "num-src-pads", &srcpads, NULL);

    return srcpads;
}

void CentralPipelineUnit::removeSourceFactoryForSource(GstElement* source)
{
    gpointer item;
    GstIterator* padsIt = gst_element_iterate_src_pads(source);
    bool done = false;
    while (!done) {
    GstIteratorResult it = gst_iterator_next(padsIt, &item);
    switch (it) {
    case GST_ITERATOR_OK: {
        GstPad* srcpad = (GstPad*)item;
        String elPadId = generateElementPadId(source, srcpad);
        SourceIdLookupMap::iterator sidIt = m_sourceIdLookupMap.find(elPadId);
        if (sidIt == m_sourceIdLookupMap.end())
            continue;

        String sourceId = sidIt->second;

        PipelineMap::iterator siIt = m_pipelineMap.find(sourceId);
        if (siIt == m_pipelineMap.end())
            continue;

        Source& sourceInfo = siIt->second;
        sourceInfo.m_sourceFactory = 0;
        gst_object_unref(srcpad);
        break;
        }
    case GST_ITERATOR_RESYNC:
        gst_iterator_resync(padsIt);
        break;
    case GST_ITERATOR_DONE:
        done = true;
        break;
    case GST_ITERATOR_ERROR:
        LOG(MediaStream, "ERROR, Iterating!");
        done = true;
        break;
    default:
        break;
    }
    }

    String elPadId = generateElementPadId(source, 0);
    SourceIdLookupMap::iterator sidIt = m_sourceIdLookupMap.find(elPadId);
    if (sidIt == m_sourceIdLookupMap.end())
        return;

    String sourceId = sidIt->second;

    PipelineMap::iterator siIt = m_pipelineMap.find(sourceId);
    if (siIt == m_pipelineMap.end())
        return;

    Source& sourceInfo = siIt->second;
    sourceInfo.m_sourceFactory = 0;
}

void CentralPipelineUnit::disconnectSourceFromPipeline(GstElement* source, GstPad* sourcePad)
{
    SourceIdLookupMap::iterator sidIt = m_sourceIdLookupMap.find(generateElementPadId(source, sourcePad));
    if (sidIt == m_sourceIdLookupMap.end())
        return;

    String sourceId = sidIt->second;

    PipelineMap::iterator siIt = m_pipelineMap.find(sourceId);
    if (siIt == m_pipelineMap.end())
        return;

    Source sourceInfo = siIt->second;

    // FIXME: do block properly async
    gst_pad_set_blocked_async(sourcePad, TRUE, 0, 0);
    GstPad* teeSinkPad = gst_pad_get_peer(sourcePad);
    GstElement* tee = gst_pad_get_parent_element(teeSinkPad);
    gst_object_unref(teeSinkPad);

    gpointer item;
    GstIterator* padsIt = gst_element_iterate_src_pads(tee);
    bool done = false;
    while (!done) {
        GstIteratorResult it = gst_iterator_next(padsIt, &item);
        switch (it) {
        case GST_ITERATOR_OK: {
            GstPad* teeSrcPad = (GstPad*)item;
            GstPad* queueSinkPad = gst_pad_get_peer(teeSrcPad);
            GstElement* queue = gst_pad_get_parent_element(queueSinkPad);
            // queue might actually be a fakesink
            gchar* qName = gst_element_get_name(queue);
            if (String(qName).startsWith("fakesink")) {
                gst_element_set_state(queue, GST_STATE_NULL);
                gst_bin_remove(GST_BIN(m_pipeline), queue);
                gst_element_release_request_pad(tee, teeSrcPad);
                gst_object_unref(teeSrcPad);
                gst_object_unref(queueSinkPad);
                gst_object_unref(queue);
                break;
            }

            GstPad* queueSrcPad = gst_element_get_static_pad(queue, "src");
            GstPad* sinkSinkPad = gst_pad_get_peer(queueSrcPad);
            GstElement* sink = gst_pad_get_parent_element(sinkSinkPad);
            GstElement* parent = (GstElement*)gst_element_get_parent(sink);
            gchar* parentName = gst_element_get_name(parent);
            GstElement* sinkBin = 0;
            if (!strcmp(parentName, "mediastream_pipeline"))
                sinkBin = sink;
            else
                sinkBin = parent;
            gchar* sinkBinName = gst_element_get_name(sinkBin);
            g_free(sinkBinName);

            GstPad* sinkSrcPad = gst_element_get_static_pad(sinkBin, "src");
            SourceIdLookupMap::iterator sidIt = m_sourceIdLookupMap.find(generateElementPadId(sinkBin, sinkSrcPad));
            gst_object_unref(sinkSrcPad);
            if (sidIt != m_sourceIdLookupMap.end()) {
                LOG(MediaStream, "Sink element was a source extension. Recursively calling disconnectSourceFromPipeline to remove it");
                GstPad* srcpad = gst_element_get_static_pad(sinkBin, "src");
                disconnectSourceFromPipeline(sinkBin, srcpad);
            } else
                disconnectSinkFromPipeline(sinkBin);

            g_free(parentName);
            gst_object_unref(teeSrcPad);
            gst_object_unref(queueSinkPad);
            gst_object_unref(queueSrcPad);
            gst_object_unref(sinkSinkPad);
            gst_object_unref(queue);
            gst_object_unref(sink);
            gst_object_unref(parent);
            break;
        }
        case GST_ITERATOR_RESYNC:
            gst_iterator_resync(padsIt);
            break;
        case GST_ITERATOR_DONE:
            done = true;
            break;
        case GST_ITERATOR_ERROR:
            LOG(MediaStream, "ERROR, Iterating!");
            done = true;
            break;
        default:
            break;
        }
    }

    gst_pad_set_blocked_async(sourcePad, FALSE, NULL, 0);

    // Since nothing is now connected to this source, we remove it and earlier sources that does not have connections
    GstPad* sourceSinkPad = gst_element_get_static_pad(source, "sink");
    if (sourceSinkPad) {
        // source is a source extension
        removeSourceExtensionFromBehind(source);
        gst_object_unref(tee);
    } else {
        // if not a source extension, there will be a loose tee that we need to remove
        gst_element_set_state(tee, GST_STATE_NULL);
        gst_bin_remove(GST_BIN(m_pipeline), tee);
    }

    gst_object_unref(sourcePad);
}

void CentralPipelineUnit::disconnectSourceWithMultiplePadsFromPipeline(GstElement* source)
{
    String elementPadId = generateElementPadId(source, 0);
    SourceIdLookupMap::iterator sidIt = m_sourceIdLookupMap.find(elementPadId);
    if (sidIt == m_sourceIdLookupMap.end())
        return;

    String sourceId = sidIt->second;

    gpointer item;
    GstIterator* padsIt = gst_element_iterate_src_pads(source);
    bool done = false;
    while (!done) {
    GstIteratorResult it = gst_iterator_next(padsIt, &item);
    switch (it) {
    case GST_ITERATOR_OK: {
        GstPad* srcpad = (GstPad*)item;
        GstPad* peerPad = gst_pad_get_peer(srcpad);
        if (!peerPad)
            continue;
        gst_object_unref(peerPad);

        disconnectSourceFromPipeline(source, srcpad);

        // disconnectSourceFromPipeline does not remove the tee
        GstPad* teeSinkPad = gst_pad_get_peer(srcpad);
        GstElement* tee = gst_pad_get_parent_element(teeSinkPad);
        gst_element_set_state(tee, GST_STATE_NULL);
        gst_bin_remove(GST_BIN(m_pipeline), tee);

        gst_object_unref(srcpad);
        break;
    }
    case GST_ITERATOR_RESYNC:
        gst_iterator_resync(padsIt);
        break;
    case GST_ITERATOR_DONE:
        done = true;
        break;
    case GST_ITERATOR_ERROR:
        LOG(MediaStream, "ERROR, Iterating!");
        done = true;
        break;
    default:
        break;
    }
    }

    gst_element_set_state(source, GST_STATE_NULL);
    gst_bin_remove(GST_BIN(m_pipeline), source);

    m_pipelineMap.remove(sourceId);
    m_sourceIdLookupMap.remove(elementPadId);
}

static gboolean messageCallback(GstBus* bus, GstMessage* message, gpointer data)
{
    GError* err = 0;
    gchar* debug = 0;

    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_ERROR: {
        gst_message_parse_error(message, &err, &debug);
        LOG(MediaStream, "ERROR, media error: %d, %s", err->code, err->message);
        GstElement* source = (GstElement*) GST_MESSAGE_SRC(message);
        GstElement* parent = (GstElement*) gst_element_get_parent(source);

        /* Error handling code that doesnt really work
         * FIXME: Make this work..
        gchar* pname = gst_element_get_name(parent);

        if (strcmp(pname, "mediastream_pipeline") == 0) {
            disconnectSourceFromPipeline(pipeline, source);
            cpui->removeSourceFromPipelineMap(source); // todo: make this call a part of disconnectSourceFromPipeline
        }
        else {
            disconnectSourceFromPipeline(pipeline, parent);
            cpui->removeSourceFromPipelineMap(parent);
        }
        if (pname)
            g_free(pname);
        */

        // FIXME: Do this on all errors? Check error code?
        // LOG(MediaStream, "Trying to disconnect source from pipeline...");
        // disconnectSourceFromPipeline(pipeline, source);

        gst_object_unref(parent);
        break;
    }
    default:
        break;
    }

    if (err)
        g_error_free(err);
    if (debug)
        g_free(debug);

    return true;
}

static String generateElementPadId(GstElement* element, GstPad* pad)
{
    String id(String::number(gulong(element)));
    id += "_";
    id += String::number(gulong(pad));
    return id;
}

}

#endif // ENABLE(MEDIA_STREAM)
