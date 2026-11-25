#pragma once

#include "StreakData.hpp"

namespace gdstreaks {

/// Shows an in-game notification for a streak change, honouring the user's
/// notification setting. Does nothing for a no-op check-in.
void notifyStreakChange(CheckInResult const& result);

} // namespace gdstreaks
