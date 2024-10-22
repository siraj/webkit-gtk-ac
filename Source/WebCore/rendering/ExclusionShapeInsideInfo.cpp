/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "ExclusionShapeInsideInfo.h"

#if ENABLE(CSS_EXCLUSIONS)

#include "NotImplemented.h"
#include "RenderBlock.h"
#include <wtf/HashMap.h>
#include <wtf/OwnPtr.h>

namespace WebCore {

typedef HashMap<const RenderBlock*, OwnPtr<ExclusionShapeInsideInfo> > ExclusionShapeInsideInfoMap;

static ExclusionShapeInsideInfoMap& exclusionShapeInsideInfoMap()
{
    DEFINE_STATIC_LOCAL(ExclusionShapeInsideInfoMap, staticExclusionShapeInsideInfoMap, ());
    return staticExclusionShapeInsideInfoMap;
}

ExclusionShapeInsideInfo::ExclusionShapeInsideInfo(RenderBlock* block)
    : m_block(block)
    , m_shapeSizeDirty(true)
{
}

ExclusionShapeInsideInfo::~ExclusionShapeInsideInfo()
{
}

ExclusionShapeInsideInfo* ExclusionShapeInsideInfo::ensureExclusionShapeInsideInfoForRenderBlock(RenderBlock* block)
{
    ExclusionShapeInsideInfoMap::AddResult result = exclusionShapeInsideInfoMap().add(block, create(block));
    return result.iterator->second.get();
}

ExclusionShapeInsideInfo* ExclusionShapeInsideInfo::exclusionShapeInsideInfoForRenderBlock(const RenderBlock* block)
{
    ASSERT(block->style()->shapeInside());
    return exclusionShapeInsideInfoMap().get(block);
}

bool ExclusionShapeInsideInfo::isExclusionShapeInsideInfoEnabledForRenderBlock(const RenderBlock* block)
{
    // FIXME: Bug 89707: Enable shape inside for non-rectangular shapes
    BasicShape* shape = block->style()->shapeInside();
    return (shape && shape->type() == BasicShape::BASIC_SHAPE_RECTANGLE);
}

void ExclusionShapeInsideInfo::removeExclusionShapeInsideInfoForRenderBlock(const RenderBlock* block)
{
    if (!block->style() || !block->style()->shapeInside())
        return;
    exclusionShapeInsideInfoMap().remove(block);
}

void ExclusionShapeInsideInfo::computeShapeSize(LayoutUnit logicalWidth, LayoutUnit logicalHeight)
{
    if (!m_shapeSizeDirty && logicalWidth == m_logicalWidth && logicalHeight == m_logicalHeight)
        return;

    m_shapeSizeDirty = false;
    m_logicalWidth = logicalWidth;
    m_logicalHeight = logicalHeight;

    // FIXME: Bug 89993: The wrap shape may come from the parent object
    BasicShape* shape = m_block->style()->shapeInside();
    ASSERT(shape);

    m_shape = ExclusionShape::createExclusionShape(shape, logicalWidth, logicalHeight, m_block->style()->writingMode());
    ASSERT(m_shape);
}

bool ExclusionShapeInsideInfo::computeSegmentsForLine(LayoutUnit lineTop, LayoutUnit lineBottom)
{
    ASSERT(lineTop <= lineBottom);
    m_lineTop = lineTop;
    m_lineBottom = lineBottom;
    m_segments.clear();

    if (lineOverlapsShapeBounds()) {
        ASSERT(m_shape);
        m_shape->getIncludedIntervals(lineTop, std::min(lineBottom, shapeLogicalBottom()), m_segments);
    }
    return m_segments.size();
}

}
#endif
