#include "StreakData.hpp"

#include <algorithm>
#include <ctime>

using namespace geode::prelude;

namespace gdstreaks {

StreakData& StreakData::get() {
    static StreakData instance;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        instance.load();
    }
    return instance;
}

bool StreakData::activeToday() const {
    return m_state.lastDay >= 0 && m_state.lastDay == dateutil::today();
}

std::optional<DayIndex> StreakData::startDay() const {
    if (m_state.startDay < 0) return std::nullopt;
    return m_state.startDay;
}

std::optional<DayIndex> StreakData::lastDay() const {
    if (m_state.lastDay < 0) return std::nullopt;
    return m_state.lastDay;
}

bool StreakData::isActiveOn(DayIndex day) const {
    return std::binary_search(m_history.begin(), m_history.end(), day);
}

std::vector<DayIndex> StreakData::recentDays(int count) const {
    std::vector<DayIndex> days;
    if (count <= 0) return days;

    auto t = dateutil::today();
    days.reserve(static_cast<std::size_t>(count));
    for (int i = count - 1; i >= 0; --i) {
        days.push_back(t - i);
    }
    return days;
}

CheckInResult StreakData::checkIn() {
    auto todayIdx = dateutil::today();
    auto now = static_cast<double>(std::time(nullptr));

    auto result = applyCheckIn(m_state, todayIdx, now);
    if (result.kind == CheckInResult::Kind::NoChange) {
        return result;
    }

    addToHistory(todayIdx);
    save();
    return result;
}

void StreakData::addToHistory(DayIndex day) {
    auto it = std::lower_bound(m_history.begin(), m_history.end(), day);
    if (it == m_history.end() || *it != day) {
        m_history.insert(it, day);
    }
    while (m_history.size() > MAX_HISTORY) {
        m_history.erase(m_history.begin());
    }
}

void StreakData::load() {
    auto mod = Mod::get();

    m_state.current = std::max(0, mod->getSavedValue<int>("current-streak", 0));
    m_state.best = std::max(0, mod->getSavedValue<int>("best-streak", 0));
    m_state.startDay = mod->getSavedValue<int>("start-day", -1);
    m_state.lastDay = mod->getSavedValue<int>("last-day", -1);
    m_state.lastSeen = mod->getSavedValue<double>("last-seen", 0.0);

    m_history = mod->getSavedValue<std::vector<int>>("history", std::vector<int>{});
    std::sort(m_history.begin(), m_history.end());
    m_history.erase(std::unique(m_history.begin(), m_history.end()), m_history.end());

    // Guard against hand edited or corrupted save data.
    if (m_state.current <= 0 || m_state.lastDay < 0) {
        m_state = StreakState{};
        m_history.clear();
    }
    if (m_state.startDay < 0 || m_state.startDay > m_state.lastDay) {
        m_state.startDay = m_state.lastDay;
    }
    m_state.best = std::max(m_state.best, m_state.current);
}

void StreakData::save() {
    auto mod = Mod::get();

    mod->setSavedValue<int>("current-streak", m_state.current);
    mod->setSavedValue<int>("best-streak", m_state.best);
    mod->setSavedValue<int>("start-day", m_state.startDay);
    mod->setSavedValue<int>("last-day", m_state.lastDay);
    mod->setSavedValue<double>("last-seen", m_state.lastSeen);
    mod->setSavedValue<std::vector<int>>("history", m_history);

    // Write immediately instead of waiting for a clean shutdown, otherwise a
    // crash right after launch would lose the day's check-in.
    if (auto res = mod->saveData(); res.isErr()) {
        log::warn("Failed to save streak data: {}", res.unwrapErr());
    }
}

} // namespace gdstreaks
