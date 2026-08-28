#include <string>

#include "Testing/Unit/UnitTest.h"

#include "GUI/UI/UIBlazonPortraits.h"

UNIT_TEST(UIBlazonPortraits, NormalConditionIsOmitted) {
    BlazonPortraits::ConditionText text = BlazonPortraits::conditionText(CONDITION_GOOD, "Good");

    EXPECT_EQ(text.prefix, "");
    EXPECT_EQ(text.suffix, "");
}

UNIT_TEST(UIBlazonPortraits, IncapacitatingConditionsComeFirst) {
    for (Condition condition : {CONDITION_SLEEP, CONDITION_UNCONSCIOUS, CONDITION_DEAD, CONDITION_ERADICATED}) {
        BlazonPortraits::ConditionText text = BlazonPortraits::conditionText(condition, "condition");
        EXPECT_EQ(text.prefix, "condition, ");
        EXPECT_EQ(text.suffix, "");
    }
}

UNIT_TEST(UIBlazonPortraits, OtherConditionsComeLast) {
    for (Condition condition : {CONDITION_POISON_WEAK, CONDITION_PETRIFIED}) {
        BlazonPortraits::ConditionText text = BlazonPortraits::conditionText(condition, "condition");
        EXPECT_EQ(text.prefix, "");
        EXPECT_EQ(text.suffix, ", condition");
    }
}
