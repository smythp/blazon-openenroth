#include <string>
#include <utility>
#include <vector>

#include "Testing/Unit/UnitTest.h"

#include "GUI/UI/UIBlazon.h"

UNIT_TEST(UIBlazon, Windows1252ToUtf8) {
    std::string windows1252 = "daggers don\x92t\x96work";

    EXPECT_EQ(BlazonBridge::stripFontCodes(windows1252), "daggers don\xe2\x80\x99t\xe2\x80\x93work");
}

UNIT_TEST(UIBlazon, ValidUtf8PassesThrough) {
    std::string utf8 = "already UTF-8 \xe2\x80\x99 \xe2\x80\x93";

    EXPECT_EQ(BlazonBridge::stripFontCodes(utf8), utf8);
}

UNIT_TEST(UIBlazon, DialogueMessageHasOnlyClose) {
    EXPECT_EQ(BlazonBridge::dialogueOptionLabels({}, "Exit"), std::vector<std::string>({"Close"}));
}

UNIT_TEST(UIBlazon, DialogueConversationEndsWithExit) {
    std::vector<std::string> topics = {"More Information", "Hire"};

    EXPECT_EQ(BlazonBridge::dialogueOptionLabels(std::move(topics), "Exit"),
              std::vector<std::string>({"More Information", "Hire", "Exit"}));
}

UNIT_TEST(UIBlazon, BookPagePreservesMemberOrder) {
    std::vector<BlazonBookMember> members = {
        {.identityKind = "quest_bit", .identityCode = "75", .text = "First quest."},
        {.identityKind = "quest_bit", .identityCode = "157", .text = "Second quest."},
    };

    EXPECT_EQ(BlazonBridge::bookPageText(members, "No quests."), "First quest. Second quest.");
}

UNIT_TEST(UIBlazon, EmptyBookPageSaysSo) {
    EXPECT_EQ(BlazonBridge::bookPageText({}, "No current quests."), "No current quests.");
}

UNIT_TEST(UIBlazon, BuyWareUsesItemNameAndCurrentPrice) {
    BlazonWare ware = {.name = "Elven Saber", .price = 412};

    EXPECT_EQ(BlazonBridge::houseWareLine(ware), "Elven Saber, 412 gold.");
}

UNIT_TEST(UIBlazon, SpellbookAddsSpellAndSchoolIdentity) {
    BlazonWare ware = {
        .name = "Spirit Lash Spellbook",
        .price = 875,
        .spellName = "Spirit Lash",
        .schoolName = "Spirit",
    };

    EXPECT_EQ(BlazonBridge::houseWareLine(ware),
              "Spirit Lash Spellbook. Spirit Lash, Spirit Magic, 875 gold.");
}

UNIT_TEST(UIBlazon, InventoryServicesNameTheirOffer) {
    BlazonWare sell = {
        .name = "Crude Longsword",
        .price = 37,
        .action = BlazonWareAction::BLAZON_WARE_ACTION_SELL,
    };
    BlazonWare identify = {
        .name = "Unidentified Ring",
        .price = 50,
        .action = BlazonWareAction::BLAZON_WARE_ACTION_IDENTIFY,
    };
    BlazonWare repair = {
        .name = "Broken Mace",
        .price = 82,
        .action = BlazonWareAction::BLAZON_WARE_ACTION_REPAIR,
    };

    EXPECT_EQ(BlazonBridge::houseWareLine(sell), "Crude Longsword, 37 gold offered.");
    EXPECT_EQ(BlazonBridge::houseWareLine(identify), "Unidentified Ring, 50 gold to identify.");
    EXPECT_EQ(BlazonBridge::houseWareLine(repair), "Broken Mace, 82 gold to repair.");
}
