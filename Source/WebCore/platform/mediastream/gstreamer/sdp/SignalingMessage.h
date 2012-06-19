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


#ifndef SignalingMessage_h
#define SignalingMessage_h

#if ENABLE(MEDIA_STREAM)

#include "MediaDescription.h"

#include <wtf/Vector.h>

namespace WebCore {

class SignalingMessage {
    friend SignalingMessage* createSignalingMessage(const String& sdp);
public:

    SignalingMessage(long sessionId = 0, long sessionVersion = 0);

    // FIXME: delete all MediaDescription objects
    ~SignalingMessage() { };

    Vector<String> labels();
    Vector<MediaDescription*> mediaDescriptions(String label);
    Vector<MediaDescription*>& mediaDescriptions() { return m_mediaDescriptions; };

    bool setOrigin(const String& line);

    long sessionId() const { return m_sessionId; }
    long sessionVersion() const { return m_sessionVersion; }
    const String networkType() const { return m_networkType; }
    const String username() const { return m_username; }
    void address(String& addrType, String& address) const;

private:
    Vector<MediaDescription*> m_mediaDescriptions;
    String m_username;
    int64_t m_sessionId;
    int64_t m_sessionVersion;
    const String m_networkType;
    String m_addrType;
    String m_address;

    // TODO: Connection stuff here as well?

};

SignalingMessage* createSignalingMessage(const String& sdp);
String formatSdp(SignalingMessage*);

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // SignalingMessage_h
