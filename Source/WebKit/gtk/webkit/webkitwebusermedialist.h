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


#ifndef webkitwebusermedialist_h
#define webkitwebusermedialist_h

#include <glib-object.h>
#include <glib.h>

#include <webkit/webkitdefines.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_WEB_USER_MEDIA_LIST               (webkit_web_user_media_list_get_type())
#define WEBKIT_WEB_USER_MEDIA_LIST(obj)               (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_WEB_USER_MEDIA_LIST, WebKitWebUserMediaList))
#define WEBKIT_WEB_USER_MEDIA_LIST_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_WEB_USER_MEDIA_LIST, WebKitWebUserMediaListClass))
#define WEBKIT_IS_WEB_USER_MEDIA_LIST(obj)            (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_WEB_USER_MEDIA_LIST))
#define WEBKIT_IS_WEB_USER_MEDIA_LIST_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_WEB_USER_MEDIA_LIST))
#define WEBKIT_WEB_USER_MEDIA_LIST_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_WEB_USER_MEDIA_LIST, WebKitWebUserMediaListClass))

typedef struct _WebKitWebUserMediaList WebKitWebUserMediaList;
typedef struct _WebKitWebUserMediaListClass WebKitWebUserMediaListClass;
typedef struct _WebKitWebUserMediaListPrivate WebKitWebUserMediaListPrivate;

struct _WebKitWebUserMediaList {
    GObject parent_instance;
    WebKitWebUserMediaListPrivate *priv;
};

struct _WebKitWebUserMediaListClass {
    GObjectClass parent_class;
};

WEBKIT_API GType
webkit_web_user_media_list_get_type (void) G_GNUC_CONST;

WEBKIT_API guint
webkit_web_user_media_list_get_length        (WebKitWebUserMediaList *list);

WEBKIT_API const gchar *
webkit_web_user_media_list_get_item_name     (WebKitWebUserMediaList *list,
                                              guint index);
WEBKIT_API gboolean
webkit_web_user_media_list_is_item_selected (WebKitWebUserMediaList *list,
                                              guint index);
WEBKIT_API void
webkit_web_user_media_list_select_item       (WebKitWebUserMediaList *list,
                                              guint index);

G_END_DECLS

#endif /* webkitwebusermedialist_h */
