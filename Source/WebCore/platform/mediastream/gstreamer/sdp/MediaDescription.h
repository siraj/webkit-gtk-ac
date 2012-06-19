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


#ifndef MediaDescription_h
#define MediaDescription_h

#if ENABLE(MEDIA_STREAM)

#include "Candidate.h"
#include "Crypto.h"
#include "Payload.h"
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>


namespace WebCore {


class MediaDescription {
    friend MediaDescription* createMediaDescription(const String& mRow);
public:

    enum Direction {
        Unknown,
        SendOnly,
        RecvOnly,
        SendRecv
    };

    enum MediaType {
        Audio,
        Video,
        Text,
        NotSpecified
    };

    enum MediaTransportType {
        UDP,
        RTP_AVP,
        RTP_AVPF
    };

    enum RtcpProfileSpecificParameter {
        ACK,
        NACK,
        FIR
    };

    static const char* RtcpProfileSpecificParameterString[3];


    MediaDescription()
    :   m_direction(Unknown)
    ,   m_label("")
    ,   m_payloads()
    ,   m_candidates()
    ,   m_cryptos()
    ,   m_mediaType(NotSpecified)
    ,   m_port(0)
    ,   m_mediaTransportType(UDP)
    ,   m_connectionType("")
    ,   m_connectionAddress("")
    ,   m_username("")
    ,   m_password("")
    ,   m_maximumBandwidth(0)
    { };
    ~MediaDescription() { };

    Direction direction() const { return m_direction; }
    void setDirection(Direction direction) { m_direction = direction; }

    String label() const { return m_label; }
    void setLabel(const String& label) { m_label = label;}

    MediaType mediaType() const { return m_mediaType; }
    void setMediaType(MediaType mediaType) { m_mediaType = mediaType; }

    unsigned int port() const { return m_port; }
    void setPort(unsigned int port) { m_port = port; }

    MediaTransportType mediaTransportType() const { return m_mediaTransportType; }
    void setMediaTransportType(MediaTransportType mediaTransportType) { m_mediaTransportType = mediaTransportType; }

    Vector<Payload*>& payloads() { return m_payloads; }
    Vector<Candidate*>& candidates() { return m_candidates; }
    Vector<Crypto*>& cryptos() { return m_cryptos; }

    String connectionAddress() const { return m_connectionAddress; }
    void setConnectionAddress(String& connectionAddress) {m_connectionAddress = connectionAddress; }

    String username() const { return m_username; }
    void setUsername(const String& username) { m_username = username; }

    String password() const { return m_password; }
    void setPassword(const String& password) { m_password = password; }

    // FIXME: Could add this for each payload as well.
    void addRtcpProfileSpecificParameter(RtcpProfileSpecificParameter);
    const Vector<RtcpProfileSpecificParameter> rtcpProfileSpecificParameters() const { return m_rtcpProfileSpecificParameters; }

    void setMaximumBandwidth(unsigned int maximumBandwidth) { m_maximumBandwidth = maximumBandwidth; }
    unsigned int maximumBandwidth() const { return m_maximumBandwidth; }


private:
    Direction m_direction;
    String m_label;

    Vector<Payload*> m_payloads;
    Vector<Candidate*> m_candidates;
    Vector<Crypto*> m_cryptos;

    MediaType m_mediaType;
    unsigned short m_port;
    MediaTransportType m_mediaTransportType;

    String m_connectionType;
    String m_connectionAddress;

    String m_username;
    String m_password;

    Vector<RtcpProfileSpecificParameter> m_rtcpProfileSpecificParameters;

    unsigned int m_maximumBandwidth;
};

MediaDescription* createMediaDescription(const String& mLine);
bool addAttribute(MediaDescription*, const String& aLine);
bool setConnection(MediaDescription*, const String& aLine);
bool setMediaBandwidth(MediaDescription*, const String& aLine);

String formatMediaDescription(MediaDescription*);

const char* getAddressType(const char* ip);

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // MediaDescription_h
