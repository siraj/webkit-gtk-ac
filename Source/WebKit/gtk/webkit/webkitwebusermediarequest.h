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


#ifndef webkitwebusermediarequest_h
#define webkitwebusermediarequest_h

#include "webkitwebusermedialist.h"

#include <glib-object.h>
#include <glib.h>
#include <webkit/webkitdefines.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_WEB_USER_MEDIA_REQUEST            (webkit_web_user_media_request_get_type())
#define WEBKIT_WEB_USER_MEDIA_REQUEST(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_WEB_USER_MEDIA_REQUEST, WebKitWebUserMediaRequest))
#define WEBKIT_WEB_USER_MEDIA_REQUEST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_WEB_USER_MEDIA_REQUEST, WebKitWebUserMediaRequestClass))
#define WEBKIT_IS_WEB_USER_MEDIA_REQUEST(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_WEB_USER_MEDIA_REQUEST))
#define WEBKIT_IS_WEB_USER_MEDIA_REQUEST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_WEB_USER_MEDIA_REQUEST))
#define WEBKIT_WEB_USER_MEDIA_REQUEST_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_WEB_USER_MEDIA_REQUEST, WebKitWebUserMediaRequestClass))

typedef struct _WebKitWebUserMediaRequest WebKitWebUserMediaRequest;
typedef struct _WebKitWebUserMediaRequestClass WebKitWebUserMediaRequestClass;
typedef struct _WebKitWebUserMediaRequestPrivate WebKitWebUserMediaRequestPrivate;

struct _WebKitWebUserMediaRequest {
    GObject parent_instance;
    WebKitWebUserMediaRequestPrivate *priv;
};

struct _WebKitWebUserMediaRequestClass {
    GObjectClass parent_class;
};

WEBKIT_API GType
webkit_web_user_media_request_get_type (void) G_GNUC_CONST;

WEBKIT_API gboolean
webkit_web_user_media_request_wants_audio              (WebKitWebUserMediaRequest *request);

WEBKIT_API gboolean
webkit_web_user_media_request_wants_video              (WebKitWebUserMediaRequest *request);

WEBKIT_API WebKitSecurityOrigin *
webkit_web_user_media_request_get_security_origin      (WebKitWebUserMediaRequest *request);

WEBKIT_API void
webkit_web_user_media_request_fail                     (WebKitWebUserMediaRequest *request);

WEBKIT_API void
webkit_web_user_media_request_succeed                  (WebKitWebUserMediaRequest *webRequest,
                                                        WebKitWebUserMediaList* audioMediaList,
                                                        WebKitWebUserMediaList* videoMediaList);

G_END_DECLS

#endif /* webkitwebusermediarequest_h */
