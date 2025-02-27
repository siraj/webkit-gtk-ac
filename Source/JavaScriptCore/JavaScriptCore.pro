# -------------------------------------------------------------------
# Main project file for JavaScriptSource
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

TEMPLATE = subdirs
CONFIG += ordered

linux-*:!equals(QT_ARCH, "arm") {
    LLIntOffsetsExtractor.file = LLIntOffsetsExtractor.pro
    LLIntOffsetsExtractor.makefile = Makefile.LLIntOffsetsExtractor
    SUBDIRS += LLIntOffsetsExtractor
}

derived_sources.file = DerivedSources.pri
target.file = Target.pri

SUBDIRS += derived_sources target

linux-*:!equals(QT_ARCH, "arm"):addStrictSubdirOrderBetween(LLIntOffsetsExtractor, derived_sources)
addStrictSubdirOrderBetween(derived_sources, target)

jsc.file = jsc.pro
jsc.makefile = Makefile.jsc
SUBDIRS += jsc
