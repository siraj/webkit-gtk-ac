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

#ifndef PeerConnectionHandlerConfiguration_h
#define PeerConnectionHandlerConfiguration_h

#if ENABLE(MEDIA_STREAM)

#include <wtf/PassOwnPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

struct PeerConnectionHandlerConfiguration : public RefCounted<PeerConnectionHandlerConfiguration> {
public:
    enum Type {
        TypeNONE,
        TypeSTUN,
        TypeTURN
    };

    static PassRefPtr<PeerConnectionHandlerConfiguration> create()
    {
        return adoptRef(new PeerConnectionHandlerConfiguration());
    }

    static PassRefPtr<PeerConnectionHandlerConfiguration> create(const String& serverConfiguration, const String& username);

    Type type;
    bool secure;
    String host;
    int port;
    String username;
    String password;

private:
    PeerConnectionHandlerConfiguration()
        : type(TypeNONE)
        , secure(false)
        , host("")
        , port(-1)
        , username("")
        , password("")
    { }
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // PeerConnectionHandlerConfiguration_h
