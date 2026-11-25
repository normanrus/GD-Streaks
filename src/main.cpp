#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>

#include "StreakBadge.hpp"
#include "StreakData.hpp"
#include "StreakNotifier.hpp"

using namespace geode::prelude;

class $modify(StreakMenuLayer, MenuLayer) {
    struct Fields {
        gdstreaks::CheckInResult result;
    };

    bool init() {
        if (!MenuLayer::init()) {
            return false;
        }

        // The first launch of a new day is what counts as activity.
        auto result = gdstreaks::StreakData::get().checkIn();
        m_fields->result = result;

        this->addStreakBadge(result);

        if (result.kind != gdstreaks::CheckInResult::Kind::NoChange) {
            // Let the menu settle before sliding the notification in.
            this->scheduleOnce(schedule_selector(StreakMenuLayer::showStreakNotification), 0.7f);
        }

        return true;
    }

    void addStreakBadge(gdstreaks::CheckInResult const& result) {
        auto badge = gdstreaks::StreakBadge::create();
        if (!badge) {
            return;
        }

        // A null selected sprite keeps the badge itself out of the item's
        // show/hide toggling, which would otherwise hide it after a click.
        auto item = CCMenuItemSpriteExtra::create(
            badge, nullptr, badge, menu_selector(gdstreaks::StreakBadge::onOpen)
        );
        item->setID("streak-badge"_spr);
        item->setVisible(!Mod::get()->getSettingValue<bool>("hide-button"));

        if (auto corner = typeinfo_cast<CCMenu*>(this->getChildByID("top-right-menu"))) {
            corner->addChild(item);
            corner->updateLayout();
        } else {
            // Geode normally provides this menu; keep working if it is missing.
            auto winSize = CCDirector::get()->getWinSize();
            item->setPosition({winSize.width - 55.f, winSize.height - 26.f});
            this->addChild(item, 100);
        }

        if (result.kind != gdstreaks::CheckInResult::Kind::NoChange) {
            badge->celebrate();
        }
    }

    void showStreakNotification(float) {
        gdstreaks::notifyStreakChange(m_fields->result);
    }
};
