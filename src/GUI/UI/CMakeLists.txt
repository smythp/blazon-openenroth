cmake_minimum_required(VERSION 3.27 FATAL_ERROR)

set(GUI_UI_SOURCES
        ItemGrid.cpp
        NPCTopics.cpp
        UISpellbook.cpp
        UIBooks.cpp
        UICharacter.cpp
        UIChest.cpp
        UICredits.cpp
        UIDialogue.cpp
        UIBranchlessDialogue.cpp
        UIGame.cpp
        UIGameOver.cpp
        UIHouses.cpp
        UIInventory.cpp
        UIMainMenu.cpp
        UIPartyCreation.cpp
        UIPopup.cpp
        UIQuickReference.cpp
        UIRest.cpp
        UISaveLoad.cpp
        UISpell.cpp
        UIStatusBar.cpp
        UIBlazon.cpp
        UITransition.cpp
        UIMessageScroll.cpp)

set(GUI_UI_HEADERS
        ItemGrid.h
        NPCTopics.h
        UISpellbook.h
        UIBooks.h
        UICharacter.h
        UIChest.h
        UICredits.h
        UIDialogue.h
        UIBranchlessDialogue.h
        UIGame.h
        UIGameOver.h
        UIHouseEnums.h
        UIHouses.h
        UIInventory.h
        UIMainMenu.h
        UIPartyCreation.h
        UIPopup.h
        UIQuickReference.h
        UIRest.h
        UISaveLoad.h
        UISpell.h
        UIStatusBar.h
        UIBlazon.h
        UIBlazonPartyCreation.h
        UIBlazonPortraits.h
        UITransition.h
        UIMessageScroll.h)

add_library(gui_ui STATIC ${GUI_UI_SOURCES} ${GUI_UI_HEADERS})
target_check_style(gui_ui)

target_link_libraries(gui_ui
        PUBLIC
        gui_ui_books
        gui_ui_houses
        arcomage
        core_serialization
        engine_spells
        library_json
        utility)

add_subdirectory(Books)
add_subdirectory(Houses)

if(OE_BUILD_TESTS)
    set(TEST_GUI_UI_SOURCES
            Tests/UIBlazon_ut.cpp
            Tests/UIBlazonPartyCreation_ut.cpp
            Tests/UIBlazonPortraits_ut.cpp)

    add_library(test_gui_ui OBJECT ${TEST_GUI_UI_SOURCES})
    target_link_libraries(test_gui_ui PUBLIC testing_unit gui_ui)

    target_check_style(test_gui_ui)

    target_link_libraries(OpenEnroth_UnitTest PUBLIC test_gui_ui)
endif()
