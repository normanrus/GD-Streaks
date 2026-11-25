#include "StreakPopup.hpp"

#include "StreakData.hpp"

#include <algorithm>
#include <vector>

using namespace geode::prelude;

namespace gdstreaks {
namespace {

constexpr float POPUP_WIDTH = 420.f;
constexpr float POPUP_HEIGHT = 280.f;

// Vertical layout, measured from the bottom of the popup background.
constexpr float HEADER_Y = 210.f;
constexpr float DIVIDER_Y = 168.f;
constexpr float CALENDAR_Y = 124.f;
constexpr float PROGRESS_LABEL_Y = 78.f;
constexpr float BAR_Y = 60.f;
constexpr float BAR_VALUE_Y = 40.f;
constexpr float FOOTER_Y = 18.f;

// Horizontal layout.
constexpr float FLAME_X = 76.f;
constexpr float COUNT_X = 132.f;
constexpr float COUNT_MAX_WIDTH = 88.f;
constexpr float BEST_LABEL_X = 244.f;
constexpr float BEST_LABEL_Y = 232.f;
constexpr float BEST_VALUE_Y = 207.f;

constexpr float CELL_SIZE = 36.f;
constexpr float CELL_GAP = 6.f;
constexpr float BAR_WIDTH = 320.f;
constexpr float BAR_HEIGHT = 14.f;

constexpr ccColor3B COLOR_TEXT = {236, 236, 236};
constexpr ccColor3B COLOR_MUTED = {155, 155, 155};
constexpr ccColor3B COLOR_GOLD = {255, 209, 102};
constexpr ccColor3B COLOR_PRIMARY = {255, 138, 51};

CCSprite* makeRect(float width, float height, ccColor4B color) {
    auto spr = CCSprite::create("white.png"_spr);
    if (!spr) return nullptr;
    auto size = spr->getContentSize();
    spr->setScaleX(width / std::max(1.f, size.width));
    spr->setScaleY(height / std::max(1.f, size.height));
    spr->setColor({color.r, color.g, color.b});
    spr->setOpacity(color.a);
    return spr;
}

CCLabelBMFont* makeLabel(
    std::string const& text, char const* font, float scale, ccColor3B color,
    CCPoint anchor = {0.5f, 0.5f}
) {
    auto label = CCLabelBMFont::create(text.c_str(), font);
    label->setScale(scale);
    label->setColor(color);
    label->setAnchorPoint(anchor);
    return label;
}

template <typename NodeT>
void fadeIn(NodeT* node, float delay, float duration = 0.25f) {
    node->setOpacity(0);
    node->runAction(CCSequence::create(
        CCDelayTime::create(delay),
        CCFadeTo::create(duration, 255),
        nullptr
    ));
}

} // namespace

StreakPopup* StreakPopup::create() {
    auto ret = new StreakPopup();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool StreakPopup::init() {
    if (!Popup::init(POPUP_WIDTH, POPUP_HEIGHT)) return false;

    this->setID("streak-popup"_spr);
    this->setTitle("Streaks");

    auto& data = StreakData::get();
    auto streak = data.current();
    auto next = StreakData::nextMilestone(streak);

    this->buildHeader(HEADER_Y);
    this->buildBest(data.best());

    if (auto line = makeRect(POPUP_WIDTH - 64.f, 1.5f, {255, 255, 255, 40})) {
        line->setPosition({POPUP_WIDTH / 2.f, DIVIDER_Y});
        m_mainLayer->addChild(line, 1);
    }

    this->buildCalendar(CALENDAR_Y);
    this->buildProgress(PROGRESS_LABEL_Y, BAR_Y, streak, next);
    this->buildFooter(FOOTER_Y);
    this->startIdleAnimation();

    return true;
}

void StreakPopup::buildHeader(float centerY) {
    auto& data = StreakData::get();
    bool milestone = StreakData::isMilestone(data.current());

    auto group = CCNode::create();
    group->setPosition({FLAME_X, centerY});
    m_mainLayer->addChild(group, 1);

    m_glow = CCSprite::create("glow.png"_spr);
    m_glow->setScale(0.62f);
    m_glow->setOpacity(0);
    m_glow->setColor(milestone ? ccColor3B{255, 226, 130} : ccColor3B{255, 150, 50});
    group->addChild(m_glow, 0);

    m_flame = CCSprite::create("flame.png"_spr);
    m_flame->setScale(0.32f);
    group->addChild(m_flame, 1);

    group->setScale(0.2f);
    group->runAction(CCEaseElasticOut::create(CCScaleTo::create(0.85f, 1.f), 0.45f));
    m_glow->runAction(CCSequence::create(
        CCDelayTime::create(0.15f),
        CCFadeTo::create(0.45f, milestone ? 190 : 110),
        nullptr
    ));

    m_countLabel = makeLabel(
        fmt::format("{}", data.current()),
        "goldFont.fnt", 1.f, milestone ? COLOR_GOLD : COLOR_PRIMARY, {0.f, 0.5f}
    );
    float baseScale = 1.3f;
    float rawWidth = std::max(1.f, m_countLabel->getContentSize().width);
    float fitScale = std::min(baseScale, COUNT_MAX_WIDTH / rawWidth);
    m_countLabel->setScale(fitScale);
    m_countLabel->setPosition({COUNT_X, centerY + 9.f});
    m_mainLayer->addChild(m_countLabel, 2);

    m_countLabel->setScale(fitScale * 0.1f);
    m_countLabel->runAction(CCSequence::create(
        CCDelayTime::create(0.1f),
        CCEaseElasticOut::create(CCScaleTo::create(0.8f, fitScale), 0.4f),
        nullptr
    ));

    auto caption = makeLabel("day streak", "chatFont.fnt", 0.62f, COLOR_MUTED, {0.f, 0.5f});
    caption->setPosition({COUNT_X + 1.f, centerY - 29.f});
    m_mainLayer->addChild(caption, 2);
    fadeIn(caption, 0.25f);

    if (auto line = makeRect(1.5f, 66.f, {255, 255, 255, 26})) {
        line->setPosition({230.f, centerY});
        m_mainLayer->addChild(line, 1);
    }
}

void StreakPopup::buildBest(int best) {
    auto label = makeLabel("best streak", "chatFont.fnt", 0.6f, COLOR_MUTED, {0.f, 0.5f});
    label->setPosition({BEST_LABEL_X, BEST_LABEL_Y});
    m_mainLayer->addChild(label, 1);
    fadeIn(label, 0.15f);

    auto value = makeLabel(
        fmt::format("{} {}", best, best == 1 ? "day" : "days"),
        "bigFont.fnt", 0.5f, COLOR_GOLD, {0.f, 0.5f}
    );
    value->setPosition({BEST_LABEL_X, BEST_VALUE_Y});
    m_mainLayer->addChild(value, 1);
    fadeIn(value, 0.22f);
}

void StreakPopup::buildCalendar(float centerY) {
    auto& data = StreakData::get();
    auto days = data.recentDays(7);
    if (days.empty()) return;

    auto totalWidth = static_cast<float>(days.size()) * CELL_SIZE +
        static_cast<float>(days.size() - 1) * CELL_GAP;
    auto startX = (POPUP_WIDTH - totalWidth) / 2.f + CELL_SIZE / 2.f;
    auto today = StreakData::today();

    for (std::size_t i = 0; i < days.size(); ++i) {
        auto day = days[i];
        bool active = data.isActiveOn(day);
        bool isToday = day == today;

        auto cell = CCNode::create();
        cell->setPosition({startX + static_cast<float>(i) * (CELL_SIZE + CELL_GAP), centerY});
        m_mainLayer->addChild(cell, 2);

        auto circle = CCSprite::create(active ? "circle-on.png"_spr : "circle-off.png"_spr);
        circle->setScale(CELL_SIZE / 128.f);
        cell->addChild(circle, 1);

        auto weekday = makeLabel(
            StreakData::weekdayInitial(day), "chatFont.fnt", 0.45f,
            isToday ? COLOR_GOLD : COLOR_MUTED
        );
        weekday->setPosition({0.f, CELL_SIZE / 2.f + 11.f});
        cell->addChild(weekday, 2);

        auto number = makeLabel(
            fmt::format("{}", StreakData::dayOfMonth(day)), "bigFont.fnt", 0.34f,
            active ? ccColor3B{255, 255, 255} : ccColor3B{140, 140, 140}
        );
        number->setPosition({0.f, -1.f});
        cell->addChild(number, 3);

        if (isToday) {
            if (auto ring = CCSprite::create("ring.png"_spr)) {
                ring->setScale((CELL_SIZE + 10.f) / 128.f);
                cell->addChild(ring, 0);
                ring->runAction(CCRepeatForever::create(CCSequence::create(
                    CCEaseSineInOut::create(CCFadeTo::create(1.f, 170)),
                    CCEaseSineInOut::create(CCFadeTo::create(1.f, 70)),
                    nullptr
                )));
            }
        }

        cell->setScale(0.05f);
        cell->runAction(CCSequence::create(
            CCDelayTime::create(0.1f + static_cast<float>(i) * 0.055f),
            CCEaseBackOut::create(CCScaleTo::create(0.4f, 1.f)),
            nullptr
        ));
    }
}

void StreakPopup::buildProgress(float labelY, float barY, int streak, int next) {
    auto caption = makeLabel(
        fmt::format("next milestone: {} days", next),
        "chatFont.fnt", 0.6f, COLOR_MUTED
    );
    caption->setPosition({POPUP_WIDTH / 2.f, labelY});
    m_mainLayer->addChild(caption, 1);
    fadeIn(caption, 0.2f);

    auto bg = NineSlice::create("bar.png"_spr, {}, {7.f, 7.f, 7.f, 7.f});
    bg->setContentSize({BAR_WIDTH, BAR_HEIGHT});
    bg->setPosition({POPUP_WIDTH / 2.f, barY});
    bg->setColor({40, 40, 46});
    m_mainLayer->addChild(bg, 1);

    float ratio = std::clamp(
        static_cast<float>(streak) / static_cast<float>(std::max(1, next)), 0.04f, 1.f
    );

    m_barFill = NineSlice::create("bar.png"_spr, {}, {7.f, 7.f, 7.f, 7.f});
    m_barFill->setContentSize({std::max(BAR_HEIGHT, BAR_WIDTH * ratio), BAR_HEIGHT});
    m_barFill->setAnchorPoint({0.f, 0.5f});
    m_barFill->setPosition({(POPUP_WIDTH - BAR_WIDTH) / 2.f, barY});
    m_barFill->setColor(COLOR_PRIMARY);
    m_mainLayer->addChild(m_barFill, 2);

    m_barFill->setScaleX(0.f);
    m_barFill->runAction(CCSequence::create(
        CCDelayTime::create(0.25f),
        CCEaseSineOut::create(CCScaleTo::create(0.7f, 1.f, 1.f)),
        nullptr
    ));

    auto value = makeLabel(
        fmt::format("{} / {}", streak, next), "bigFont.fnt", 0.3f, COLOR_MUTED
    );
    value->setPosition({POPUP_WIDTH / 2.f, BAR_VALUE_Y});
    m_mainLayer->addChild(value, 1);
    fadeIn(value, 0.3f);
}

void StreakPopup::buildFooter(float y) {
    auto& data = StreakData::get();

    std::string text = "come back every day to keep the flame alive";
    if (auto start = data.startDay()) {
        text = fmt::format("started {} - ", StreakData::formatDate(*start));
        if (auto last = data.lastDay()) {
            bool today = *last == StreakData::today();
            text += "last active " + (today ? std::string("today") : StreakData::formatDate(*last));
        }
    }

    auto hint = makeLabel(text, "chatFont.fnt", 0.5f, {132, 132, 132});
    hint->setPosition({POPUP_WIDTH / 2.f, y});
    m_mainLayer->addChild(hint, 1);
    fadeIn(hint, 0.35f);
}

void StreakPopup::startIdleAnimation() {
    if (m_flame) {
        m_flame->runAction(CCSequence::create(
            CCDelayTime::create(0.6f),
            CCRepeatForever::create(CCSequence::create(
                CCEaseSineInOut::create(CCScaleTo::create(1.f, 0.345f)),
                CCEaseSineInOut::create(CCScaleTo::create(1.f, 0.32f)),
                nullptr
            )),
            nullptr
        ));
    }
    if (m_glow) {
        m_glow->runAction(CCSequence::create(
            CCDelayTime::create(0.6f),
            CCRepeatForever::create(CCSequence::create(
                CCEaseSineInOut::create(CCFadeTo::create(1.3f, 170)),
                CCEaseSineInOut::create(CCFadeTo::create(1.3f, 90)),
                nullptr
            )),
            nullptr
        ));
    }
}

} // namespace gdstreaks
