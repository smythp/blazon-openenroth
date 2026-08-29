#include <memory>
#include <string>

#include "Engine/AssetsManager.h"
#include "Engine/Localization.h"
#include "Engine/Party.h"

#include "Engine/Graphics/Renderer/Renderer.h"
#include "Engine/Graphics/Viewport.h"
#include "Engine/Graphics/Image.h"
#include "Engine/mm7_data.h"

#include "Engine/Tables/QuestTable.h"

#include "GUI/GUIButton.h"
#include "GUI/UI/UIGame.h"
#include "GUI/UI/UIBlazon.h"
#include "GUI/UI/Books/QuestBook.h"

#include "Media/Audio/AudioPlayer.h"

GraphicsImage *ui_book_quests_background = nullptr;

static std::string_view questBitName(QuestBit questBit) {
    switch (questBit) {
      case QBIT_EMERALD_ISLAND_RED_POTION_ACTIVE: return "QBIT_EMERALD_ISLAND_RED_POTION_ACTIVE";
      case QBIT_EMERALD_ISLAND_SEASHELL_ACTIVE: return "QBIT_EMERALD_ISLAND_SEASHELL_ACTIVE";
      case QBIT_EMERALD_ISLAND_LONGBOW_ACTIVE: return "QBIT_EMERALD_ISLAND_LONGBOW_ACTIVE";
      case QBIT_EMERALD_ISLAND_PLATE_ACTIVE: return "QBIT_EMERALD_ISLAND_PLATE_ACTIVE";
      case QBIT_EMERALD_ISLAND_LUTE_ACTIVE: return "QBIT_EMERALD_ISLAND_LUTE_ACTIVE";
      case QBIT_EMERALD_ISLAND_HAT_ACTIVE: return "QBIT_EMERALD_ISLAND_HAT_ACTIVE";
      case QBIT_EMERALD_ISLAND_MARGARETH_OFF: return "QBIT_EMERALD_ISLAND_MARGARETH_OFF";
      case QBIT_EVENMORN_MAP_FOUND: return "QBIT_EVENMORN_MAP_FOUND";
      case QBIT_HARMONDALE_REBUILT: return "QBIT_HARMONDALE_REBUILT";
      case QBIT_LIGHT_PATH: return "QBIT_LIGHT_PATH";
      case QBIT_DARK_PATH: return "QBIT_DARK_PATH";
      case QBIT_110: return "QBIT_110";
      case QBIT_114: return "QBIT_114";
      case QBIT_120: return "QBIT_120";
      case QBIT_123: return "QBIT_123";
      case QBIT_ESCAPED_EMERALD_ISLE: return "QBIT_ESCAPED_EMERALD_ISLE";
      case QBIT_OBELISK_IN_HARMONDALE_FOUND: return "QBIT_OBELISK_IN_HARMONDALE_FOUND";
      case QBIT_OBELISK_IN_ERATHIA_FOUND: return "QBIT_OBELISK_IN_ERATHIA_FOUND";
      case QBIT_OBELISK_IN_TULAREAN_FOREST_FOUND: return "QBIT_OBELISK_IN_TULAREAN_FOREST_FOUND";
      case QBIT_OBELISK_IN_DEYJA_FOUND: return "QBIT_OBELISK_IN_DEYJA_FOUND";
      case QBIT_OBELISK_IN_BRACADA_DESERT_FOUND: return "QBIT_OBELISK_IN_BRACADA_DESERT_FOUND";
      case QBIT_OBELISK_IN_CELESTE_FOUND: return "QBIT_OBELISK_IN_CELESTE_FOUND";
      case QBIT_OBELISK_IN_THE_PIT_FOUND: return "QBIT_OBELISK_IN_THE_PIT_FOUND";
      case QBIT_OBELISK_IN_EVENMORN_ISLAND_FOUND: return "QBIT_OBELISK_IN_EVENMORN_ISLAND_FOUND";
      case QBIT_OBELISK_IN_MOUNT_NIGHON_FOUND: return "QBIT_OBELISK_IN_MOUNT_NIGHON_FOUND";
      case QBIT_OBELISK_IN_BARROW_DOWNS_FOUND: return "QBIT_OBELISK_IN_BARROW_DOWNS_FOUND";
      case QBIT_OBELISK_IN_LAND_OF_THE_GIANTS_FOUND: return "QBIT_OBELISK_IN_LAND_OF_THE_GIANTS_FOUND";
      case QBIT_OBELISK_IN_TATALIA_FOUND: return "QBIT_OBELISK_IN_TATALIA_FOUND";
      case QBIT_OBELISK_IN_AVLEE_FOUND: return "QBIT_OBELISK_IN_AVLEE_FOUND";
      case QBIT_OBELISK_IN_STONE_CITY_FOUND: return "QBIT_OBELISK_IN_STONE_CITY_FOUND";
      case QBIT_OBELISK_TREASURE_FOUND: return "QBIT_OBELISK_TREASURE_FOUND";
      case QBIT_SPLITTER_FOUND: return "QBIT_SPLITTER_FOUND";
      case QBIT_REMOVE_FEAR_FOUND: return "QBIT_REMOVE_FEAR_FOUND";
      case QBIT_FOUNTAIN_IN_HARMONDALE_ACTIVATED: return "QBIT_FOUNTAIN_IN_HARMONDALE_ACTIVATED";
      case QBIT_FOUNTAIN_IN_STEADWICK_ACTIVATED: return "QBIT_FOUNTAIN_IN_STEADWICK_ACTIVATED";
      case QBIT_FOUNTAIN_IN_PIERPONT_ACTIVATED: return "QBIT_FOUNTAIN_IN_PIERPONT_ACTIVATED";
      case QBIT_FOUNTAIN_IN_CELESTIA_ACTIVATED: return "QBIT_FOUNTAIN_IN_CELESTIA_ACTIVATED";
      case QBIT_FOUNTAIN_IN_THE_PIT_ACTIVATED: return "QBIT_FOUNTAIN_IN_THE_PIT_ACTIVATED";
      case QBIT_FOUNTAIN_IN_MOUNT_NIGHON_ACTIVATED: return "QBIT_FOUNTAIN_IN_MOUNT_NIGHON_ACTIVATED";
      case QBIT_212: return "QBIT_212";
      case QBIT_213: return "QBIT_213";
      case QBIT_214: return "QBIT_214";
      case QBIT_215: return "QBIT_215";
      case QBIT_216: return "QBIT_216";
      case QBIT_217: return "QBIT_217";
      case QBIT_218: return "QBIT_218";
      case QBIT_219: return "QBIT_219";
      case QBIT_220: return "QBIT_220";
      case QBIT_221: return "QBIT_221";
      case QBIT_222: return "QBIT_222";
      case QBIT_223: return "QBIT_223";
      case QBIT_224: return "QBIT_224";
      case QBIT_225: return "QBIT_225";
      case QBIT_226: return "QBIT_226";
      case QBIT_227: return "QBIT_227";
      case QBIT_228: return "QBIT_228";
      case QBIT_229: return "QBIT_229";
      case QBIT_230: return "QBIT_230";
      case QBIT_231: return "QBIT_231";
      case QBIT_232: return "QBIT_232";
      case QBIT_233: return "QBIT_233";
      case QBIT_234: return "QBIT_234";
      case QBIT_235: return "QBIT_235";
      case QBIT_236: return "QBIT_236";
      case QBIT_237: return "QBIT_237";
      case QBIT_ARCOMAGE_CHAMPION: return "QBIT_ARCOMAGE_CHAMPION";
      case QBIT_DIVINE_INTERVENTION_RETRIEVED: return "QBIT_DIVINE_INTERVENTION_RETRIEVED";
      case QBIT_241: return "QBIT_241";
      case QBIT_LAST: return "QBIT_LAST";
      default: return {};
    }
}

