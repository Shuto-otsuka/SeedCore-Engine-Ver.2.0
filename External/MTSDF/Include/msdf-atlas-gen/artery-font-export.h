
#pragma once

#ifndef MSDF_ATLAS_NO_ARTERY_FONT

#include <External/MTSDF/Include/msdfgen/msdfgen.h>
#include <External/MTSDF/Include/msdfgen/msdfgen-ext.h>
#include "types.h"
#include "FontGeometry.h"

namespace msdf_atlas {

struct ArteryFontExportProperties {
    double fontSize;
    msdfgen::Range pxRange;
    ImageType imageType;
    ImageFormat imageFormat;
};

/// Encodes the atlas bitmap and its layout into an Artery Atlas Font file
template <typename REAL, typename T, int N>
bool exportArteryFont(const FontGeometry *fonts, int fontCount, const msdfgen::BitmapConstSection<T, N> &atlas, const char *filename, const ArteryFontExportProperties &properties);

}

#endif
