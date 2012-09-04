/*
 * Copyright (C) 2012 Intel Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */


#include "config.h"

#if ENABLE(MEDIA_STREAM)

#include "UserMediaClientGtk.h"

#include "webkitwebusermedialistprivate.h"
#include "webkitwebusermediarequestprivate.h"
#include "webkitwebviewprivate.h"

using namespace WebCore;

namespace WebKit {

UserMediaClientGtk::UserMediaClientGtk(WebKitWebView *webView)
    : m_webView(webView)
{
}

UserMediaClientGtk::~UserMediaClientGtk()
{
}

void UserMediaClientGtk::pageDestroyed()
{
    // TODO: we should do some clean up here.
}

void UserMediaClientGtk::requestUserMedia(WTF::PassRefPtr<UserMediaRequest> prpRequest, const WebCore::MediaStreamSourceVector& audioSources, const WebCore::MediaStreamSourceVector& videoSources)
{
    RefPtr<UserMediaRequest> request = prpRequest;
    m_requestSet.add(request);

    g_signal_emit_by_name(m_webView, "user-media-requested", kitNew(request.get()), kitNew(audioSources), kitNew(videoSources));
}

void UserMediaClientGtk::cancelUserMediaRequest(UserMediaRequest* request)
{
    ASSERT(m_requestSet.contains(request));
    g_signal_emit_by_name(m_webView, "user-media-request-cancelled", 0);
}

void UserMediaClientGtk::userMediaRequestSucceeded(UserMediaRequest* request, const MediaStreamSourceVector& audioSources, const MediaStreamSourceVector& videoSources)
{
    request->succeed(audioSources, videoSources);
    m_requestSet.remove(request);
}

void UserMediaClientGtk::userMediaRequestFailed(UserMediaRequest* request)
{
    request->fail();
    m_requestSet.remove(request);
}

}

#endif // ENABLE(MEDIA_STREAM)
