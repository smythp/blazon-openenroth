#pragma once

#include <array>
#include <cstdint>
#include <ctime>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class GUIWindow;
class GUIWindow_CharacterRecord;

enum class BlazonWareAction {
    BLAZON_WARE_ACTION_BUY,
    BLAZON_WARE_ACTION_SELL,
    BLAZON_WARE_ACTION_IDENTIFY,
    BLAZON_WARE_ACTION_REPAIR,
    BLAZON_WARE_ACTION_ALREADY_IDENTIFIED,
    BLAZON_WARE_ACTION_NO_REPAIR_NEEDED,
    BLAZON_WARE_ACTION_UNAVAILABLE,
};

struct BlazonWare {
    std::string name;
    int price = 0;
    BlazonWareAction action = BlazonWareAction::BLAZON_WARE_ACTION_BUY;
    std::string spellName;
    std::string schoolName;
};

struct BlazonBookMember {
    std::string identityKind;
    std::string identityCode;
    std::string identityName;
    std::string text;
};

struct BlazonBookPage {
    std::string bookCode;
    std::string title;
    std::string emptyText;
    int totalMembers = 0;
    std::string totalMembersDisplay;
    int page = 0;
    int pageCount = 0;
    std::vector<BlazonBookMember> members;
};

/**
 * Blazon bridge for OpenEnroth.
 *
 * Inert unless the BLAZON_SOCKET environment variable names a Unix datagram
 * socket that a Blazon runtime is listening on. Emits evidence-bearing
 * semantic pieces (schema blazon.semantic-pieces/draft-1) exactly as the
 * ScummVM Xeen bridge does. It never speaks and never reads game input.
 *
 * Most scopes capture strings from the status bar and popup composers.
 * Party creation, portrait vitals and book pages instead emit typed identity
 * and values directly from game state and localization tables.
 */
class BlazonBridge {
 public:
    static BlazonBridge &instance();

    bool enabled() const { return _enabled; }

    /**
     * Per-frame observation of the permanent (hover) status-bar text. A change
     * of text ends the previous hover instance and begins a new one. An empty
     * text ends the current instance without beginning another.
     *
     * @param text                      Current permanent status-bar text.
     */
    void observeStatus(std::string_view text);

    /**
     * Observes changes to the status bar's timed event message, the channel
     * behind "Game Saved!", "Nothing here" and similar feedback. Each setter
     * call is a new event instance even when its text repeats.
     *
     * @param text                      Event text, empty when no event is showing.
     * @param stamp                     The event's expiry tick, zero when none is showing.
     * @param hook                      Status bar setter that produced the event.
     */
    void observeEvent(std::string_view text, int stamp, const char *hook);

    /**
     * Per-frame observation of the right-button hold. A rising edge begins a
     * popup instance, a falling edge ends it.
     *
     * @param holding                   Whether the right button is held this frame.
     */
    void observePopupHold(bool holding);

    /** Brackets the popup dispatcher so text drawn inside it is captured. */
    void beginPopupFrame();
    void endPopupFrame();

    /**
     * Text drawn by the engine. Inside a popup frame it becomes popup text,
     * everywhere else it is raw draw evidence for offline authoring, never
     * semantic authority.
     *
     * @param text                      Raw text with font control codes.
     * @param title                     Whether this came from DrawTitleText.
     * @param x                         Frame rectangle left, in render pixels.
     * @param y                         Frame rectangle top.
     * @param w                         Frame rectangle width.
     * @param h                         Frame rectangle height.
     */
    void captureText(std::string_view text, bool title, int x, int y, int w, int h);

    /**
     * NPC dialogue window lifecycle and content. The window observes once per
     * frame after it has composed its body and option labels.
     *
     * @param npcId                     Speaking NPC id, the subject.
     * @param name                      NPC name and title as drawn.
     * @param body                      Dialogue body with font codes.
     * @param options                   Option labels in screen order, exit button last.
     * @param highlighted               Index into options of the highlighted one, or -1.
     */
    /** Branchless message window: one body, no options, closes on any key. */
    void beginMessage();
    void observeMessage(std::string_view body);
    void endMessage();

