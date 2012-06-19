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

#include "webkitwebusermedialist.h"

#include "MediaStreamSource.h"
#include "webkitglobalsprivate.h"
#include "webkitwebusermedialistprivate.h"

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <stdio.h>
#include <wtf/text/CString.h>


/**
 * SECTION:webkitwebusermedialist
 * @short_description: UserMedia List
 * @Title: WebKitWebUserMediaList
 *
 * #WebKitWebUserMediaList contains a list of
 * audio and video sources from a WebUserMediaRequest
 *
 * Since: FIXME
 */

using namespace WebKit;

struct _WebKitWebUserMediaListPrivate {
    WebCore::MediaStreamSourceVector audioSources;
    WebCore::MediaStreamSourceVector videoSources;
    gboolean* audioSourceSelections;
    gboolean* videoSourceSelections;
};

G_DEFINE_TYPE(WebKitWebUserMediaList, webkit_web_user_media_list, G_TYPE_OBJECT);

static void webkit_web_user_media_list_dispose(GObject* object)
{
    G_OBJECT_CLASS(webkit_web_user_media_list_parent_class)->dispose(object);
}

static void webkit_web_user_media_list_finalize(GObject* object)
{
    WebKitWebUserMediaList* userMediaList = WEBKIT_WEB_USER_MEDIA_LIST(object);

    g_free(userMediaList->priv->audioSourceSelections);
    g_free(userMediaList->priv->videoSourceSelections);

    G_OBJECT_CLASS(webkit_web_user_media_list_parent_class)->finalize(object);
}

static void webkit_web_user_media_list_class_init(WebKitWebUserMediaListClass* listClass)
{
    GObjectClass* gobject_class = G_OBJECT_CLASS(listClass);
    gobject_class->dispose = webkit_web_user_media_list_dispose;
    gobject_class->finalize = webkit_web_user_media_list_finalize;

    webkitInit();

    g_type_class_add_private(listClass, sizeof(WebKitWebUserMediaListPrivate));
}

static void webkit_web_user_media_list_init(WebKitWebUserMediaList* list)
{
    WebKitWebUserMediaListPrivate* priv = G_TYPE_INSTANCE_GET_PRIVATE(list, WEBKIT_TYPE_WEB_USER_MEDIA_LIST, WebKitWebUserMediaListPrivate);
    list->priv = priv;
}

/**
 * webkit_web_user_media_list_get_audio_length:
 * @list: the WebKitWebUserMediaList itself
 *
 * Returns the size of the audio elements list.
 *
 * Return value: number of audio elements.
 *
 * Since: FIXME
 */
guint webkit_web_user_media_list_get_audio_length(WebKitWebUserMediaList* list)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_USER_MEDIA_LIST(list), 0);

    return audioCore(list).size();
}

/**
 * webkit_web_user_media_list_get_video_length:
 * @list: the WebKitWebUserMediaList itself
 *
 * Returns the size of the video elements list. 
 *
 * Return value: number of video elements.
 *
 * Since: FIXME
 */
guint webkit_web_user_media_list_get_video_length(WebKitWebUserMediaList* list)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_USER_MEDIA_LIST(list), 0);

    return videoCore(list).size();
}

/**
 * webkit_web_user_media_list_get_audio_item_name:
 * @list: the WebKitWebUserMediaList itself
 * @index: the wanted element index.
 *
 * Returns the device name of an audio element.
 *
 * Return value: the device id of the @index given element.
 *
 * Since: FIXME
 */
const gchar* webkit_web_user_media_list_get_audio_item_name(WebKitWebUserMediaList* list, guint index)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_USER_MEDIA_LIST(list), 0);

    WebCore::MediaStreamSourceVector& sources = audioCore(list);
    g_return_val_if_fail(index < sources.size(), 0);

    return g_strdup(sources[index]->name().utf8().data());
}

/**
 * webkit_web_user_media_list_get_video_item_name:
 * @list: the WebKitWebUserMediaList itself
 * @index: the wanted element index.
 *
 * Returns the device name of an video element.
 *
 * Return value: the device id of the @index given element.
 *
 * Since: FIXME
 */
