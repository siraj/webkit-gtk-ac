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

#include "webkitwebusermediarequest.h"

#include "Frame.h"
#include "SecurityOrigin.h"
#include "UserMediaRequest.h"
#include "webkitglobalsprivate.h"
#include "webkitwebusermediarequestprivate.h"
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <wtf/text/CString.h>

/**
 * SECTION:webkitwebusermediarequest
 * @short_description: The WebKit's UserMedia Request.
 * @Title: WebKitWebUserMediaRequest
 *
 * This object contains the information of a media request.
 * It can be used to check if the request is asking for audio and/or video.
 *
 * Since: 1.10.0
 */

using namespace WebKit;

struct _WebKitWebUserMediaRequestPrivate {
    RefPtr<WebCore::UserMediaRequest> userMediaRequest;
};

G_DEFINE_TYPE(WebKitWebUserMediaRequest, webkit_web_user_media_request, G_TYPE_OBJECT);

static void webkit_web_user_media_request_class_init(WebKitWebUserMediaRequestClass* requestClass)
{
    webkitInit();

    g_type_class_add_private(requestClass, sizeof(WebKitWebUserMediaRequestPrivate));
}

static void webkit_web_user_media_request_init(WebKitWebUserMediaRequest* request)
{
    WebKitWebUserMediaRequestPrivate* priv = G_TYPE_INSTANCE_GET_PRIVATE(request, WEBKIT_TYPE_WEB_USER_MEDIA_REQUEST, WebKitWebUserMediaRequestPrivate);
    request->priv = priv;
}

/**
 * webkit_web_user_media_request_wants_audio:
 * @request: the #WebKitUserMediaRequest itself.
 *
 * Indicates the content of an UserMediaRequest.
 *
 * Return value: True if the request contains an audio request.
 *
 * Since: 1.10.0
 */
gboolean webkit_web_user_media_request_wants_audio(WebKitWebUserMediaRequest* request)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_USER_MEDIA_REQUEST(request), FALSE);

    return core(request)->audio();
}

/**
 * webkit_web_user_media_request_wants_video:
 * @request: the #WebKitUserMediaRequest itself.
 *
 * Indicates the content of an UserMediaRequest.
 *
 * Return value: True if the request contains a video request.
 *
 * Since: 1.10.0
 */
gboolean webkit_web_user_media_request_wants_video(WebKitWebUserMediaRequest* request)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_USER_MEDIA_REQUEST(request), FALSE);

    return core(request)->video();
}

/**
 * webkit_web_user_media_request_get_origin:
 * @request: the #WebKitUserMediaRequest itself.
 *
 * The origin of a media request.
 *
 * Return value: string with the origin of a media request.
 *
 * Since: 1.10.0
 */
const gchar* webkit_web_user_media_request_get_origin(WebKitWebUserMediaRequest* request)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_USER_MEDIA_REQUEST(request), NULL);

    return core(request)->scriptExecutionContext()->securityOrigin()->toString().utf8().data();
}

namespace WebKit {

WebCore::UserMediaRequest* core(WebKitWebUserMediaRequest* request)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_USER_MEDIA_REQUEST(request), NULL);

    return request->priv->userMediaRequest.get();
}

WebKitWebUserMediaRequest* kitNew(WebCore::UserMediaRequest* request)
{
    g_return_val_if_fail(request, NULL);

    WebKitWebUserMediaRequest* webRequest = WEBKIT_WEB_USER_MEDIA_REQUEST(g_object_new(WEBKIT_TYPE_WEB_USER_MEDIA_REQUEST, NULL));
    webRequest->priv->userMediaRequest = request;

    return webRequest;
}

}

#endif /* ENABLE(MEDIA_STREAM) */