    /**
     * Business screens (tavern, temple, bank, shops, guilds, training). One
     * bracket around the house dialog manager captures the heading, the body
     * and the option labels in draw order.
     *
     * @param houseId                   House identity, the subject.
     * @param houseName                 Building name from the game's house table, may be empty.
     */
    void beginHouseFrame(int houseId, std::string_view houseName);
    void endHouseFrame(std::string_view focusedOption);
    void endHouse();

    /**
     * Ordered wares visible in the current house submenu. The caller supplies
     * item identity and prices from the game tables and PriceCalculator.
     *
     * @param wares                     Wares in visual reading order.
     * @param hook                      House composer that supplied the wares.
     */
    void observeHouseWares(const std::vector<BlazonWare> &wares, const char *hook);

    /**
     * Ware resolved by the house UI's item rectangle hit test this frame.
     *
     * @param ware                      Focused ware and its current offer.
     * @param hook                      Item rectangle hit test that resolved it.
     */
    void observeHouseWare(const BlazonWare &ware, const char *hook);

    /**
     * Formats one normalized ware for speech.
     *
     * @param ware                      Ware identity and current offer.
     * @return                          Terse spoken line in UTF-8.
     */
    static std::string houseWareLine(const BlazonWare &ware);

    void beginDialogue(int npcId);
    void observeDialogue(int npcId, std::string_view name, std::string_view body,
                         const std::vector<std::string> &options, int highlighted);
    void endDialogue();

    /**
     * Party creation content and focus, observed once after the screen composes
     * a frame. Values come from character state and localization tables.
     *
     * @param window                    Party creation window and its controls.
     */
    void observePartyCreation(GUIWindow &window);
    void endPartyCreation();

    /**
     * Character record content and focus, observed after the current tab and
     * paper doll have been composed. Values come from character state and game
     * tables. The entry collection is emitted once per window lifetime.
     *
     * @param window                    Character record window and its controls.
     */
    void observeCharacterRecord(GUIWindow_CharacterRecord &window);
    void endCharacterRecord();

    /**
     * Current page of one book-family window. The first observation starts the
     * book lifetime and emits its entry announcement. Later page-number changes
     * also emit an automatic page-change occurrence.
     *
     * @param page                      Book identity, paging state and ordered members.
     * @param hook                      Book composer that supplied the state.
     */
    void observeBookPage(const BlazonBookPage &page, const char *hook);
    void endBook();

    /**
     * Joins ordered book members for the current Voice mapper.
     *
     * @param members                   Members in book reading order.
     * @param emptyText                 Line used when the page has no members.
     * @return                          Sentence-separated member text, or an empty-page line.
     */
    static std::string bookPageText(const std::vector<BlazonBookMember> &members,
                                    std::string_view emptyText);

    /**
     * Observes the party portrait under the pointer once per HUD frame. A
     * portrait entry begins one vitals instance. Leaving or entering another
     * portrait ends it. Character changes within an instance stay silent.
     *
     * @param window                    Window holding the portrait buttons.
     * @param visible                   Whether the portrait HUD is visible.
     */
    void observePortraitHover(const GUIWindow &window, bool visible);

    /**
     * Announces a character selected through a portrait button or its key.
     *
     * @param characterSlot             Zero-based character slot.
     */
    void observePortraitSelection(int characterSlot);

    /**
     * Explicit Blazon command from a key press.
     *
     * @param action                    "capture", "stop", "read_collection" or "mark".
     */
    void sendInput(const char *action);

    /**
     * Plain text of a font-formatted string, control codes removed.
     *
     * @param text                      UTF-8 or MM7 Windows-1252 text with \f, \t, \r, \n codes.
     * @return                          UTF-8 text without codes, positioning codes become spaces.
     */
    static std::string stripFontCodes(std::string_view text);