GUIWindow_QuestBook::GUIWindow_QuestBook() {
    this->eWindowType = WindowType::WINDOW_QuestBook;

    pChildBooksOverlay = std::make_unique<GUIWindow_BooksButtonOverlay>(Pointi{493, 355}, Sizei{0, 0}, pBtn_Quests);
    bFlashQuestBook = false;

    ui_book_quests_background = assets->getImage_Solid("sbquiknot");
    ui_book_quest_div_bar = assets->getImage_Alpha("divbar");

    ui_book_button1_on = assets->getImage_Alpha("tab-an-6b");
    ui_book_button2_on = assets->getImage_Alpha("tab-an-7b");
    ui_book_button1_off = assets->getImage_Alpha("tab-an-6a");
    ui_book_button2_off = assets->getImage_Alpha("tab-an-7a");

    pBtn_Book_1 = CreateButton(pViewport.topLeft() + Pointi(398, 1), ui_book_button1_on->size(), BUTTON_TYPE_NORMAL, 0,
                               UIMSG_ClickBooksBtn, std::to_underlying(BOOK_PREV_PAGE), INPUT_ACTION_DIALOG_LEFT, localization->str(LSTR_SCROLL_UP), {ui_book_button1_on});
    pBtn_Book_2 = CreateButton(pViewport.topLeft() + Pointi(398, 38), ui_book_button2_on->size(), BUTTON_TYPE_NORMAL, 0,
                               UIMSG_ClickBooksBtn, std::to_underlying(BOOK_NEXT_PAGE), INPUT_ACTION_DIALOG_RIGHT, localization->str(LSTR_SCROLL_DOWN), {ui_book_button2_on});

    for (auto i : pQuestTable.indices()) {
        if (pParty->_questBits[i] && !pQuestTable[i].empty()) {
            _activeQuestsIdx.push_back(i);
        }
    }

    for (int startingQuestIdx = 0; startingQuestIdx < static_cast<int>(_activeQuestsIdx.size());) {
        int count = questsOnPage(startingQuestIdx);
        _questsPerPage.push_back(count);
        startingQuestIdx += count;
    }
    if (_questsPerPage.empty())
        _questsPerPage.push_back(0);
}

