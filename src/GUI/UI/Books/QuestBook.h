#pragma once
#include <vector>

#include "GUI/UI/UIBooks.h"

struct GUIWindow_QuestBook : public GUIWindow_Book {
    GUIWindow_QuestBook();
    virtual ~GUIWindow_QuestBook() {}

    virtual void Update() override;

 private:
    int questsOnPage(int startingQuestIdx) const;

    int _startingQuestIdx = 0;
    int _currentPage = 0;
    int _currentPageQuests = 0;
    std::vector<QuestBit> _activeQuestsIdx;
    std::vector<int> _questsPerPage;
};
