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


#ifndef MediaStreamCenterPrivateGStreamer_h
#define MediaStreamCenterPrivateGStreamer_h

#if ENABLE(MEDIA_STREAM)

#include "MediaStreamSource.h"
#include "SourceFactory.h"

#include <gst/gst.h>
#include <wtf/HashMap.h>
#include <wtf/gobject/GOwnPtr.h>
#include <wtf/gobject/GRefPtr.h>
#include <wtf/text/WTFString.h>


namespace WebCore {

class MediaStreamCenter;
class MediaStreamDescriptor;
class MediaStreamComponent;
class MediaStreamSourcesQueryClient;

class MediaStreamCenterPrivateGStreamer : private SourceFactory {
private:
    typedef struct {
        String m_factoryKey;
        MediaStreamSource::Type m_type;
        String m_device;
    } SourceInfo;

    typedef HashMap<String, SourceInfo> SourceInfoMap;
    typedef HashMap<String, GRefPtr<GstElementFactory> > ElementFactoryMap;

public:
    static PassOwnPtr<MediaStreamCenterPrivateGStreamer> create()
    {
        return adoptPtr(new MediaStreamCenterPrivateGStreamer());
    }

    void queryMediaStreamSources(PassRefPtr<MediaStreamSourcesQueryClient>);

    void didSetMediaStreamTrackEnabled(MediaStreamDescriptor*, MediaStreamComponent*);
    void didStopLocalMediaStream(MediaStreamDescriptor*);

private:
    MediaStreamCenterPrivateGStreamer()
    {
    }

    void registerSourceFactories(const MediaStreamSourceVector&);
    String storeElementFactoryForElement(GstElement* source);
    GstElementFactory *storedElementFactory(const String& key);
    String storeSourceInfo(MediaStreamSource::Type, const String& factoryKey, const String& device);

    // SourceFactory
    GstElement* createSource(const String& sourceId, GstPad** srcPad);

    ElementFactoryMap m_elementFactoryMap;
    SourceInfoMap m_sourceInfoMap;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // MediaStreamCenterPrivateGStreamer_h
