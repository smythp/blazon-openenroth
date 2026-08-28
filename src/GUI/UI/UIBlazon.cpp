#include "UIBlazon.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "GUI/GUIWindow.h"

#include "Library/Logger/Logger.h"

namespace {

using Json = nlohmann::json;

constexpr const char *kSchema = "blazon.semantic-pieces/draft-1";
constexpr const char *kBuild = "8673e3e7+blazon-pointer";
constexpr const char *kPointerCollectionKind = "mm7.under_pointer.state";
constexpr const char *kPopupCollectionKind = "mm7.popup.state";
constexpr const char *kEventCollectionKind = "mm7.status_event.state";
constexpr const char *kHouseCollectionKind = "mm7.house.state";
constexpr const char *kHouseFocusCollectionKind = "mm7.house_focus.state";
constexpr const char *kMessageCollectionKind = "mm7.message.state";
constexpr const char *kDialogueCollectionKind = "mm7.dialogue.state";
constexpr const char *kDialogueFocusCollectionKind = "mm7.dialogue_focus.state";
constexpr const char *kSubjectId = "mm7/pointer";
constexpr const char *kGameSubjectId = "mm7/game";

Json makeLifetime(const char *kind, const std::string &id) {
    return Json{{"kind", kind}, {"id", id}};
}

Json makeProvenance(const char *hook) {
    return Json{{"source", "OpenEnroth"}, {"build", kBuild}, {"hook", hook}};
}

Json makePieceBase(const std::string &id, const char *kind, const Json &lifetime, const char *origin, const char *hook) {
    return Json{
        {"id", id},
        {"kind", kind},
        {"lifetime", lifetime},
        {"origin", origin},
        {"authority", "game"},
        {"currency", "current"},
        {"provenance", makeProvenance(hook)},
    };
}

Json makeField(const std::string &instance, const std::string &collection, const char *lifetimeKind,
               const char *subjectId, const char *key, const char *origin, const char *hook, Json value) {
    Json field = makePieceBase(instance + "/field/" + key, "field", makeLifetime(lifetimeKind, instance), origin, hook);
    field["key"] = key;
    field["authority_scope"] = "collection";
    field["relationships"] = Json{{"collection", collection}, {"subject", subjectId}};
    field["value"] = std::move(value);
    return field;
}

Json makeSubject(const std::string &sourceRun, const std::string &subjectId, const char *subjectKind,
                 const char *origin, const char *hook, const std::string &name) {
    Json subject = makePieceBase(subjectId, subjectKind, makeLifetime("game_session", sourceRun), origin, hook);
    subject["value"] = Json{{"type", "text"}, {"text", name}};
    return subject;
}

Json makeTextField(const std::string &instance, const std::string &collection, const char *lifetimeKind,
                   const std::string &subjectId, const char *key, const char *origin, const char *hook,
                   const std::string &text) {
    return makeField(instance, collection, lifetimeKind, subjectId.c_str(), key, origin, hook,
                     Json{{"type", "text"}, {"text", text}, {"display", text}});
}

// A complete collection of text fields under one subject, as one replace with the subject upserted first.
std::string makeCollection(const std::string &sourceRun, const std::string &instance, const std::string &collection,
                           const char *lifetimeKind, const char *collectionKind, const std::string &subjectId,
                           const char *subjectKind, const char *subjectOrigin, const std::string &subjectName,
                           const char *hook, Json fields) {
    Json upsert = Json{{"op", "upsert"},
                       {"pieces", Json::array({makeSubject(sourceRun, subjectId, subjectKind, subjectOrigin, hook, subjectName)})}};
    Json replace = Json{
        {"op", "replace"},
        {"collection", Json{
            {"id", collection},
            {"kind", collectionKind},
            {"complete", true},
            {"subject", subjectId},
            {"lifetime", makeLifetime(lifetimeKind, instance)},
        }},
        {"pieces", std::move(fields)},
    };
    return Json::array({upsert, replace}).dump(-1, ' ', false, Json::error_handler_t::replace);
}

// One complete text collection: the subject plus a text field and a kind field.
std::string makeTextCollection(const std::string &sourceRun, const std::string &instance, const char *lifetimeKind,
                               const char *collectionKind, const char *subjectId, const char *subjectKind,
                               const char *subjectOrigin, const char *origin, const char *hook,
                               const std::string &text, const char *kindCode) {
    std::string collection = instance + "/state";
    Json upsert = Json{{"op", "upsert"},
                       {"pieces", Json::array({makeSubject(sourceRun, subjectId, subjectKind, subjectOrigin, hook, subjectKind)})}};
    Json fields = Json::array();
    fields.push_back(makeField(instance, collection, lifetimeKind, subjectId, "text", origin, hook,
                               Json{{"type", "text"}, {"text", text}, {"display", text}}));
    fields.push_back(makeField(instance, collection, lifetimeKind, subjectId, "kind", origin, hook,
                               Json{{"type", "enum"}, {"code", kindCode}, {"display", kindCode}}));
    Json replace = Json{
        {"op", "replace"},
        {"collection", Json{
            {"id", collection},
            {"kind", collectionKind},
            {"complete", true},
            {"subject", subjectId},
            {"lifetime", makeLifetime(lifetimeKind, instance)},
        }},
        {"pieces", fields},
    };
    // MM7 strings are cp1252, so anything outside UTF-8 becomes U+FFFD rather than an exception.
    return Json::array({upsert, replace}).dump(-1, ' ', false, Json::error_handler_t::replace);
}

std::string makeEnd(const char *lifetimeKind, const std::string &instance) {
    Json end = Json{{"op", "end_lifetime"}, {"lifetime", makeLifetime(lifetimeKind, instance)}};
    return Json::array({end}).dump();
}

void appendSentence(std::string &out, const std::string &part) {
    if (part.empty())
        return;
    if (!out.empty()) {
        char last = out.back();
        out += (last == '.' || last == '!' || last == '?' || last == ':' || last == ',') ? " " : ". ";
    }
    out += part;
}

}  // namespace

