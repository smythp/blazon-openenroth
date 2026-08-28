#include <string>

#include "Testing/Unit/UnitTest.h"

#include "GUI/UI/UIBlazonPartyCreation.h"

UNIT_TEST(UIBlazonPartyCreation, StatSlot) {
    EXPECT_EQ(BlazonPartyCreation::statSlot(0), 0);
    EXPECT_EQ(BlazonPartyCreation::statSlot(2), 0);
    EXPECT_EQ(BlazonPartyCreation::statSlot(7), 1);
    EXPECT_EQ(BlazonPartyCreation::statSlot(19), 2);
    EXPECT_EQ(BlazonPartyCreation::statSlot(27), 3);
}

UNIT_TEST(UIBlazonPartyCreation, SkillRemovedBeforeNonePlaceholder) {
    std::array<std::string, 4> before = {"Sword", "Leather", "Bow", "Armsmaster"};
    std::array<std::string, 4> after = {"Sword", "Leather", "Bow", "None"};

    EXPECT_EQ(BlazonPartyCreation::skillChange(before, after, "None"), "Skill Armsmaster removed");
}

UNIT_TEST(UIBlazonPartyCreation, SkillChosenAfterNonePlaceholder) {
    std::array<std::string, 4> before = {"Sword", "Leather", "Bow", "None"};
    std::array<std::string, 4> after = {"Sword", "Leather", "Bow", "Repair Item"};

    EXPECT_EQ(BlazonPartyCreation::skillChange(before, after, "None"), "Skill Repair Item chosen");
}
