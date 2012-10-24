/*
 * Copyright (C) 2012 Collabora.
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
 */

#ifndef GtkMediaChooserDialog_h
#define GtkMediaChooserDialog_h

#include "MediaStreamSource.h"
#include "UserMediaRequest.h"

#include <wtf/FastAllocBase.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

class GtkMediaChooserDialog {
    WTF_MAKE_NONCOPYABLE(GtkMediaChooserDialog);
    WTF_MAKE_FAST_ALLOCATED;

public:
    GtkMediaChooserDialog(GtkWidget *parent, UserMediaRequest*, const MediaStreamSourceVector&, const MediaStreamSourceVector&);
    ~GtkMediaChooserDialog();

    void show();
    int selectedAudio();
    int selectedVideo();
    GtkWidget* widget();

private:

    GtkWidget *m_dialog;
    GtkWidget *m_audioCombo;
    GtkWidget *m_videoCombo;
};

} // namespace WebCore

#endif // GtkMediaChooserDialog_h