BlazonBridge &BlazonBridge::instance() {
    static BlazonBridge bridge;
    return bridge;
}

BlazonBridge::BlazonBridge() {
    const char *path = std::getenv("BLAZON_SOCKET");
    if (path == nullptr || *path == '\0')
        return;
    _socketPath = path;
    if (_socketPath.size() >= sizeof(sockaddr_un::sun_path)) {
        MM_LOG(LOG_WARNING, "Blazon: socket path is too long, bridge disabled: {}", _socketPath);
        return;
    }

    std::time_t now = std::time(nullptr);
    std::tm utc{};
    gmtime_r(&now, &utc);
    char stamp[32];
    std::strftime(stamp, sizeof(stamp), "%Y%m%dT%H%M%S", &utc);
    _sourceRun = std::string("openenroth-mm7:") + stamp + ":pid" + std::to_string(getpid());
    _startedAt = now;
    // BLAZON_HEARTBEAT_SECONDS=0 silences the liveness pings once the loop is stable.
    if (const char *heartbeat = std::getenv("BLAZON_HEARTBEAT_SECONDS"))
        _heartbeatSeconds = std::atoi(heartbeat);
    _enabled = true;
    MM_LOG(LOG_INFO, "Blazon: bridge enabled, socket {}, source run {}, heartbeat {}s", _socketPath, _sourceRun,
           _heartbeatSeconds);
}

void BlazonBridge::sendHeartbeat() {
    std::time_t now = std::time(nullptr);
    if (_heartbeatSeconds <= 0 || now - _lastHeartbeat < _heartbeatSeconds)
        return;
    _lastHeartbeat = now;
    Json heartbeat = Json{
        {"type", "heartbeat"},
        {"source_run", _sourceRun},
        {"uptime_seconds", static_cast<long long>(now - _startedAt)},
        {"frame", _frame},
        {"screen", static_cast<int>(current_screen_type)},
        {"transactions_sent", _transactionSequence},
        {"raw_draws_sent", _rawSequence},
        {"inputs_sent", _inputSequence},
    };
    sendDatagram(heartbeat.dump());
}

BlazonBridge::~BlazonBridge() {
    if (_socket >= 0)
        close(_socket);
}

std::string BlazonBridge::stripFontCodes(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        switch (c) {
        case '\f': // Color tag, five digits.
            i += 5;
            break;
        case '\t': // Cell offset, three digits.
        case '\r': // Right-justify offset, three digits.
            i += 3;
            if (!result.empty() && result.back() != ' ')
                result += ' ';
            break;
        case '\n':
            if (!result.empty() && result.back() != ' ')
                result += ' ';
            break;
        default:
            result += c;
            break;
        }
    }
    while (!result.empty() && result.back() == ' ')
        result.pop_back();
    size_t start = result.find_first_not_of(' ');
    return start == std::string::npos ? std::string() : result.substr(start);
}

