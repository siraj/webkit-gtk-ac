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

#ifndef PeerConnectionHandlerPrivateFarstream_h
#define PeerConnectionHandlerPrivateFarstream_h

#if ENABLE(MEDIA_STREAM)

#include "CentralPipelineUnit.h"
#include "FsMediaStream.h"
#include "GRefPtrGStreamer.h"
#include "MediaStreamDescriptor.h"
#include "PeerConnectionHandlerConfiguration.h"

#include <wtf/HashMap.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/gobject/GOwnPtr.h>
#include <wtf/gobject/GRefPtr.h>

typedef struct _FsCandidate FsCandidate;
typedef struct _FsCodec FsCodec;
typedef struct _FsConference FsConference;
typedef struct _FsParticipant FsParticipant;
typedef struct _FsStream FsStream;
typedef struct _GstBus GstBus;
typedef struct _GstMessage GstMessage;
typedef struct _GstPad GstPad;

namespace WebCore {

class MediaDescription;
class DeprecatedPeerConnectionHandlerClient;
class SecurityOrigin;
class SignalingMessage;

enum IceState {
    ICE_STATE_NEW,
    ICE_STATE_GATHERING,
    ICE_STATE_WAITING,
    ICE_STATE_CHECKING,
    ICE_STATE_CONNECTED,
    ICE_STATE_COMPLETED,
    ICE_STATE_FAILED,
    ICE_STATE_CLOSED
};

enum SDPState {
    SDP_STATE_NEW,
    SDP_STATE_IDLE,
    SDP_STATE_WAITING,
    SDP_STATE_PENDING_ANSWER,
    SDP_STATE_GLARE
};

class PeerConnectionHandlerPrivateFarstream : public FsMediaStreamClient {
    WTF_MAKE_NONCOPYABLE(PeerConnectionHandlerPrivateFarstream);
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<PeerConnectionHandlerPrivateFarstream> create(DeprecatedPeerConnectionHandlerClient*, const String& serverConfiguration, const String& username);
    ~PeerConnectionHandlerPrivateFarstream();

    void produceInitialOffer(const MediaStreamDescriptorVector& pendingAddStreams);
    void handleInitialOffer(const String& sdp);
    void processSDP(const String& sdp);
    void processPendingStreams(const MediaStreamDescriptorVector& pendingAddStreams, const MediaStreamDescriptorVector& pendingRemoveStreams);
    void sendDataStreamMessage(const char* data, size_t length);
    void sourceInserted(GstElement* source, GstPad*);

    void stop();

private:
    friend class FsMediaStreamComponent;

    typedef HashMap<String, FsMediaStream*> FsMediaStreamMap;

    PeerConnectionHandlerPrivateFarstream(DeprecatedPeerConnectionHandlerClient*, const String& serverConfiguration, const String& username);

    void addStreams(const MediaStreamDescriptorVector& streams);
    void removeStreams(const MediaStreamDescriptorVector& streams);
    void removeRequestedStreams();
    void shutdownAndRemoveMediaStream(FsMediaStream*, bool fromClose);
    void addMediaDescriptionToSignalingMessage(SignalingMessage*, FsMediaStream*);

    void newLocalCandidate(GRefPtr<FsStream>, FsCandidate*);
    void generateSDP(FsStream* fstream);

    void onStreamConnectionReady(FsMediaStream*);
    void onStreamReceivedData(FsMediaStream*);
    void onLocalCandidatesPrepared(FsMediaStream*);
    void onSrcPadAdded(FsMediaStreamComponent*, GstPad*);
    void codecsNeedResend(FsMediaStreamComponent*);
    // static gboolean onBusMessage(GstBus*, GstMessage*, gpointer);

    SignalingMessage* createSignalingMessageOffer();

    void processStreamMediaDescriptionOffer(Vector<MediaDescription*>&);
    void processStreamMediaDescriptionAnswer(Vector<MediaDescription*>&);

    void changeIceState(IceState newState);
    void changeSDPState(SDPState newState);
    void sendMessage();
    void scheduleOffer();

    RefPtr<PeerConnectionHandlerConfiguration> m_configuration;
    DeprecatedPeerConnectionHandlerClient* m_client;
    GRefPtr<GstElement> m_pipeline;
    GstBus *m_bus;
    GRefPtr<FsConference> m_conference;
    GRefPtr<FsParticipant> m_participant;
    // for looking up by label
    FsMediaStreamMap m_mediaStreamMap;
    // an ordered list to ensure stable ordering in SDP
    Vector<FsMediaStream*> m_sessionStreams;
    // streams that are not part of the current negotiation, but will be added
    // at the next opportunity
    Vector<FsMediaStream*> m_streamsToAdd;
    // streams that *are* part of the current negotiation, but will be removed
    // at the next opportunity
    Vector<FsMediaStream*> m_streamsToRemove;
    // whether this peer is the initiator of this connection
    bool m_initiator;
    const long m_sessionId;
    long m_sessionVersion;
    IceState m_iceState;
    SDPState m_sdpState;
    bool m_needResendOffer;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // PeerConnectionHandlerPrivateFarstream_h
