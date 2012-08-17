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


#ifndef Candidate_h
#define Candidate_h

#if ENABLE(MEDIA_STREAM)

#include <wtf/text/WTFString.h>

namespace WebCore {

class Candidate {
public:
    enum CandidateType {
        ServerReflexive,
        PeerReflexive,
        Relayed,
        Host,
        Multicast
    };

    enum Protocol {
        UDP,
        TCP
    };

    Candidate();
    ~Candidate() { };

    String foundation() const { return m_foundation; }
    void setFoundation(const String& foundation) { m_foundation = foundation; }

    unsigned int componentId() const { return m_componentId; }
    void setComponentId(unsigned int componentId) { m_componentId = componentId; }

    CandidateType candidateType() const { return m_candidateType; }
    void setCandidateType(CandidateType candidateType) { m_candidateType = candidateType; }

    String address() const { return m_address; }
    void setAddress(const String& address) { m_address = address; }

    unsigned short port() const { return m_port; }
    void setPort(unsigned short port) { m_port = port; }

    unsigned int priority() const { return m_prio; }
    void setPriority(unsigned int prio) { m_prio = prio; }

    bool relatedAddress() const { return m_relAddress; }
    void setRelatedAddress(bool relatedAddress) { m_relAddress = relatedAddress; }

    bool relatedPort() const { return m_relPort; }
    void setRelatedPort(bool relatedPort) { m_relPort = relatedPort; }

    Protocol protocol() const { return m_protocol; }
    void setProtocol(Protocol protocol) { m_protocol = protocol; }

private:
    String m_foundation;
    unsigned int m_componentId;
    CandidateType m_candidateType;
    String m_address;
    unsigned short m_port;
    unsigned int m_prio;
    bool m_relAddress;
    bool m_relPort;
    Protocol m_protocol;
};

Candidate* createCandidate(const String& candidateAttribute);

String formatCandidate(Candidate*);


} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // Candidate_h
