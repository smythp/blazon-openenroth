#pragma once

#include <string>
#include "GUI/GUIWindow.h"

struct CastSpellInfo;

class TargetedSpellUI : public GUIWindow {
 public:
    TargetedSpellUI(WindowType windowType, Pointi position, Sizei dimensions, CastSpellInfo *spellInfo, std::string_view hint = {});

    CastSpellInfo *spellInfo() const {
        return _spellInfo;
    }

    GUIButton *viewportButton() const {
        return _viewportButton;
    }

    void updateViewportButtons();
    void CreateButtonsTargetCharacters();

 protected:
    void createViewportButton(UIMessageType message);

 private:
    CastSpellInfo *_spellInfo = nullptr;
    GUIButton *_viewportButton = nullptr;
};

class TargetedSpellUI_Hirelings : public TargetedSpellUI {
 public:
    TargetedSpellUI_Hirelings(Pointi position, Sizei dimensions, CastSpellInfo *spellInfo, std::string_view hint = {});
};

class TargetedSpellUI_Character : public TargetedSpellUI {
 public:
    TargetedSpellUI_Character(Pointi position, Sizei dimensions, CastSpellInfo *spellInfo, std::string_view hint = {});
};

class TargetedSpellUI_Actor : public TargetedSpellUI {
 public:
    TargetedSpellUI_Actor(Pointi position, Sizei dimensions, CastSpellInfo *spellInfo, std::string_view hint = {});
};

class TargetedSpellUI_ActorOrCharacter : public TargetedSpellUI {
 public:
    TargetedSpellUI_ActorOrCharacter(Pointi position, Sizei dimensions, CastSpellInfo *spellInfo, std::string_view hint = {});
};

class TargetedSpellUI_Telekinesis : public TargetedSpellUI {
 public:
    TargetedSpellUI_Telekinesis(Pointi position, Sizei dimensions, CastSpellInfo *spellInfo, std::string_view hint = {});
};
