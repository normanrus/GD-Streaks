#pragma once

#include <algorithm>
#include <cstdint>
#include <ctime>
#include <string>

namespace gdstreaks {

/**
 * A calendar day expressed as days since 1970-01-01 in the player's local
 * timezone. Comparing plain day numbers keeps the streak logic immune to DST
 * shifts and to the exact clock time the game was launched at.
 *
 * This header only depends on the standard library so the rules below can be
 * unit tested without the Geometry Dash SDK (see tests/streak_logic_test.cpp).
 */
using DayIndex = int;

struct CheckInResult {
    enum class Kind {
        /// Nothing happened: the player was already counted today, or the local
        /// date moved backwards (travelling west / clock correction).
        NoChange,
        /// The very first activity ever recorded.
        FirstEver,
        /// Consecutive day, the streak grew.
        Continued,
        /// At least one day was missed, the streak restarted at 1.
        Reset,
    };

    Kind kind = Kind::NoChange;
    int streak = 0;
    int previousStreak = 0;
};

struct StreakState {
    int current = 0;
    int best = 0;
    DayIndex startDay = -1;
    DayIndex lastDay = -1;
    /// Wall clock time of the last check-in, used to tell a real missed day
    /// apart from a timezone jump that moves the local date forward by two.
    double lastSeen = 0.0;
};

namespace dateutil {

// Howard Hinnant's civil calendar algorithms.
inline std::int64_t daysFromCivil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    auto const era = (y >= 0 ? y : y - 399) / 400;
    auto const yoe = static_cast<unsigned>(y - era * 400);
    auto const doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    auto const doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097ll + static_cast<std::int64_t>(doe) - 719468ll;
}

inline void civilFromDays(std::int64_t z, int& y, unsigned& m, unsigned& d) {
    z += 719468ll;
    auto const era = (z >= 0 ? z : z - 146096) / 146097;
    auto const doe = static_cast<unsigned>(z - era * 146097);
    auto const yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    auto const doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    auto const mp = (5 * doy + 2) / 153;
    d = doy - (153 * mp + 2) / 5 + 1;
    m = mp + (mp < 10 ? 3 : -9);
    y = static_cast<int>(yoe) + static_cast<int>(era) * 400 + (m <= 2);
}

inline char const* monthName(unsigned m) {
    static char const* const months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
    };
    return (m >= 1 && m <= 12) ? months[m - 1] : "";
}

inline std::string formatDate(DayIndex day, bool shortMonth = false) {
    int y = 1970;
    unsigned m = 1, d = 1;
    civilFromDays(day, y, m, d);
    std::string out = std::to_string(d);
    out += ' ';
    out += monthName(m);
    if (!shortMonth) {
        out += ' ';
        out += std::to_string(y);
    }
    return out;
}

inline std::string weekdayInitial(DayIndex day) {
    // 1970-01-01 was a Thursday; the table below is Sunday first.
    static char const* const weekdays[] = {"S", "M", "T", "W", "T", "F", "S"};
    auto idx = ((static_cast<std::int64_t>(day) % 7) + 11) % 7;
    return weekdays[idx];
}

inline int dayOfMonth(DayIndex day) {
    int y = 1970;
    unsigned m = 1, d = 1;
    civilFromDays(day, y, m, d);
    return static_cast<int>(d);
}

inline DayIndex fromLocalTime(std::time_t when) {
    std::tm local{};
#ifdef GEODE_IS_WINDOWS
    localtime_s(&local, &when);
#else
    localtime_r(&when, &local);
#endif
    return static_cast<DayIndex>(daysFromCivil(local.tm_year + 1900, local.tm_mon + 1, local.tm_mday));
}

inline DayIndex today() {
    return fromLocalTime(std::time(nullptr));
}

} // namespace dateutil

inline bool isStreakMilestone(int streak) {
    for (auto m : {7, 14, 30, 50, 100}) {
        if (streak == m) return true;
    }
    return false;
}

inline int nextStreakMilestone(int streak) {
    for (auto m : {7, 14, 30, 50, 100}) {
        if (streak < m) return m;
    }
    return (streak / 100 + 1) * 100;
}

/**
 * Applies one activity check-in to `state`. Call this whenever the game is
 * launched, the player returns to the main menu, or a new local day starts.
 *
 * Rules:
 *  - the first activity ever starts a 1 day streak;
 *  - the same day never counts twice;
 *  - the next calendar day grows the streak;
 *  - any larger gap restarts the streak at 1, while `best` is kept;
 *  - a local date moving backwards (travel / clock change) never punishes.
 */
inline CheckInResult applyCheckIn(StreakState& state, DayIndex todayIdx, double nowSeconds) {
    CheckInResult result;

    if (state.lastDay < 0) {
        state.current = 1;
        state.best = std::max(state.best, 1);
        state.startDay = todayIdx;
        state.lastDay = todayIdx;
        state.lastSeen = nowSeconds;

        result.kind = CheckInResult::Kind::FirstEver;
        result.streak = state.current;
        return result;
    }

    auto diff = static_cast<std::int64_t>(todayIdx) - static_cast<std::int64_t>(state.lastDay);

    if (diff <= 0) {
        state.lastSeen = nowSeconds;
        result.kind = CheckInResult::Kind::NoChange;
        result.streak = state.current;
        return result;
    }

    bool const continued = diff == 1 ||
        // Two local days within one real day can only be a timezone change
        // (e.g. flying across the date line), not an actually missed day.
        (diff == 2 && state.lastSeen > 0.0 && nowSeconds - state.lastSeen < 36 * 3600);

    result.previousStreak = state.current;

    if (continued) {
        state.current += 1;
        result.kind = CheckInResult::Kind::Continued;
    } else {
        state.current = 1;
        state.startDay = todayIdx;
        result.kind = CheckInResult::Kind::Reset;
    }

    state.best = std::max(state.best, state.current);
    state.lastDay = todayIdx;
    state.lastSeen = nowSeconds;

    result.streak = state.current;
    return result;
}

} // namespace gdstreaks
