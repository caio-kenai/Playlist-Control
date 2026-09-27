#include "ui/views/ViewFactory.h"
#include "ui/views/DashboardView.h"
#include "ui/views/ScheduleView.h"

namespace pc::ui
{

std::unique_ptr<View> createNamedView (ViewId id, AppContext& context)
{
    switch (id)
    {
        default: break;
        case ViewId::dashboard:   return std::make_unique<DashboardView> (context);
        case ViewId::maps:        return std::make_unique<ScheduleView> (context, ScheduleView::Mode::maps);
        case ViewId::grades:      return std::make_unique<ScheduleView> (context, ScheduleView::Mode::grades);
        case ViewId::clocks:      return std::make_unique<ScheduleView> (context, ScheduleView::Mode::clocks);
    }
    return std::make_unique<DashboardView> (context);
}

} // namespace pc::ui