const gchar* webkit_web_user_media_list_get_video_item_name(WebKitWebUserMediaList* list, guint index)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_USER_MEDIA_LIST(list), 0);

    WebCore::MediaStreamSourceVector& sources = videoCore(list);
    g_return_val_if_fail(index < sources.size(), 0);

    return g_strdup(sources[index]->name().utf8().data());
}

/**
 * webkit_web_user_media_list_get_audio_item_selected:
 * @list: the WebKitWebUserMediaList itself
 * @index: the wanted element index.
 *
 * Indicate if an audio device is marked as selected.
 *
 * Return value: True if the device represented by @index is marked as selected.
 *
 * Since: FIXME
 */
gboolean webkit_web_user_media_list_get_audio_item_selected(WebKitWebUserMediaList* list, guint index)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_USER_MEDIA_LIST(list), FALSE);

    WebCore::MediaStreamSourceVector& sources = audioCore(list);
    g_return_val_if_fail(index < sources.size(), FALSE);

    return list->priv->audioSourceSelections[index];
}

/**
 * webkit_web_user_media_list_get_video_item_selected:
 * @list: the WebKitWebUserMediaList itself
 * @index: the wanted element index.
 *
 * Indicate if a video device is marked as selected.
 *
 * Return value: True if the device represented by @index is marked as selected.
 *
 * Since: FIXME
 */
gboolean webkit_web_user_media_list_get_video_item_selected(WebKitWebUserMediaList* list, guint index)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_USER_MEDIA_LIST(list), FALSE);

    WebCore::MediaStreamSourceVector& sources = videoCore(list);
    g_return_val_if_fail(index < sources.size(), FALSE);

    return list->priv->videoSourceSelections[index];
}

/**
 * webkit_web_user_media_list_select_audio_item:
 * @list: the WebKitWebUserMediaList itself
 * @index: the wanted element index.
 *
 * Mark an audio device as selecteed.
 *
 * Since: FIXME
 */
void webkit_web_user_media_list_select_audio_item(WebKitWebUserMediaList* list, guint index)
{
    g_return_if_fail(WEBKIT_IS_WEB_USER_MEDIA_LIST(list));

    WebCore::MediaStreamSourceVector& sources = audioCore(list);
    g_return_if_fail(index < sources.size());

    list->priv->audioSourceSelections[index] = TRUE;
}

/**
 * webkit_web_user_media_list_select_video_item:
 * @list: the WebKitWebUserMediaList itself
 * @index: the wanted element index.
 *
 * Mark an video device as selecteed.
 *
 * Since: FIXME
 */
void webkit_web_user_media_list_select_video_item(WebKitWebUserMediaList* list, guint index)
{
    g_return_if_fail(WEBKIT_IS_WEB_USER_MEDIA_LIST(list));

    WebCore::MediaStreamSourceVector& sources = videoCore(list);
    g_return_if_fail(index < sources.size());

    list->priv->videoSourceSelections[index] = TRUE;
}

namespace WebKit {

WebCore::MediaStreamSourceVector& audioCore(WebKitWebUserMediaList* list)
{
    return list->priv->audioSources;
}

WebCore::MediaStreamSourceVector& videoCore(WebKitWebUserMediaList* list)
{
    return list->priv->videoSources;
}

WebKitWebUserMediaList* kitNew(const WebCore::MediaStreamSourceVector& audioSources, const WebCore::MediaStreamSourceVector& videoSources)
{
    WebKitWebUserMediaList* webList = WEBKIT_WEB_USER_MEDIA_LIST(g_object_new(WEBKIT_TYPE_WEB_USER_MEDIA_LIST, NULL));
    webList->priv->audioSources = audioSources;
    webList->priv->videoSources = videoSources;
    webList->priv->audioSourceSelections = g_new0(gboolean, audioSources.size());
    webList->priv->videoSourceSelections = g_new0(gboolean, videoSources.size());

    return webList;
}

}

#endif /* ENABLE(MEDIA_STREAM) */
