#pragma once

#include <Geode/Geode.hpp>

#include "StreakLogic.hpp"

#include <optional>
#include <string>
#include <vector>

namespace gdstreaks {

/// Daily streak state, persisted through the mod's save data.
class StreakData {
public:
    static StreakData& get();

    /// Registers today as an active day. Safe to call any number of times per
    /// day and per frame; only the first call of a new day changes the streak.
    CheckInResult checkIn();

    int current() const { return m_state.current; }
    int best() const { return m_state.best; }
    bool hasStreak() const { return m_state.current > 0; }
    bool activeToday() const;

    std::optional<DayIndex> startDay() const;
    std::optional<DayIndex> lastDay() const;

    /// Whether the player was active on the given local date.
    bool isActiveOn(DayIndex day) const;

    /// `count` day indices ending with today, oldest first.
    std::vector<DayIndex> recentDays(int count) const;

    static DayIndex today() { return dateutil::today(); }
    static std::string formatDate(DayIndex day, bool shortMonth = false) {
        return dateutil::formatDate(day, shortMonth);
    }
    static std::string weekdayInitial(DayIndex day) { return dateutil::weekdayInitial(day); }
    static int dayOfMonth(DayIndex day) { return dateutil::dayOfMonth(day); }
    static bool isMilestone(int streak) { return isStreakMilestone(streak); }
    static int nextMilestone(int streak) { return nextStreakMilestone(streak); }

private:
    static constexpr std::size_t MAX_HISTORY = 64;

    StreakState m_state;
    std::vector<int> m_history;

    StreakData() = default;

    void load();
    void save();
    void addToHistory(DayIndex day);
};

} // namespace gdstreaks