void BlazonBridge::observeStatus(std::string_view text) {
    if (!_enabled)
        return;
    if (text == _currentText)
        return;
    if (_currentInstance != 0)
        endPointer();
    _currentText.assign(text.data(), text.size());
    if (!_currentText.empty())
        beginPointer(_currentText);
}

void BlazonBridge::beginPointer(const std::string &text) {
    uint64_t number = ++_instanceSequence;
    std::string instance = "mm7/under-pointer/" + std::to_string(number);
    std::string operations = makeTextCollection(_sourceRun, instance, "hover_instance", kPointerCollectionKind,
                                                kSubjectId, "pointer", "Io::Mouse",
                                                "StatusBar::_statusString", "StatusBar::draw", text, "status");
    if (sendTransaction(operations))
        _currentInstance = number;
}

void BlazonBridge::endPointer() {
    std::string instance = "mm7/under-pointer/" + std::to_string(_currentInstance);
    sendTransaction(makeEnd("hover_instance", instance));
    _currentInstance = 0;
}

void BlazonBridge::observeEvent(std::string_view text, int stamp) {
    if (!_enabled)
        return;
    if (stamp == _eventStamp)
        return;
    _eventStamp = stamp;
    if (_eventInstance != 0)
        endEvent();
    if (stamp != 0 && !text.empty())
        beginEvent(std::string(text));
}

void BlazonBridge::beginEvent(const std::string &text) {
    uint64_t number = ++_instanceSequence;
    std::string instance = "mm7/status-event/" + std::to_string(number);
    std::string operations = makeTextCollection(_sourceRun, instance, "event_instance", kEventCollectionKind,
                                                kGameSubjectId, "game", "Engine",
                                                "StatusBar::_eventStatusString", "StatusBar::draw", text, "event");
    if (sendTransaction(operations))
        _eventInstance = number;
}

void BlazonBridge::endEvent() {
    std::string instance = "mm7/status-event/" + std::to_string(_eventInstance);
    sendTransaction(makeEnd("event_instance", instance));
    _eventInstance = 0;
}

void BlazonBridge::beginMessage() {
    if (!_enabled)
        return;
    if (_messageInstance != 0)
        endMessage();
    _messageInstance = ++_instanceSequence;
    _messageEmitted = false;
    _messageText.clear();
}

void BlazonBridge::observeMessage(std::string_view body) {
    if (!_enabled || _messageInstance == 0)
        return;
    std::string plain = stripFontCodes(body);
    if (plain.empty() || plain == _messageText)
        return;
    std::string instance = "mm7/message/" + std::to_string(_messageInstance);
    std::string collection = instance + "/state";
    Json fields = Json::array();
    fields.push_back(makeTextField(instance, collection, "message_instance", kGameSubjectId, "body",
                                   "branchless_dialogue_str", "GUIWindow_BranchlessDialogue::Update", plain));
    if (sendTransaction(makeCollection(_sourceRun, instance, collection, "message_instance", kMessageCollectionKind,
                                       kGameSubjectId, "game", "Engine", "game", "GUIWindow_BranchlessDialogue::Update",
                                       std::move(fields)))) {
        _messageEmitted = true;
        _messageText = plain;
    }
}

void BlazonBridge::endMessage() {
    if (!_enabled || _messageInstance == 0)
        return;
    if (_messageEmitted)
        sendTransaction(makeEnd("message_instance", "mm7/message/" + std::to_string(_messageInstance)));
    _messageInstance = 0;
    _messageEmitted = false;
}

void BlazonBridge::beginHouseFrame(int houseId, std::string_view houseName) {
    if (!_enabled)
        return;
    _houseName = stripFontCodes(houseName);
    if (_houseInstance == 0 || houseId != _houseId) {
        if (_houseInstance != 0)
            endHouse();
        _houseInstance = ++_instanceSequence;
        _houseId = houseId;
        _houseEmitted = false;
        _houseKey.clear();
        _houseFocusSeen = false;
        _houseFocusText.clear();
    }
    _inHouseFrame = true;
    _houseTitles.clear();
    _houseBody.clear();
}

