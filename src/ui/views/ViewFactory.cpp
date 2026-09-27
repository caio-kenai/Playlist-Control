#include "ui/views/ViewFactory.h"
#include "ui/views/ConfigManagerView.h"
#include "ui/views/ConfigView.h"
#include "ui/views/DashboardView.h"
#include "ui/views/IndexesView.h"
#include "ui/views/PlaylistIniView.h"
#include "ui/views/ReferenceViews.h"
#include "ui/views/ScheduleView.h"

namespace pc::ui
{

std::unique_ptr<View> createNamedView (ViewId id, AppContext& context)
{
    switch (id)
    {
        case ViewId::dashboard:   return std::make_unique<DashboardView> (context);
        case ViewId::maps:        return std::make_unique<ScheduleView> (context, ScheduleView::Mode::maps);
        case ViewId::grades:      return std::make_unique<ScheduleView> (context, ScheduleView::Mode::grades);
        case ViewId::clocks:      return std::make_unique<ScheduleView> (context, ScheduleView::Mode::clocks);
        case ViewId::playlistIni: return std::make_unique<PlaylistIniView> (context);
        case ViewId::config:      return std::make_unique<ConfigView> (context);
        case ViewId::configManager: return std::make_unique<ConfigManagerView> (context);
        case ViewId::folders:     return std::make_unique<FoldersView> (context);
        case ViewId::operators:   return std::make_unique<OperatorsView> (context);
        case ViewId::diagnostics: return std::make_unique<DiagnosticsView> (context);
        case ViewId::indexes:     return std::make_unique<IndexesView> (context);
        case ViewId::history:     return std::make_unique<HistoryView> (context);
    }
    return std::make_unique<DashboardView> (context);
}

} // namespace pc::ui
