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
#include "webkitwebusermedialist.h"

#if ENABLE(MEDIA_STREAM)

#include "webkitglobalsprivate.h"
#include "webkitwebusermedialistprivate.h"
#include <wtf/text/CString.h>


/**
 * SECTION:webkitwebusermedialist
 * @short_description: UserMedia List
 * @Title: WebKitWebUserMediaList
 *
 * #WebKitWebUserMediaList contains a list of
 * audio or video sources from a WebUserMediaRequest
 *
 * Since: 2.0.0
 */

using namespace WebKit;

struct _WebKitWebUserMediaListPrivate {
    WebCore::MediaStreamSourceVector sources;
    Vector<bool> sourceSelections;
};

G_DEFINE_TYPE(WebKitWebUserMediaList, webkit_web_user_media_list, G_TYPE_OBJECT)

static void webkitWebUserMediaListFinalize(GObject* object)
{
    WEBKIT_WEB_USER_MEDIA_LIST(object)->priv->~WebKitWebUserMediaListPrivate();
    G_OBJECT_CLASS(webkit_web_user_media_list_parent_class)->finalize(object);
}

static void webkit_web_user_media_list_class_init(WebKitWebUserMediaListClass* listClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(listClass);
    gobjectClass->finalize = webkitWebUserMediaListFinalize;

    webkitInit();

    g_type_class_add_private(listClass, sizeof(WebKitWebUserMediaListPrivate));
}

static void webkit_web_user_media_list_init(WebKitWebUserMediaList* list)
{
    WebKitWebUserMediaListPrivate* priv = G_TYPE_INSTANCE_GET_PRIVATE(list, WEBKIT_TYPE_WEB_USER_MEDIA_LIST, WebKitWebUserMediaListPrivate);
    list->priv = priv;

    // placement new syntax
    new (priv) WebKitWebUserMediaListPrivate();
}

/**
 * webkit_web_user_media_list_get_length:
 * @list: the WebKitWebUserMediaList itself
 *
 * Returns the size of the elements list.
 *
 * Return value: number of elements.
 *
 * Since: 2.0.0
 */
guint webkit_web_user_media_list_get_length(WebKitWebUserMediaList* list)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_USER_MEDIA_LIST(list), 0);

    return core(list).size();
}


/**
 * webkit_web_user_media_list_get_item_name:
 * @list: the #WebKitWebUserMediaList itself
 * @index: the wanted element index.
 *
 * Returns the device name of the element.
 *
 * Return value: the device id of the @index given element.
 *
 * Since: 2.0.0
 */
const gchar* webkit_web_user_media_list_get_item_name(WebKitWebUserMediaList* list, guint index)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_USER_MEDIA_LIST(list), 0);

    WebCore::MediaStreamSourceVector& sources = core(list);
    g_return_val_if_fail(index < sources.size(), 0);

    return g_strdup(sources[index]->name().utf8().data());
}

/**
 * webkit_web_user_media_list_is_item_selected:
 * @list: the #WebKitWebUserMediaList itself
 * @index: the wanted element index.
 *
 * Indicate if a device is marked as selected.
 *
 * Return value: True if the device represented by @index is marked as selected.
 *
 * Since: 2.0.0
 */
gboolean webkit_web_user_media_list_is_item_selected(WebKitWebUserMediaList* list, guint index)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_USER_MEDIA_LIST(list), FALSE);
    g_return_val_if_fail(index < core(list).size(), FALSE);

    return list->priv->sourceSelections[index];
}

/**
 * webkit_web_user_media_list_select_item:
 * @list: the #WebKitWebUserMediaList itself
 * @index: the wanted element index.
 *
 * Mark a device as selecteed.
 *
 * Since: 2.0.0
 */
void webkit_web_user_media_list_select_item(WebKitWebUserMediaList* list, guint index)
{
    g_return_if_fail(WEBKIT_IS_WEB_USER_MEDIA_LIST(list));
    g_return_if_fail(index < core(list).size());

    list->priv->sourceSelections[index] = TRUE;
}

namespace WebKit {

WebCore::MediaStreamSourceVector& core(WebKitWebUserMediaList* list)
{
    return list->priv->sources;
}

WebKitWebUserMediaList* kitNew(const WebCore::MediaStreamSourceVector& sources)
{
    WebKitWebUserMediaList* webList = WEBKIT_WEB_USER_MEDIA_LIST(g_object_new(WEBKIT_TYPE_WEB_USER_MEDIA_LIST, NULL));
    webList->priv->sources = sources;
    webList->priv->sourceSelections.fill(FALSE, sources.size());

    return webList;
}

}

#endif /* ENABLE(MEDIA_STREAM) */