void BlazonBridge::endHouseFrame(std::string_view focusedOption) {
    if (!_inHouseFrame)
        return;
    _inHouseFrame = false;
    if (_houseTitles.empty())
        return;
    // houseDialogManager draws the building name first when the house table has one, then the
    // proprietor, then the options. Matching the name rather than the position keeps a nameless
    // house (and a map transition, which draws neither) from reading its proprietor as an option.
    std::vector<std::string> titles;
    for (const std::string &title : _houseTitles) {
        if (title != _houseName)
            titles.push_back(title);
    }
    std::string heading = _houseName;
    size_t firstOption = 0;
    if (!titles.empty()) {
        appendSentence(heading, titles.front());
        firstOption = 1;
    }
    if (heading.empty())
        return;
    std::string options;
    for (size_t i = firstOption; i < titles.size(); ++i) {
        if (!options.empty())
            options += ", ";
        options += titles[i];
    }
    std::string body;
    for (const std::string &part : _houseBody)
        appendSentence(body, part);

    // Compose from what the screen actually has. The heading rides along only on
    // the first read of a house, so stepping into a submenu says what the submenu
    // says rather than the proprietor's name again, and a screen with no message
    // reads as a bare option list rather than announcing that it has no message.
    std::string spoken;
    if (!_houseEmitted)
        spoken = heading;
    appendSentence(spoken, body);
    if (!options.empty())
        appendSentence(spoken, spoken.empty() ? options : "Options: " + options);
    if (spoken.empty())
        return;

    std::string key = heading + "\x1f" + body + "\x1f" + options;
    if (key == _houseKey) {
        emitHouseFocus(focusedOption);
        return;
    }
    std::string instance = "mm7/house/" + std::to_string(_houseInstance);
    std::string collection = instance + "/state";
    std::string subjectId = "mm7/house/id/" + std::to_string(_houseId);
    Json fields = Json::array();
    fields.push_back(makeTextField(instance, collection, "house_instance", subjectId, "say",
                                   "composed from the heading, message and options actually present",
                                   "GUIWindow_House::Update", spoken));
    if (!body.empty())
        fields.push_back(makeTextField(instance, collection, "house_instance", subjectId, "body",
                                       "house greeting or response", "GUIWindow_House::Update", body));
    if (!options.empty())
        fields.push_back(makeTextField(instance, collection, "house_instance", subjectId, "options",
                                       "house dialogue option labels", "GUIWindow_House::Update", options));
    if (sendTransaction(makeCollection(_sourceRun, instance, collection, "house_instance", kHouseCollectionKind,
                                       subjectId, "house", "GUIWindow_House::houseId", heading,
                                       "GUIWindow_House::Update", std::move(fields)))) {
        _houseEmitted = true;
        _houseKey = key;
    }
    emitHouseFocus(focusedOption);
}

void BlazonBridge::emitHouseFocus(std::string_view focusedOption) {
    std::string focus = stripFontCodes(focusedOption);
    // The first observation is the screen's opening state, not a move onto an option.
    if (!_houseFocusSeen) {
        _houseFocusSeen = true;
        _houseFocusText = focus;
        return;
    }
    if (focus.empty() || focus == _houseFocusText)
        return;
    std::string instance = "mm7/house/" + std::to_string(_houseInstance);
    std::string collection = instance + "/focus";
    std::string subjectId = "mm7/house/id/" + std::to_string(_houseId);
    Json fields = Json::array();
    fields.push_back(makeTextField(instance, collection, "house_instance", subjectId, "option",
                                   "GUIWindow::pCurrentPosActiveItem", "GUIWindow_House::Update", focus));
    if (sendTransaction(makeCollection(_sourceRun, instance, collection, "house_instance",
                                       kHouseFocusCollectionKind, subjectId, "house",
                                       "GUIWindow_House::houseId", _houseName.empty() ? "house" : _houseName,
                                       "GUIWindow_House::Update", std::move(fields))))
        _houseFocusText = focus;
}

void BlazonBridge::endHouse() {
    if (!_enabled || _houseInstance == 0)
        return;
    if (_houseEmitted)
        sendTransaction(makeEnd("house_instance", "mm7/house/" + std::to_string(_houseInstance)));
    _houseInstance = 0;
    _houseId = -1;
    _houseEmitted = false;
    _houseKey.clear();
}

void BlazonBridge::beginDialogue(int npcId) {
    if (!_enabled)
        return;
    if (_dialogueInstance != 0)
        endDialogue();
    _dialogueInstance = ++_instanceSequence;
    _dialogueEmitted = false;
    _dialogueKey.clear();
    _dialogueFocusSeen = false;
    _dialogueFocusText.clear();
}

