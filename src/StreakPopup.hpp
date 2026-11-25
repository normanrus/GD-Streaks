#pragma once

#include <Geode/Geode.hpp>

namespace gdstreaks {

/// Window opened from the badge: shows the current and best streak, the start
/// and last activity dates, a 7 day calendar and progress to the next milestone.
class StreakPopup : public geode::Popup {
public:
    static StreakPopup* create();

protected:
    bool init();

private:
    cocos2d::CCSprite* m_flame = nullptr;
    cocos2d::CCSprite* m_glow = nullptr;
    cocos2d::CCLabelBMFont* m_countLabel = nullptr;
    geode::NineSlice* m_barFill = nullptr;

    void buildHeader(float centerY);
    void buildBest(int best);
    void buildCalendar(float centerY);
    void buildProgress(float labelY, float barY, int streak, int next);
    void buildFooter(float y);
    void startIdleAnimation();
};

} // namespace gdstreaks