    /**
     * Labels spoken for an NPC dialogue's topic controls and final action.
     * A dialogue without topics is a message window whose image-backed action
     * reads "Close", while a conversation retains its localized exit label.
     *
     * @param topics                    Topic labels in screen order.
     * @param exitLabel                 Localized label for leaving a conversation.
     * @return                          Labels in spoken order, final action last.
     */
    static std::vector<std::string> dialogueOptionLabels(std::vector<std::string> topics,
                                                         std::string_view exitLabel);

 private:
    BlazonBridge();
    ~BlazonBridge();
    BlazonBridge(const BlazonBridge &) = delete;
    BlazonBridge &operator=(const BlazonBridge &) = delete;

    bool sendDatagram(const std::string &payload);
    bool sendTransaction(const std::string &operationsJson, bool resync = false);
    bool sendTransactionDatagram(const std::string &operationsJson, bool resync);
    bool sendResync();
    bool flushPendingEnds();
    void endLifetime(const char *lifetimeKind, const std::string &instance);
    void beginPointer(const std::string &text);
    void endPointer();
    void emitHouseFocus(std::string_view focusedOption);
    void finishHouseWaresFrame();
    void endHouseWares();
    void endHouseWareFocus();
    void emitPopup(const std::string &text);
    void endPopup();
    void emitRawDraw(std::string_view text, int x, int y, int w, int h);
    void beginEvent(const std::string &text, const char *hook);
    void endEvent();

    struct PartyCharacterState {
        std::string name;
        std::string race;
        std::string className;
        int face = -1;
        int voice = -1;
        std::array<int, 7> stats{};
        std::array<std::string, 4> skills{};

        bool operator==(const PartyCharacterState &) const = default;
    };

    struct PartyCreationState {
        std::array<PartyCharacterState, 4> characters{};
        int activeSlot = 0;
        int bonus = 0;

        bool operator==(const PartyCreationState &) const = default;
    };

    struct PartyCreationFocus {
        std::string key;
        std::string text;
    };

    struct CharacterRecordState {
        int characterSlot = -1;
        int tab = 0;
        std::string characterName;
        std::string tabName;
        std::string contents;

        bool operator==(const CharacterRecordState &) const = default;
    };

    struct CharacterRecordFocus {
        std::string key;
        std::string text;
    };

    PartyCreationState partyCreationState() const;
    PartyCreationFocus partyCreationPointerFocus(const GUIWindow &window, const PartyCreationState &state) const;
    PartyCreationFocus partyCreationKeyboardFocus(const GUIWindow &window, const PartyCreationState &state) const;
    std::string partyCreationChange(const PartyCreationState &before, const PartyCreationState &after) const;
    bool emitPartyCreationState(const PartyCreationState &state);
    bool emitPartyCreationEntry(const PartyCreationState &state);
    bool emitPartyCreationFocus(const PartyCreationFocus &focus);
    bool emitPartyCreationChange(const std::string &text);
    CharacterRecordState characterRecordState(const GUIWindow_CharacterRecord &window) const;
    CharacterRecordFocus characterRecordPointerFocus(const GUIWindow_CharacterRecord &window,
                                                       const CharacterRecordState &state) const;
    CharacterRecordFocus characterRecordKeyboardFocus(const GUIWindow_CharacterRecord &window,
                                                        const CharacterRecordState &state) const;
    bool emitCharacterRecordState(const CharacterRecordState &state);
    bool emitCharacterRecordEntry(const CharacterRecordState &state);
    bool emitCharacterRecordFocus(const CharacterRecordFocus &focus);
    bool emitBookEntry(const BlazonBookPage &page, const char *hook);
    bool emitBookPage(const BlazonBookPage &page, const char *hook,
                      const char *collectionSuffix, std::string *operations);
    bool emitPortraitVitals(int characterSlot, const std::string &instance,
                            const char *lifetimeKind, const char *collectionKind,
                            const char *hook, std::string *operations);
    void endPortraitHover();
    void endPortraitSelection();

