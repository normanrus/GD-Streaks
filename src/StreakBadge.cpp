#include "StreakBadge.hpp"

#include "StreakData.hpp"
#include "StreakNotifier.hpp"
#include "StreakPopup.hpp"

#include <algorithm>

using namespace geode::prelude;

namespace gdstreaks {
namespace {

constexpr float BADGE_HEIGHT = 36.f;
constexpr float SIDE_PAD = 8.f;
constexpr float FLAME_SIZE = 26.f;
constexpr float GAP = 5.f;
constexpr float RIGHT_PAD = 10.f;
constexpr float FLAME_SCALE = FLAME_SIZE / 256.f;
constexpr int POP_ACTION_TAG = 0x5354524b;

} // namespace

StreakBadge* StreakBadge::create() {
    auto ret = new StreakBadge();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool StreakBadge::init() {
    if (!CCNode::init()) return false;

    m_hideButton = Mod::get()->getSettingValue<bool>("hide-button");
    m_showCounter = Mod::get()->getSettingValue<bool>("show-counter");

    m_pill = NineSlice::create("pill.png"_spr, {}, {18.f, 18.f, 18.f, 18.f});
    this->addChild(m_pill, 0);

    m_flameGroup = CCNode::create();
    this->addChild(m_flameGroup, 1);

    m_glow = CCSprite::create("glow.png"_spr);
    m_glow->setScale(0.22f);
    m_glow->setOpacity(120);
    m_flameGroup->addChild(m_glow, 0);

    m_flame = CCSprite::create("flame.png"_spr);
    m_flame->setScale(FLAME_SCALE);
    m_flameGroup->addChild(m_flame, 1);

    m_count = CCLabelBMFont::create("0", "bigFont.fnt");
    m_count->setScale(0.5f);
    m_count->setColor({255, 209, 102});
    m_count->setAnchorPoint({0.f, 0.5f});
    this->addChild(m_count, 2);

    this->refresh();
    this->setVisible(!m_hideButton);

    auto breathe = CCSequence::create(
        CCEaseSineInOut::create(CCScaleTo::create(0.9f, FLAME_SCALE * 1.09f)),
        CCEaseSineInOut::create(CCScaleTo::create(0.9f, FLAME_SCALE)),
        nullptr
    );
    m_flame->runAction(CCRepeatForever::create(breathe));
    m_flame->runAction(CCRepeatForever::create(CCSequence::create(
        CCRotateTo::create(1.1f, 4.f),
        CCRotateTo::create(1.1f, -4.f),
        nullptr
    )));
    m_glow->runAction(CCRepeatForever::create(CCSequence::create(
        CCEaseSineInOut::create(CCFadeTo::create(1.2f, 190)),
        CCEaseSineInOut::create(CCFadeTo::create(1.2f, 110)),
        nullptr
    )));

    this->scheduleUpdate();
    return true;
}

void StreakBadge::update(float dt) {
    // Settings can be toggled while the menu is open, so poll them instead of
    // keeping listener handles alive for every badge instance.
    m_settingPoll += dt;
    if (m_settingPoll >= 0.35f) {
        m_settingPoll = 0.f;
        this->applySettings();
    }

    // The game can stay open past midnight; keep the streak current without
    // waiting for the player to return to the menu.
    m_dayTick += dt;
    if (m_dayTick >= 60.f) {
        m_dayTick = 0.f;
        auto result = StreakData::get().checkIn();
        if (result.kind != CheckInResult::Kind::NoChange) {
            this->refresh();
            this->celebrate();
            notifyStreakChange(result);
        }
    }
}

void StreakBadge::applySettings() {
    auto hidden = Mod::get()->getSettingValue<bool>("hide-button");
    auto showCounter = Mod::get()->getSettingValue<bool>("show-counter");

    bool changed = false;

    if (hidden != m_hideButton) {
        m_hideButton = hidden;
        if (auto node = this->clickableNode()) {
            node->setVisible(!hidden);
            if (auto menu = typeinfo_cast<CCMenu*>(node->getParent())) {
                menu->updateLayout();
            }
        }
        changed = true;
    }
    if (showCounter != m_showCounter) {
        m_showCounter = showCounter;
        changed = true;
    }
    if (changed) {
        this->refresh();
    }
}

cocos2d::CCNode* StreakBadge::clickableNode() {
    if (auto item = typeinfo_cast<CCMenuItem*>(this->getParent())) {
        return item;
    }
    return this;
}

void StreakBadge::refresh() {
    auto& data = StreakData::get();
    m_count->setString(fmt::format("{}", data.current()).c_str());
    m_count->setVisible(m_showCounter);
    this->relayout();
}

void StreakBadge::relayout() {
    float labelWidth = m_showCounter
        ? m_count->getContentSize().width * m_count->getScale()
        : 0.f;

    float width = SIDE_PAD + FLAME_SIZE + (m_showCounter ? GAP + labelWidth : 0.f) + RIGHT_PAD;
    width = std::max(width, FLAME_SIZE + SIDE_PAD * 2.f);

    this->setContentSize({width, BADGE_HEIGHT});

    m_pill->setContentSize({width, BADGE_HEIGHT});
    m_pill->setPosition({width * 0.5f, BADGE_HEIGHT * 0.5f});

    m_flameGroup->setPosition({SIDE_PAD + FLAME_SIZE * 0.5f, BADGE_HEIGHT * 0.5f});
    m_count->setPosition({SIDE_PAD + FLAME_SIZE + GAP, BADGE_HEIGHT * 0.5f + 0.5f});

    // The menu item owns the hitbox, so keep it in sync with the new size.
    if (auto item = typeinfo_cast<CCMenuItem*>(this->getParent())) {
        item->setContentSize({width, BADGE_HEIGHT});
        if (auto menu = typeinfo_cast<CCMenu*>(item->getParent())) {
            menu->updateLayout();
        }
    }
}

void StreakBadge::celebrate() {
    // Animate the flame, not the badge itself: the badge is the menu item's
    // normal sprite and the button already animates its own scale on press.
    m_flameGroup->stopActionByTag(POP_ACTION_TAG);
    m_flameGroup->setScale(0.35f);
    auto pop = CCEaseElasticOut::create(CCScaleTo::create(0.85f, 1.f), 0.45f);
    pop->setTag(POP_ACTION_TAG);
    m_flameGroup->runAction(pop);
}

void StreakBadge::onOpen(cocos2d::CCObject* sender) {
    // Guard against a second click while the window is already up.
    if (auto scene = CCDirector::get()->getRunningScene()) {
        if (scene->getChildByIDRecursive("streak-popup"_spr)) {
            return;
        }
    }
    if (auto popup = StreakPopup::create()) {
        popup->show();
    }
}

} // namespace gdstreaks
