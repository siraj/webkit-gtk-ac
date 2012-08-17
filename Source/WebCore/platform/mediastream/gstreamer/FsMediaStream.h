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


#ifndef FsMediaStream_h
#define FsMediaStream_h

#if ENABLE(MEDIA_STREAM)

#include "../sdp/Candidate.h"
#include "../sdp/Payload.h"

#include "MediaStreamDescriptor.h"
#include <farstream/fs-conference.h>
#include <farstream/fs-session.h>
#include <farstream/fs-stream.h>

#include <wtf/HashMap.h>
#include <wtf/gobject/GRefPtr.h>
#include <wtf/text/WTFString.h>


namespace WebCore {

class FsMediaStream;
class PeerConnectionHandlerConfiguration;
class MediaDescription;

class FsMediaStreamComponent {
    friend class FsMediaStream;

private:
    FsMediaStreamComponent(FsMediaStream*, FsMediaType);
public:
    ~FsMediaStreamComponent();
    // XXX: return FsSession type instead ?
    FsMediaType type() const {return m_type;}
    // XXX: return FsStream direction instead ?
    FsStreamDirection direction() const {return m_direction;}

    FsMediaStream* fsMediaStream() const {return m_fsMediaStream;}

    bool localCandidatesPrepared() const;
    Vector<Candidate*> localCandidates() const;
    void clearLocalCandidates();
    bool getLocalParams(String& outIp, unsigned int& outPort, String& outUsername, String& outPassword) const;
    Vector<Candidate*> activeCandidates() const;
    void clearActiveCandidates();

    void addRemoteCandidates(const Vector<Candidate*>& candidates, const String& username, const String& password);
    void setRemoteCodecs(const Vector<Payload*>&);

    void onSrcPadAdded(GstPad*, FsCodec*);
    void setSourceId(const String&);
    String sourceId() const;
    GstPad* sourcePad() const;
    GstElement* sourceElement() const;
    GstPad* sinkPad() const { return m_sinkPad.get(); }
    GstElement* sinkElement() const { return m_sinkElement.get(); }

    bool setServerConfiguration(const PeerConnectionHandlerConfiguration*, bool initiator);

    Vector<Payload*> offerCodecs() const;
    Vector<Payload*> responseCodecs() const;
    bool isConnectionReady() const;
    MediaDescription* createMediaDescription();
    void onInsertedIntoPipeline();
    bool isInsertedIntoPipeline() const;
    void onBufferProbeCalled();
    bool hasReceivedData() const;

private:
    static gboolean onBusMessage(GstBus*, GstMessage*, gpointer);

    void onNewLocalCandidate(FsCandidate*);
    void onLocalCandidatesPrepared();
    void onNewActiveCandidatePair(FsCandidate* localCandidate, FsCandidate* remoteCandidate);
    void onReceiveCodecsChanged(GList* codecs);
    void onComponentStateChanged(guint component, FsStreamState);
    void onSendCodecChanged(FsCodec*, GList* secondaryCodecs);
    void onCodecsChanged();

private:
    GRefPtr<FsSession> m_session;
    GRefPtr<FsStream> m_stream;
    GRefPtr<GstPad> m_sinkPad;
    GRefPtr<GstElement> m_sinkElement;
    GRefPtr<GstPad> m_srcPad;

    FsMediaType m_type;
    FsStreamDirection m_direction;
    FsMediaStream* m_fsMediaStream;
    GRefPtr<GstElement> m_pipeline;
    GstBus *m_bus;
    Vector<FsCandidate*> m_localCandidates;
    bool m_localCandidatesPrepared;
    Vector<FsCandidate*> m_activeCandidates;
    String m_sourceId;
    HashMap<guint, FsStreamState> m_componentState;
    GRefPtr<GstElement> m_valve;
    GRefPtr<GstPad> m_valveSrc;
    bool m_insertedIntoPipeline;
    bool m_hasReceivedData;
    bool m_bufferProbeHandlerId;
    GList *m_codecs;
};

class FsMediaStreamClient {
public:
    virtual ~FsMediaStreamClient() { }
    virtual void onStreamReceivedData(FsMediaStream*) = 0;
    virtual void onStreamConnectionReady(FsMediaStream*) = 0;
    virtual void onLocalCandidatesPrepared(FsMediaStream*) = 0;
    virtual void onSrcPadAdded(FsMediaStreamComponent*, GstPad*) = 0;
    virtual void codecsNeedResend(FsMediaStreamComponent*) = 0;
};


class FsMediaStream {
public:
    typedef enum {Local, Remote} StreamType;

    FsMediaStream(FsMediaStreamClient* client, FsConference* fsConference, FsParticipant* fsParticipant, StreamType type, String label)
        : m_client(client)
        , m_fsConference(fsConference)
        , m_fsParticipant(fsParticipant)
        , m_type(type)
        , m_label(label)
        , m_closed(false)
        { };
    ~FsMediaStream() { };

    FsMediaStreamClient* client() { return m_client; }

    StreamType type() const {return m_type;}
    // XXX: needed ? also in MediaStreamDescriptor
    String label() const {return m_label;}

    // XXX: needed ? also in MediaStreamDescriptor
    bool closed() const {return m_closed;}
    // FIXME: trigger some action as well?
    void setClosed() {m_closed = true;}

    FsMediaStreamComponent* addComponent(FsMediaType);
    const Vector<FsMediaStreamComponent*>& components() const {return m_components;}
    void onComponentConnectionReady(FsMediaStreamComponent*);
    bool isConnectionReady() const;
    void onLocalCandidatesPrepared(FsMediaStreamComponent*);
    void onComponentReceivedData(FsMediaStreamComponent*);
    bool hasReceivedData() const;

    FsConference* conference() { return m_fsConference.get();}
    FsParticipant* participant() { return m_fsParticipant.get();}

    bool localCandidatesPrepared() const;

    bool componentSourceInserted(FsMediaStreamComponent*);

    void setStreamDescriptor(PassRefPtr<MediaStreamDescriptor> descriptor) { m_streamDescriptor = descriptor; }
    MediaStreamDescriptor* streamDescriptor() { return m_streamDescriptor.get(); }

private:
    bool allComponentsInserted() const;

    FsMediaStreamClient* m_client;
    GRefPtr<FsConference> m_fsConference;
    GRefPtr<FsParticipant> m_fsParticipant;
    StreamType m_type;
    // XXX: needed ? also in MediaStreamDescriptor
    String m_label;
    // XXX: needed ? also in MediaStreamDescriptor
    bool m_closed;
    RefPtr<MediaStreamDescriptor> m_streamDescriptor;
    Vector<FsMediaStreamComponent*> m_components;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // FsMediaStream_h
