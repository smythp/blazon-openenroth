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
