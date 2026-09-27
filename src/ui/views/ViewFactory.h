#pragma once

#include "ui/View.h"

namespace pc::ui
{

std::unique_ptr<View> createNamedView (ViewId id, AppContext& context);

} // namespace pc::ui