void BlazonBridge::observeDialogue(int npcId, std::string_view name, std::string_view body,
                                   const std::vector<std::string> &options, int highlighted) {
    if (!_enabled || _dialogueInstance == 0)
        return;
    std::string instance = "mm7/dialogue/" + std::to_string(_dialogueInstance);
    std::string subjectId = "mm7/npc/" + std::to_string(npcId);
    std::string subjectName = stripFontCodes(name);
    std::string plainBody = stripFontCodes(body);
    std::string optionText;
    for (const std::string &option : options) {
        std::string plain = stripFontCodes(option);
        if (plain.empty())
            continue;
        if (!optionText.empty())
            optionText += ", ";
        optionText += plain;
    }
    if (plainBody.empty())
        plainBody = "No message";
    if (optionText.empty())
        optionText = "none";

    std::string key = subjectName + "\x1f" + plainBody + "\x1f" + optionText;
    if (key != _dialogueKey) {
        std::string collection = instance + "/state";
        Json fields = Json::array();
        fields.push_back(makeTextField(instance, collection, "dialogue_instance", subjectId, "body",
                                       "GUIWindow_Dialogue::Update dialogue_string", "GUIWindow_Dialogue::Update", plainBody));
        fields.push_back(makeTextField(instance, collection, "dialogue_instance", subjectId, "options",
                                       "GUIButton::label per dialogue option", "GUIWindow_Dialogue::Update", optionText));
        if (sendTransaction(makeCollection(_sourceRun, instance, collection, "dialogue_instance", kDialogueCollectionKind,
                                           subjectId, "npc", "speakingNpcId", subjectName, "GUIWindow_Dialogue::Update",
                                           std::move(fields)))) {
            _dialogueEmitted = true;
            _dialogueKey = key;
        }
    }

    // The highlight follows the pointer. The first observation is the opening state and is not spoken on its own.
    std::string focus = highlighted >= 0 && highlighted < static_cast<int>(options.size())
                        ? stripFontCodes(options[highlighted]) : std::string();
    if (!_dialogueFocusSeen) {
        _dialogueFocusSeen = true;
        _dialogueFocusText = focus;
        return;
    }
    if (focus.empty() || focus == _dialogueFocusText)
        return;
    std::string collection = instance + "/focus";
    Json fields = Json::array();
    fields.push_back(makeTextField(instance, collection, "dialogue_instance", subjectId, "option",
                                   "GUIWindow::pCurrentPosActiveItem", "GUIWindow_Dialogue::Update", focus));
    if (sendTransaction(makeCollection(_sourceRun, instance, collection, "dialogue_instance", kDialogueFocusCollectionKind,
                                       subjectId, "npc", "speakingNpcId", subjectName, "GUIWindow_Dialogue::Update",
                                       std::move(fields))))
        _dialogueFocusText = focus;
}

void BlazonBridge::endDialogue() {
    if (!_enabled || _dialogueInstance == 0)
        return;
    if (_dialogueEmitted)
        sendTransaction(makeEnd("dialogue_instance", "mm7/dialogue/" + std::to_string(_dialogueInstance)));
    _dialogueInstance = 0;
    _dialogueEmitted = false;
}

void BlazonBridge::observePopupHold(bool holding) {
    if (!_enabled)
        return;
    ++_frame;
    sendHeartbeat();
    if (holding && !_popupHolding) {
        _popupHolding = true;
        _popupInstance = ++_instanceSequence;
        _popupEmitted = false;
        _popupText.clear();
    } else if (!holding && _popupHolding) {
        if (_popupEmitted)
            endPopup();
        _popupHolding = false;
        _popupInstance = 0;
        _popupText.clear();
    }
}

void BlazonBridge::beginPopupFrame() {
    if (!_enabled)
        return;
    _inPopupFrame = true;
    _popupTitles.clear();
    _popupBody.clear();
}

void BlazonBridge::captureText(std::string_view text, bool title, int x, int y, int w, int h) {
    if (!_enabled)
        return;
    if (!_inPopupFrame && !_inHouseFrame) {
        emitRawDraw(text, x, y, w, h);
        return;
    }
    std::string plain = stripFontCodes(text);
    if (plain.empty())
        return;
    if (_inPopupFrame) {
        (title ? _popupTitles : _popupBody).push_back(std::move(plain));
        return;
    }
    (title ? _houseTitles : _houseBody).push_back(std::move(plain));
}

