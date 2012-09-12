/*
 * Copyright (C) 2010 Collabora Ltd.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1 of
 *  the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */

#include "config.h"
#include "TransformationMatrix.h"

#include <cogl/cogl.h>

namespace WebCore {

TransformationMatrix::operator CoglMatrix() const
{
    CoglMatrix matrix;

    matrix.xx = (float)m11();
    matrix.xy = (float)m21();
    matrix.xz = (float)m31();
    matrix.xw = (float)m41();

    matrix.yx = (float)m12();
    matrix.yy = (float)m22();
    matrix.yz = (float)m32();
    matrix.yw = (float)m42();

    matrix.zx = (float)m13();
    matrix.zy = (float)m23();
    matrix.zz = (float)m33();
    matrix.zw = (float)m43();

    matrix.wx = (float)m14();
    matrix.wy = (float)m24();
    matrix.wz = (float)m34();
    matrix.ww = (float)m44();

    return matrix;
}

}

// vim: ts=4 sw=4 et
