#pragma once

#include "aviotrix/metadata.h"

struct AVFormatContext;

namespace aviotrix::detail {
Metadata readMetadata(const AVFormatContext* fmt);
}
