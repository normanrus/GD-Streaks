#pragma once

#include <Geode/Geode.hpp>

namespace gdstreaks {

/// Small flame + day count pill that lives in the main menu's top right corner.
class StreakBadge : public cocos2d::CCNode {
public:
    static StreakBadge* create();

    /// Re-reads the streak values and resizes the pill.
    void refresh();
    /// Little pop, played when the badge appears on a day that counted.
    void celebrate();
    void onOpen(cocos2d::CCObject* sender);

    void update(float dt) override;

protected:
    bool init() override;

private:
    geode::NineSlice* m_pill = nullptr;
    cocos2d::CCNode* m_flameGroup = nullptr;
    cocos2d::CCSprite* m_flame = nullptr;
    cocos2d::CCSprite* m_glow = nullptr;
    cocos2d::CCLabelBMFont* m_count = nullptr;

    float m_settingPoll = 0.f;
    float m_dayTick = 0.f;
    bool m_showCounter = true;
    bool m_hideButton = false;

    void applySettings();
    void relayout();
    /// The node whose visibility actually decides whether clicks land on us.
    cocos2d::CCNode* clickableNode();
};

} // namespace gdstreaks
