/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TimeInputType_h
#define TimeInputType_h

#if ENABLE(INPUT_TYPE_TIME)
#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
#include "BaseMultipleFieldsDateAndTimeInputType.h"
#else
#include "BaseDateAndTimeInputType.h"
#endif

namespace WebCore {

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
typedef BaseMultipleFieldsDateAndTimeInputType BaseTimeInputType;
#else
typedef BaseDateAndTimeInputType BaseTimeInputType;
#endif

class TimeInputType : public BaseTimeInputType {
public:
    static PassOwnPtr<InputType> create(HTMLInputElement*);

private:
    TimeInputType(HTMLInputElement*);
    virtual const AtomicString& formControlType() const OVERRIDE;
    virtual DateComponents::Type dateType() const OVERRIDE;
    virtual Decimal defaultValueForStepUp() const OVERRIDE;
    virtual StepRange createStepRange(AnyStepHandling) const OVERRIDE;
    virtual bool parseToDateComponentsInternal(const UChar*, unsigned length, DateComponents*) const OVERRIDE;
    virtual bool setMillisecondToDateComponents(double, DateComponents*) const OVERRIDE;
    virtual bool isTimeField() const OVERRIDE;

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
    // BaseMultipleFieldsDateAndTimeInputType functions
    virtual String formatDateTimeFieldsState(const DateTimeFieldsState&) const OVERRIDE FINAL;
    virtual void setupLayoutParameters(DateTimeEditElement::LayoutParameters&, const DateComponents&) const OVERRIDE FINAL;
#endif
};

} // namespace WebCore

#endif
#endif // TimeInputType_h