void BlazonBridge::emitRawDraw(std::string_view text, int x, int y, int w, int h) {
    if (text.empty())
        return;
    // Screens repaint the same strings every frame, so a text seen within the last half second is not resent.
    std::string key(text);
    auto seen = _rawSeen.find(key);
    if (seen != _rawSeen.end() && _frame - seen->second < 30) {
        seen->second = _frame;
        return;
    }
    if (_rawSeen.size() > 4096)
        _rawSeen.clear();
    _rawSeen[key] = _frame;

    uint64_t sequence = _rawSequence + 1;
    Json event = Json{
        {"type", "raw_draw"},
        {"source_run", _sourceRun},
        {"mode", static_cast<int>(current_screen_type)},
        {"window", -1},
        {"bounds", Json::array({x, y, x + w, y + h})},
        {"text", key},
        {"draw_sequence", sequence},
    };
    if (sendDatagram(event.dump(-1, ' ', false, Json::error_handler_t::replace)))
        _rawSequence = sequence;
}

void BlazonBridge::endPopupFrame() {
    if (!_inPopupFrame)
        return;
    _inPopupFrame = false;
    if (!_popupHolding)
        return;
    // Item popups draw their title after the body, so titles are read first regardless of draw order.
    std::string text;
    for (const std::string &part : _popupTitles)
        appendSentence(text, part);
    for (const std::string &part : _popupBody)
        appendSentence(text, part);
    if (text.empty() || text == _popupText)
        return;
    emitPopup(text);
}

void BlazonBridge::emitPopup(const std::string &text) {
    std::string instance = "mm7/popup/" + std::to_string(_popupInstance);
    std::string operations = makeTextCollection(_sourceRun, instance, "popup_instance", kPopupCollectionKind,
                                                kSubjectId, "pointer", "Io::Mouse",
                                                "GUIWindow::DrawText", "UI_OnMouseRightClick", text, "unknown");
    if (sendTransaction(operations)) {
        _popupEmitted = true;
        _popupText = text;
    }
}

void BlazonBridge::endPopup() {
    std::string instance = "mm7/popup/" + std::to_string(_popupInstance);
    sendTransaction(makeEnd("popup_instance", instance));
    _popupEmitted = false;
}

void BlazonBridge::sendInput(const char *action) {
    if (!_enabled)
        return;
    uint64_t sequence = _inputSequence + 1;
    Json input = Json{
        {"type", "input"},
        {"action", action},
        {"source_run", _sourceRun},
        {"input_id", _sourceRun + ":input:" + std::to_string(sequence)},
    };
    if (sendDatagram(input.dump()))
        _inputSequence = sequence;
}

bool BlazonBridge::sendTransaction(const std::string &operationsJson) {
    // The sequence is reserved only when the datagram leaves, so a dropped send never opens a gap in the run.
    uint64_t sequence = _transactionSequence + 1;
    Json transaction = Json{
        {"type", "transaction"},
        {"schema", kSchema},
        {"source_run", _sourceRun},
        {"sequence", sequence},
        {"transaction_id", _sourceRun + ":transaction:" + std::to_string(sequence)},
        {"operations", Json::parse(operationsJson)},
    };
    if (!sendDatagram(transaction.dump(-1, ' ', false, Json::error_handler_t::replace)))
        return false;
    _transactionSequence = sequence;
    return true;
}

bool BlazonBridge::sendDatagram(const std::string &payload) {
    if (_socket < 0) {
        _socket = socket(AF_UNIX, SOCK_DGRAM, 0);
        if (_socket < 0)
            return false;
        int flags = fcntl(_socket, F_GETFL, 0);
        if (flags >= 0)
            fcntl(_socket, F_SETFL, flags | O_NONBLOCK);
    }
    // Unconnected sends address the path every time, so a runtime that restarts
    // and rebinds the same path keeps receiving without the game noticing.
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    std::strncpy(address.sun_path, _socketPath.c_str(), sizeof(address.sun_path) - 1);
    ssize_t sent = sendto(_socket, payload.data(), payload.size(), MSG_DONTWAIT,
                          reinterpret_cast<const sockaddr *>(&address), sizeof(address));
    if (sent != static_cast<ssize_t>(payload.size())) {
        if (!_warned) {
            MM_LOG(LOG_WARNING, "Blazon: datagram to {} failed: {}", _socketPath, std::strerror(errno));
            _warned = true;
        }
        return false;
    }
    _warned = false;
    return true;
}
