#include <string>

#include "Testing/Unit/UnitTest.h"

#include "GUI/UI/UIBlazonCharacterRecord.h"
#include "GUI/UI/UICharacter.h"

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

UNIT_TEST(UIBlazonCharacterRecord, RingOverlayMapsEveryVisibleEquipmentSlot) {
    EXPECT_EQ(CharacterUI_RingOverlaySlotAt({493, 91}), ITEM_SLOT_AMULET);
    EXPECT_EQ(CharacterUI_RingOverlaySlotAt({586, 88}), ITEM_SLOT_GAUNTLETS);
    EXPECT_EQ(CharacterUI_RingOverlaySlotAt({0x1EA, 0x0CA}), ITEM_SLOT_RING1);
    EXPECT_EQ(CharacterUI_RingOverlaySlotAt({0x21A, 0x0CA}), ITEM_SLOT_RING2);
    EXPECT_EQ(CharacterUI_RingOverlaySlotAt({0x248, 0x0CA}), ITEM_SLOT_RING3);
    EXPECT_EQ(CharacterUI_RingOverlaySlotAt({0x1EA, 0x0FA}), ITEM_SLOT_RING4);
    EXPECT_EQ(CharacterUI_RingOverlaySlotAt({0x21A, 0x0FA}), ITEM_SLOT_RING5);
    EXPECT_EQ(CharacterUI_RingOverlaySlotAt({0x248, 0x0FA}), ITEM_SLOT_RING6);
}

UNIT_TEST(UIBlazonCharacterRecord, RingOverlayRejectsBackground) {
    EXPECT_EQ(CharacterUI_RingOverlaySlotAt({540, 170}), ITEM_SLOT_INVALID);
}