    std::string _socketPath;
    std::string _sourceRun;
    int _socket = -1;
    bool _enabled = false;
    bool _warned = false;
    uint64_t _transactionSequence = 0;
    uint64_t _inputSequence = 0;
    uint64_t _instanceSequence = 0;
    std::vector<std::string> _pendingEnds;

    uint64_t _currentInstance = 0;
    std::string _currentText;
    std::string _currentOperations;

    bool _popupHolding = false;
    bool _inPopupFrame = false;
    uint64_t _popupInstance = 0;
    bool _popupEmitted = false;
    std::string _popupText;
    std::string _popupOperations;
    std::vector<std::string> _popupTitles;
    std::vector<std::string> _popupBody;

    uint64_t _eventInstance = 0;
    std::string _eventOperations;

    uint64_t _houseInstance = 0;
    int _houseId = -1;
    std::string _houseName;
    bool _inHouseFrame = false;
    bool _houseEmitted = false;
    std::string _houseKey;
    bool _houseFocusSeen = false;
    std::string _houseFocusText;
    std::string _houseOperations;
    std::string _houseFocusOperations;
    std::vector<std::string> _houseTitles;
    std::vector<std::string> _houseBody;

    uint64_t _houseWaresInstance = 0;
    uint64_t _houseWareFocusInstance = 0;
    bool _houseWaresSeenInFrame = false;
    bool _houseWareSeenInFrame = false;
    std::vector<BlazonWare> _houseFrameWares;
    BlazonWare _houseFrameWare;
    std::string _houseWaresHook;
    std::string _houseWareFocusHook;
    std::string _houseWaresKey;
    std::string _houseWareFocusKey;
    std::string _houseWaresOperations;
    std::string _houseWareFocusOperations;

    uint64_t _messageInstance = 0;
    bool _messageEmitted = false;
    std::string _messageText;
    std::string _messageOperations;

    uint64_t _dialogueInstance = 0;
    bool _dialogueEmitted = false;
    std::string _dialogueKey;
    bool _dialogueFocusSeen = false;
    std::string _dialogueFocusText;
    std::string _dialogueOperations;
    std::string _dialogueFocusOperations;

    uint64_t _partyCreationInstance = 0;
    uint64_t _partyCreationFocusInstance = 0;
    bool _partyCreationStateSeen = false;
    bool _partyCreationEntryEmitted = false;
    PartyCreationState _partyCreationState;
    std::string _partyCreationStateKey;
    std::string _partyCreationPointerKey;
    std::string _partyCreationKeyboardKey;
    std::string _partyCreationOperations;
    std::string _partyCreationEntryOperations;
    std::string _partyCreationFocusOperations;
    std::string _partyCreationChangeOperations;

    uint64_t _characterRecordInstance = 0;
    uint64_t _characterRecordFocusInstance = 0;
    bool _characterRecordEntryEmitted = false;
    CharacterRecordState _characterRecordState;
    std::string _characterRecordStateKey;
    std::string _characterRecordPointerKey;
    std::string _characterRecordKeyboardKey;
    std::string _characterRecordOperations;
    std::string _characterRecordEntryOperations;
    std::string _characterRecordFocusOperations;

    uint64_t _bookInstance = 0;
    bool _bookEntryEmitted = false;
    int _bookPage = -1;
    std::string _bookCode;
    std::string _bookPageKey;
    std::string _bookEntryOperations;
    std::string _bookPageOperations;
    std::string _bookPageChangeOperations;

    uint64_t _portraitHoverInstance = 0;
    int _portraitHoverSlot = -1;
    std::string _portraitHoverOperations;

    uint64_t _portraitSelectionInstance = 0;
    std::string _portraitSelectionOperations;

    void sendHeartbeat();

    uint64_t _frame = 0;
    uint64_t _rawSequence = 0;
    std::time_t _startedAt = 0;
    std::time_t _lastHeartbeat = 0;
    int _heartbeatSeconds = 5;
    std::unordered_map<std::string, uint64_t> _rawSeen;
};
