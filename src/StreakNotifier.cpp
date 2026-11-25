#include "StreakNotifier.hpp"

using namespace geode::prelude;

namespace gdstreaks {
namespace {

std::string milestoneMessage(int streak) {
    switch (streak) {
        case 7: return "One week streak! The flame is burning.";
        case 14: return "Two weeks in a row! Unstoppable.";
        case 30: return "30 day streak! A whole month of dedication.";
        case 50: return "50 day streak! Halfway to triple digits.";
        case 100: return "100 day streak! Absolute legend.";
        default: return fmt::format("{} day streak! You're on fire.", streak);
    }
}

} // namespace

void notifyStreakChange(CheckInResult const& result) {
    if (result.kind == CheckInResult::Kind::NoChange) return;
    if (!Mod::get()->getSettingValue<bool>("notifications")) return;

    std::string text;
    float time = 2.6f;

    switch (result.kind) {
        case CheckInResult::Kind::FirstEver:
            text = "Daily streak started! Come back tomorrow to keep it going.";
            time = 3.f;
            break;

        case CheckInResult::Kind::Reset:
            text = "Streak reset after a missed day. Day 1 starts now!";
            time = 3.2f;
            break;

        case CheckInResult::Kind::Continued:
        default:
            if (StreakData::isMilestone(result.streak)) {
                text = milestoneMessage(result.streak);
                time = 4.f;
            } else {
                text = fmt::format("{} day streak! Keep it up.", result.streak);
            }
            break;
    }

    auto icon = CCSprite::create("flame.png"_spr);
    if (icon) {
        icon->setScale(0.11f);
        Notification::create(std::string(text), icon, time)->show();
    } else {
        Notification::create(std::string(text), NotificationIcon::Success, time)->show();
    }
}

} // namespace gdstreaks