int GUIWindow_QuestBook::questsOnPage(int startingQuestIdx) const {
    Recti questbookWindow(48, 70, 360, 264);
    int count = 0;
    for (int i = startingQuestIdx; i < static_cast<int>(_activeQuestsIdx.size()); ++i) {
        ++count;
        int textHeight = assets->pFontBookOnlyShadow->CalcTextHeight(
            pQuestTable[_activeQuestsIdx[i]], questbookWindow.w, 1);
        if (questbookWindow.y + textHeight > questbookWindow.h)
            break;
        questbookWindow.y += textHeight + 24;
    }
    return count;
}

void GUIWindow_QuestBook::Update() {
    render->DrawQuad2D(ui_exit_cancel_button_background, {471, 445});

    int pTextHeight;
    render->DrawQuad2D(ui_book_quests_background, pViewport.topLeft());

    if ((_bookButtonClicked && _bookButtonAction == BOOK_PREV_PAGE) || !_startingQuestIdx) {
        render->DrawQuad2D(ui_book_button1_off, pViewport.topLeft() + Pointi(407, 2));
    } else {
        render->DrawQuad2D(ui_book_button1_on, pViewport.topLeft() + Pointi(398, 1));
    }

    if ((_bookButtonClicked && _bookButtonAction == BOOK_NEXT_PAGE) || (_startingQuestIdx + _currentPageQuests) >= _activeQuestsIdx.size()) {
        render->DrawQuad2D(ui_book_button2_off, pViewport.topLeft() + Pointi(407, 38));
    } else {
        render->DrawQuad2D(ui_book_button2_on, pViewport.topLeft() + Pointi(398, 38));
    }

    // for title
    DrawTitleText(assets->pFontBookTitle.get(), 0, 22, ui_book_quests_title_color, localization->str(LSTR_CURRENT_QUESTS), 3, pViewport);

    // for other text
    Recti questbook_window(48, 70, 360, 264);

    if (_bookButtonClicked == BOOK_BUTTON_PRESSED_FRAMES && _bookButtonAction == BOOK_NEXT_PAGE &&
        (_currentPage + 1) < static_cast<int>(_questsPerPage.size())) {
        pAudioPlayer->playUISound(SOUND_openbook);
        _startingQuestIdx += _questsPerPage[_currentPage];
        _currentPage++;
    }

    if (_bookButtonClicked == BOOK_BUTTON_PRESSED_FRAMES && _bookButtonAction == BOOK_PREV_PAGE && _startingQuestIdx) {
        pAudioPlayer->playUISound(SOUND_openbook);
        _currentPage--;
        _startingQuestIdx -= _questsPerPage[_currentPage];
    }

    if (_bookButtonClicked)
        _bookButtonClicked--;

    _currentPageQuests = _questsPerPage[_currentPage];

    for (int i = _startingQuestIdx; i < _startingQuestIdx + _currentPageQuests; ++i) {
        DrawText(assets->pFontBookOnlyShadow.get(), {1, 0}, ui_book_quests_text_color, pQuestTable[_activeQuestsIdx[i]], questbook_window);
        pTextHeight = assets->pFontBookOnlyShadow->CalcTextHeight(pQuestTable[_activeQuestsIdx[i]], questbook_window.w, 1);
        if ((questbook_window.y + pTextHeight) > questbook_window.h) {
            break;
        }

        render->DrawQuad2D(ui_book_quest_div_bar, {100, (questbook_window.y + pTextHeight) + 12});
        questbook_window.y = (questbook_window.y + pTextHeight) + 24;
    }

    BlazonBookPage page = {
        .bookCode = "quest",
        .title = localization->str(LSTR_CURRENT_QUESTS),
        .emptyText = "No current quests.",
        .totalMembers = static_cast<int>(_activeQuestsIdx.size()),
        .page = _currentPage,
        .pageCount = static_cast<int>(_questsPerPage.size()),
    };
    for (int i = _startingQuestIdx; i < _startingQuestIdx + _currentPageQuests; ++i) {
        QuestBit questBit = _activeQuestsIdx[i];
        page.members.push_back({
            .identityKind = "quest_bit",
            .identityCode = std::to_string(std::to_underlying(questBit)),
            .identityName = std::string(questBitName(questBit)),
            .text = pQuestTable[questBit],
        });
    }
    BlazonBridge::instance().observeBookPage(page, "GUIWindow_QuestBook::Update");
}
