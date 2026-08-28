#pragma once

#include <cstdint>
#include <ctime>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

/**
 * Blazon bridge for OpenEnroth.
 *
 * Inert unless the BLAZON_SOCKET environment variable names a Unix datagram
 * socket that a Blazon runtime is listening on. Emits evidence-bearing
 * semantic pieces (schema blazon.semantic-pieces/draft-1) exactly as the
 * ScummVM Xeen bridge does. It never speaks and never reads game input.
 *
 * Scopes so far: the text under the pointer, observed once per frame at the
 * status bar, and the right-click popup, captured from the strings the popup
 * draws while the button is held. Both are string chokepoints. Identity from
 * the hover resolver and the popup composers comes next.
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

    void beginDialogue(int npcId);
    void observeDialogue(int npcId, std::string_view name, std::string_view body,
                         const std::vector<std::string> &options, int highlighted);
    void endDialogue();

    /**
     * Explicit Blazon command from a key press.
     *
     * @param action                    "capture", "stop", "read_collection" or "mark".
     */
    void sendInput(const char *action);

    /**
     * Plain text of a font-formatted string, control codes removed.
     *
     * @param text                      Text with \f, \t, \r, \n codes.
     * @return                          Text without codes, positioning codes become spaces.
     */
    static std::string stripFontCodes(std::string_view text);

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
    void emitPopup(const std::string &text);
    void endPopup();
    void emitRawDraw(std::string_view text, int x, int y, int w, int h);
    void beginEvent(const std::string &text, const char *hook);
    void endEvent();

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

    void sendHeartbeat();

    uint64_t _frame = 0;
    uint64_t _rawSequence = 0;
    std::time_t _startedAt = 0;
    std::time_t _lastHeartbeat = 0;
    int _heartbeatSeconds = 5;
    std::unordered_map<std::string, uint64_t> _rawSeen;
};
