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
#include "webkitwebusermediarequest.h"

#if ENABLE(MEDIA_STREAM)

#include "Frame.h"
#include "SecurityOrigin.h"
#include "UserMediaRequest.h"
#include "webkitglobalsprivate.h"
#include "webkitsecurityoriginprivate.h"
#include "webkitwebusermediarequestprivate.h"
#include <wtf/text/CString.h>

/**
 * SECTION:webkitwebusermediarequest
 * @short_description: The WebKit's UserMedia Request.
 * @Title: WebKitWebUserMediaRequest
 *
 * This object contains the information of a media request.
 * It can be used to check if the request is asking for audio and/or video.
 *
 * Since: 2.0.0
 */

using namespace WebKit;

struct _WebKitWebUserMediaRequestPrivate {
    RefPtr<WebCore::UserMediaRequest> userMediaRequest;
};

G_DEFINE_TYPE(WebKitWebUserMediaRequest, webkit_web_user_media_request, G_TYPE_OBJECT)

static void webkitWebUserMediaRequestFinalize(GObject *object)
{
    WEBKIT_WEB_USER_MEDIA_REQUEST(object)->priv->~WebKitWebUserMediaRequestPrivate();

    G_OBJECT_CLASS(webkit_web_user_media_request_parent_class)->finalize(object);
}

static void webkit_web_user_media_request_class_init(WebKitWebUserMediaRequestClass* requestClass)
{
    GObjectClass* gobject_class = G_OBJECT_CLASS(requestClass);
    gobject_class->finalize = webkitWebUserMediaRequestFinalize;

    webkitInit();

    g_type_class_add_private(requestClass, sizeof(WebKitWebUserMediaRequestPrivate));
}

static void webkit_web_user_media_request_init(WebKitWebUserMediaRequest* request)
{
    WebKitWebUserMediaRequestPrivate* priv = G_TYPE_INSTANCE_GET_PRIVATE(request, WEBKIT_TYPE_WEB_USER_MEDIA_REQUEST, WebKitWebUserMediaRequestPrivate);
    request->priv = priv;

    new (priv) WebKitWebUserMediaRequestPrivate();
}

/**
 * webkit_web_user_media_request_wants_audio:
 * @request: the #WebKitUserMediaRequest itself.
 *
 * Indicates the content of an UserMediaRequest.
 *
 * Return value: %TRUE if the request contains an audio request or %FALSE if otherwise.
 *
 * Since: 2.0.0
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
 * Return value: %TRUE if the request contains a video request or %FALSE if otherwise.
 *
 * Since: 2.0.0
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
 * Returns the security origin of the Media Request.
 *
 * Return value: (transfer none): the security origin of the media request.
 *
 * Since: 2.0.0
 */
WebKitSecurityOrigin* webkit_web_user_media_request_get_security_origin(WebKitWebUserMediaRequest* request)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_USER_MEDIA_REQUEST(request), 0);

    return kit(core(request)->scriptExecutionContext()->securityOrigin());
}

/**
 * webkit_web_user_media_request_succeed:
 * @webView: a #WebKitWebView
 * @webRequest: a #WebKitWebUserMediaRequest
 * @audioMediaList: a #WebKitWebUserMediaList containing audio devices
 * @videoMediaList: a #WebKitWebUserMediaList containing video devices
 *
 * This method should be called by the application after the user selects
 * a media device and accepts an media request
 *
 * Since: 2.0.0
 **/
void webkit_web_user_media_request_succeed(WebKitWebUserMediaRequest *webRequest,
                                           WebKitWebUserMediaList* audioMediaList,
                                           WebKitWebUserMediaList* audioMediaList)
{
    g_return_if_fail(WEBKIT_IS_WEB_USER_MEDIA_REQUEST(webRequest));
    g_return_if_fail(WEBKIT_IS_WEB_USER_MEDIA_LIST(audioMediaList));
    g_return_if_fail(WEBKIT_IS_WEB_USER_MEDIA_LIST(videoMediaList));

    MediaStreamSourceVector& audioSources = core(audioMediaList);
    MediaStreamSourceVector& videoSources = core(videoMediaList);

    for (size_t i = audioSources.size() - 1; i != (size_t) -1; --i)
        if (!webkit_web_user_media_list_is_item_selected(audioMediaList, i))
            audioSources.remove(i);

    for (size_t i = videoSources.size() - 1; i != (size_t) -1; --i)
        if (!webkit_web_user_media_list_is_item_selected(videoMediaList, i))
            videoSources.remove(i);

    core(webRequest)->succeed(audioSources, videoSources);

}

/**
 * webkit_web_view_reject_user_media_request:
 * @webView: a #WebKitWebView
 * @webRequest: a #WebKitWebUserMediaRequest
 *
 * This method should be called by the application when the user rejected a userMedia request.
 *
 * Since: 2.0.0
 **/
void webkit_web_user_media_request_fail(WebKitWebUserMediaRequest *webRequest)
{
    g_return_if_fail(WEBKIT_IS_WEB_USER_MEDIA_REQUEST(webRequest));
    core(webRequest)->fail();
}


namespace WebKit {

WebCore::UserMediaRequest* core(WebKitWebUserMediaRequest* request)
{
    ASSERT(WEBKIT_IS_WEB_USER_MEDIA_REQUEST(request));

    return request->priv->userMediaRequest.get();
}

WebKitWebUserMediaRequest* kitNew(WebCore::UserMediaRequest* request)
{
    ASSERT(request);

    WebKitWebUserMediaRequest* webRequest = WEBKIT_WEB_USER_MEDIA_REQUEST(g_object_new(WEBKIT_TYPE_WEB_USER_MEDIA_REQUEST, NULL));
    webRequest->priv->userMediaRequest = request;

    return webRequest;
}

}

#endif /* ENABLE(MEDIA_STREAM) */
