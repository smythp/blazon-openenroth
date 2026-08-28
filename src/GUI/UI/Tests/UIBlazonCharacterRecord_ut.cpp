#include <string>

#include "Testing/Unit/UnitTest.h"

#include "GUI/UI/UIBlazonCharacterRecord.h"

UNIT_TEST(UIBlazonCharacterRecord, UnmodifiedValueOmitsDuplicateBase) {
    EXPECT_EQ(BlazonCharacterRecord::currentAndBase("Might", 30, 30), "Might 30");
}

UNIT_TEST(UIBlazonCharacterRecord, ModifiedValueIncludesBase) {
    EXPECT_EQ(BlazonCharacterRecord::currentAndBase("Might", 30, 25), "Might 30 of 25");
}

UNIT_TEST(UIBlazonCharacterRecord, ResourceValueAlwaysIncludesMaximum) {
    EXPECT_EQ(BlazonCharacterRecord::currentAndMaximum("Hit Points", 7, 30), "Hit Points 7 of 30");
}

UNIT_TEST(UIBlazonCharacterRecord, SkillIncludesLevelAndMastery) {
    EXPECT_EQ(BlazonCharacterRecord::skill("Sword", 4, "Expert"), "Sword 4 Expert");
}
