// Host-only unit tests for the pure streak rules. No Geode SDK needed:
//   c++ -std=c++20 -O2 tests/streak_logic_test.cpp -o streak_tests && ./streak_tests

#include "../src/StreakLogic.hpp"

#include <cstdio>
#include <string>

using namespace gdstreaks;

namespace {

int failures = 0;

void expect(bool condition, char const* expr, int line) {
    if (!condition) {
        std::printf("FAIL line %d: %s\n", line, expr);
        ++failures;
    }
}

#define CHECK(cond) expect((cond), #cond, __LINE__)

void testDateMath() {
    CHECK(dateutil::daysFromCivil(1970, 1, 1) == 0);
    CHECK(dateutil::daysFromCivil(1970, 1, 2) == 1);
    CHECK(dateutil::daysFromCivil(2024, 10, 4) == 20000);

    CHECK(dateutil::formatDate(0) == "1 Jan 1970");
    CHECK(dateutil::formatDate(0, true) == "1 Jan");
    CHECK(dateutil::formatDate(20000) == "4 Oct 2024");

    // 1970-01-01 was a Thursday, 1970-01-04 a Sunday.
    CHECK(dateutil::weekdayInitial(0) == "T");
    CHECK(dateutil::weekdayInitial(3) == "S");
    CHECK(dateutil::weekdayInitial(20000) == "F");

    CHECK(dateutil::dayOfMonth(0) == 1);
    CHECK(dateutil::dayOfMonth(20000) == 4);
    CHECK(dateutil::dayOfMonth(19999) == 3);
}

void testMilestones() {
    CHECK(isStreakMilestone(7));
    CHECK(isStreakMilestone(14));
    CHECK(isStreakMilestone(30));
    CHECK(isStreakMilestone(50));
    CHECK(isStreakMilestone(100));
    CHECK(!isStreakMilestone(6));
    CHECK(!isStreakMilestone(8));

    CHECK(nextStreakMilestone(0) == 7);
    CHECK(nextStreakMilestone(1) == 7);
    CHECK(nextStreakMilestone(7) == 14);
    CHECK(nextStreakMilestone(14) == 30);
    CHECK(nextStreakMilestone(99) == 100);
    CHECK(nextStreakMilestone(100) == 200);
    CHECK(nextStreakMilestone(250) == 300);
}

void testFirstEver() {
    StreakState state;
    auto result = applyCheckIn(state, 1000, 5000.0);

    CHECK(result.kind == CheckInResult::Kind::FirstEver);
    CHECK(result.streak == 1);
    CHECK(state.current == 1);
    CHECK(state.best == 1);
    CHECK(state.startDay == 1000);
    CHECK(state.lastDay == 1000);
}

void testSameDayDoesNotCountTwice() {
    StreakState state;
    applyCheckIn(state, 1000, 5000.0);

    auto again = applyCheckIn(state, 1000, 6000.0);
    CHECK(again.kind == CheckInResult::Kind::NoChange);
    CHECK(again.streak == 1);
    CHECK(state.current == 1);
    CHECK(state.lastDay == 1000);
    CHECK(state.lastSeen == 6000.0);

    auto third = applyCheckIn(state, 1000, 9000.0);
    CHECK(third.kind == CheckInResult::Kind::NoChange);
    CHECK(state.current == 1);
}

void testConsecutiveDaysGrow() {
    StreakState state;
    applyCheckIn(state, 1000, 0.0);

    auto second = applyCheckIn(state, 1001, 86400.0);
    CHECK(second.kind == CheckInResult::Kind::Continued);
    CHECK(second.streak == 2);
    CHECK(second.previousStreak == 1);
    CHECK(state.best == 2);
    CHECK(state.startDay == 1000);

    auto third = applyCheckIn(state, 1002, 2 * 86400.0);
    CHECK(third.kind == CheckInResult::Kind::Continued);
    CHECK(third.streak == 3);
    CHECK(state.best == 3);
}

void testMissedDayResetsButKeepsBest() {
    StreakState state;
    // Build a 5 day streak.
    for (int i = 0; i < 5; ++i) {
        applyCheckIn(state, 1000 + i, 86400.0 * i);
    }
    CHECK(state.current == 5);
    CHECK(state.best == 5);

    // Skip day 6 entirely.
    auto result = applyCheckIn(state, 1007, 86400.0 * 7);
    CHECK(result.kind == CheckInResult::Kind::Reset);
    CHECK(result.streak == 1);
    CHECK(result.previousStreak == 5);
    CHECK(state.current == 1);
    CHECK(state.best == 5);
    CHECK(state.startDay == 1007);
}

void testTimezoneJumpForwardKeepsStreak() {
    double const start = 1700000000.0;  // a realistic wall clock value
    StreakState state;
    applyCheckIn(state, 1000, start);

    // Local date jumps by two but only 5 real hours passed: a flight east.
    auto result = applyCheckIn(state, 1002, start + 5 * 3600.0);
    CHECK(result.kind == CheckInResult::Kind::Continued);
    CHECK(state.current == 2);
}

void testTimezoneJumpForwardAfterLongAbsenceResets() {
    double const start = 1700000000.0;
    StreakState state;
    applyCheckIn(state, 1000, start);

    // Two local days later, but more than a day and a half of real time passed.
    auto result = applyCheckIn(state, 1002, start + 40 * 3600.0);
    CHECK(result.kind == CheckInResult::Kind::Reset);
    CHECK(state.current == 1);
}

void testDateMovingBackwardsIsHarmless() {
    StreakState state;
    applyCheckIn(state, 1005, 0.0);
    applyCheckIn(state, 1006, 86400.0);
    CHECK(state.current == 2);

    // Flew west and the local date went back a day.
    auto result = applyCheckIn(state, 1005, 90000.0);
    CHECK(result.kind == CheckInResult::Kind::NoChange);
    CHECK(state.current == 2);
    CHECK(state.lastDay == 1006);

    // The next real day still continues the streak.
    auto next = applyCheckIn(state, 1007, 172800.0);
    CHECK(next.kind == CheckInResult::Kind::Continued);
    CHECK(state.current == 3);
}

void testLongGapStartsNewStreak() {
    StreakState state;
    applyCheckIn(state, 1000, 0.0);
    auto result = applyCheckIn(state, 1400, 400 * 86400.0);
    CHECK(result.kind == CheckInResult::Kind::Reset);
    CHECK(state.current == 1);
    CHECK(state.best == 1);
}

void testCorruptStateIsRecovered() {
    StreakState state;
    state.current = 0;
    state.lastDay = -1;
    auto result = applyCheckIn(state, 42, 0.0);
    CHECK(result.kind == CheckInResult::Kind::FirstEver);
    CHECK(state.current == 1);
}

} // namespace

int main() {
    testDateMath();
    testMilestones();
    testFirstEver();
    testSameDayDoesNotCountTwice();
    testConsecutiveDaysGrow();
    testMissedDayResetsButKeepsBest();
    testTimezoneJumpForwardKeepsStreak();
    testTimezoneJumpForwardAfterLongAbsenceResets();
    testDateMovingBackwardsIsHarmless();
    testLongGapStartsNewStreak();
    testCorruptStateIsRecovered();

    if (failures == 0) {
        std::printf("All streak logic tests passed.\n");
        return 0;
    }
    std::printf("%d assertion(s) failed.\n", failures);
    return 1;
}
