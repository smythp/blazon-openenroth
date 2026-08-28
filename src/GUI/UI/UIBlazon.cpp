#include "UIBlazon.h"
#include "UIBlazonPartyCreation.h"
#include "UIBlazonPortraits.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "Engine/EngineIocContainer.h"
#include "Engine/Localization.h"
#include "Engine/Objects/Character.h"
#include "Engine/Party.h"

#include "GUI/GUIButton.h"
#include "GUI/GUIWindow.h"

#include "Io/Mouse.h"

#include "Library/Logger/Logger.h"
#include "Utility/String/Encoding.h"
#include "Utility/String/Format.h"

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
constexpr const char *kPartyCreationStateCollectionKind = "mm7.party_creation.character.state";
constexpr const char *kPartyCreationEntryCollectionKind = "mm7.party_creation.entry.state";
constexpr const char *kPartyCreationFocusCollectionKind = "mm7.party_creation.focus.state";
constexpr const char *kPartyCreationChangeCollectionKind = "mm7.party_creation.change.state";
constexpr const char *kPortraitVitalsCollectionKind = "mm7.portrait_vitals.state";
constexpr const char *kPortraitSelectionVitalsCollectionKind = "mm7.portrait_selection_vitals.state";
constexpr const char *kSubjectId = "mm7/pointer";
constexpr const char *kGameSubjectId = "mm7/game";
constexpr const char *kPartyCreationSubjectId = "mm7/party-creation";

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

Json makeIntegerField(const std::string &instance, const std::string &collection, const char *lifetimeKind,
                      const std::string &subjectId, const char *key, const char *origin, const char *hook,
                      const std::string &label, int value) {
    Json field = makeField(instance, collection, lifetimeKind, subjectId.c_str(), key, origin, hook,
                           Json{{"type", "integer"}, {"integer", value}, {"display", std::to_string(value)}});
    field["label"] = Json{{"text", label}, {"origin", "runtime_resource"}};
    return field;
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
    return Json::array({upsert, replace}).dump(-1, ' ', false, Json::error_handler_t::replace);
}

std::string makeEnd(const char *lifetimeKind, const std::string &instance) {
    Json end = Json{{"op", "end_lifetime"}, {"lifetime", makeLifetime(lifetimeKind, instance)}};
    return Json::array({end}).dump();
}

void appendOperations(Json &out, const std::string &operationsJson) {
    if (operationsJson.empty())
        return;
    for (Json &operation : Json::parse(operationsJson))
        out.push_back(std::move(operation));
}

uint64_t makeRunToken() {
    try {
        std::random_device randomDevice;
        std::uniform_int_distribution<uint64_t> tokenDistribution;
        return tokenDistribution(randomDevice);
    } catch (const std::exception &) {
        uint64_t clock = static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
        return clock ^ (static_cast<uint64_t>(getpid()) << 32);
    }
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

bool isValidUtf8(std::string_view text) {
    for (size_t i = 0; i < text.size();) {
        unsigned char first = static_cast<unsigned char>(text[i]);
        if (first <= 0x7f) {
            ++i;
            continue;
        }

        size_t length;
        unsigned char secondMin = 0x80;
        unsigned char secondMax = 0xbf;
        if (first >= 0xc2 && first <= 0xdf) {
            length = 2;
        } else if (first >= 0xe0 && first <= 0xef) {
            length = 3;
            if (first == 0xe0)
                secondMin = 0xa0;
            else if (first == 0xed)
                secondMax = 0x9f;
        } else if (first >= 0xf0 && first <= 0xf4) {
            length = 4;
            if (first == 0xf0)
                secondMin = 0x90;
            else if (first == 0xf4)
                secondMax = 0x8f;
        } else {
            return false;
        }

        if (i + length > text.size())
            return false;
        unsigned char second = static_cast<unsigned char>(text[i + 1]);
        if (second < secondMin || second > secondMax)
            return false;
        for (size_t offset = 2; offset < length; ++offset) {
            unsigned char continuation = static_cast<unsigned char>(text[i + offset]);
            if (continuation < 0x80 || continuation > 0xbf)
                return false;
        }
        i += length;
    }
    return true;
}

std::string gameTextToUtf8(std::string_view text) {
    if (isValidUtf8(text))
        return std::string(text);
    return txt::encodedToUtf8(text, ENCODING_WINDOWS_1252);
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
    std::string token = fmt::format("{:016x}", makeRunToken());
    _sourceRun = std::string("openenroth-mm7:") + stamp + ":pid" + std::to_string(getpid()) + ":random" + token;
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
    flushPendingEnds();
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
    return gameTextToUtf8(start == std::string::npos ? std::string_view() : std::string_view(result).substr(start));
}

void BlazonBridge::observeStatus(std::string_view text) {
    if (!_enabled)
        return;
    std::string utf8 = _portraitHoverSlot >= 0 ? std::string() : gameTextToUtf8(text);
    if (utf8 == _currentText && (_currentInstance != 0 || utf8.empty()))
        return;
    if (_currentInstance != 0)
        endPointer();
    _currentText = std::move(utf8);
    if (!_currentText.empty())
        beginPointer(_currentText);
}

void BlazonBridge::beginPointer(const std::string &text) {
    uint64_t number = ++_instanceSequence;
    std::string instance = "mm7/under-pointer/" + std::to_string(number);
    std::string operations = makeTextCollection(_sourceRun, instance, "hover_instance", kPointerCollectionKind,
                                                kSubjectId, "pointer", "Io::Mouse",
                                                "StatusBar::_statusString", "StatusBar::draw", text, "status");
    if (sendTransaction(operations)) {
        _currentInstance = number;
        _currentOperations = std::move(operations);
    }
}

void BlazonBridge::endPointer() {
    std::string instance = "mm7/under-pointer/" + std::to_string(_currentInstance);
    endLifetime("hover_instance", instance);
    _currentInstance = 0;
    _currentOperations.clear();
}

void BlazonBridge::observeEvent(std::string_view text, int stamp, const char *hook) {
    if (!_enabled)
        return;
    if (_eventInstance != 0)
        endEvent();
    if (stamp != 0 && !text.empty())
        beginEvent(gameTextToUtf8(text), hook);
}

void BlazonBridge::beginEvent(const std::string &text, const char *hook) {
    uint64_t number = ++_instanceSequence;
    std::string instance = "mm7/status-event/" + std::to_string(number);
    std::string operations = makeTextCollection(_sourceRun, instance, "event_instance", kEventCollectionKind,
                                                kGameSubjectId, "game", "Engine",
                                                "StatusBar::_eventStatusString", hook, text, "event");
    if (sendTransaction(operations)) {
        _eventInstance = number;
        _eventOperations = std::move(operations);
    }
}

void BlazonBridge::endEvent() {
    std::string instance = "mm7/status-event/" + std::to_string(_eventInstance);
    endLifetime("event_instance", instance);
    _eventInstance = 0;
    _eventOperations.clear();
}

void BlazonBridge::beginMessage() {
    if (!_enabled)
        return;
    if (_messageInstance != 0)
        endMessage();
    _messageInstance = ++_instanceSequence;
    _messageEmitted = false;
    _messageText.clear();
    _messageOperations.clear();
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
    std::string operations = makeCollection(_sourceRun, instance, collection, "message_instance", kMessageCollectionKind,
                                            kGameSubjectId, "game", "Engine", "game",
                                            "GUIWindow_BranchlessDialogue::Update", std::move(fields));
    if (sendTransaction(operations)) {
        _messageEmitted = true;
        _messageText = plain;
        _messageOperations = std::move(operations);
    }
}

void BlazonBridge::endMessage() {
    if (!_enabled || _messageInstance == 0)
        return;
    endLifetime("message_instance", "mm7/message/" + std::to_string(_messageInstance));
    _messageInstance = 0;
    _messageEmitted = false;
    _messageOperations.clear();
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
        _houseOperations.clear();
        _houseFocusOperations.clear();
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
    std::string operations = makeCollection(_sourceRun, instance, collection, "house_instance", kHouseCollectionKind,
                                            subjectId, "house", "GUIWindow_House::houseId", heading,
                                            "GUIWindow_House::Update", std::move(fields));
    if (sendTransaction(operations)) {
        _houseEmitted = true;
        _houseKey = key;
        _houseOperations = std::move(operations);
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
    std::string operations = makeCollection(_sourceRun, instance, collection, "house_instance",
                                            kHouseFocusCollectionKind, subjectId, "house",
                                            "GUIWindow_House::houseId", _houseName.empty() ? "house" : _houseName,
                                            "GUIWindow_House::Update", std::move(fields));
    if (sendTransaction(operations)) {
        _houseFocusText = focus;
        _houseFocusOperations = std::move(operations);
    }
}

void BlazonBridge::endHouse() {
    if (!_enabled || _houseInstance == 0)
        return;
    endLifetime("house_instance", "mm7/house/" + std::to_string(_houseInstance));
    _houseInstance = 0;
    _houseId = -1;
    _houseEmitted = false;
    _houseKey.clear();
    _houseOperations.clear();
    _houseFocusOperations.clear();
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
    _dialogueOperations.clear();
    _dialogueFocusOperations.clear();
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
        std::string operations = makeCollection(_sourceRun, instance, collection, "dialogue_instance",
                                                kDialogueCollectionKind, subjectId, "npc", "speakingNpcId", subjectName,
                                                "GUIWindow_Dialogue::Update", std::move(fields));
        if (sendTransaction(operations)) {
            _dialogueEmitted = true;
            _dialogueKey = key;
            _dialogueOperations = std::move(operations);
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
    std::string operations = makeCollection(_sourceRun, instance, collection, "dialogue_instance",
                                            kDialogueFocusCollectionKind, subjectId, "npc", "speakingNpcId",
                                            subjectName, "GUIWindow_Dialogue::Update", std::move(fields));
    if (sendTransaction(operations)) {
        _dialogueFocusText = focus;
        _dialogueFocusOperations = std::move(operations);
    }
}

void BlazonBridge::endDialogue() {
    if (!_enabled || _dialogueInstance == 0)
        return;
    endLifetime("dialogue_instance", "mm7/dialogue/" + std::to_string(_dialogueInstance));
    _dialogueInstance = 0;
    _dialogueEmitted = false;
    _dialogueOperations.clear();
    _dialogueFocusOperations.clear();
}

BlazonBridge::PartyCreationState BlazonBridge::partyCreationState() const {
    PartyCreationState result;
    for (int slot = 0; slot < result.characters.size(); ++slot) {
        Character &character = pParty->pCharacters[slot];
        PartyCharacterState &out = result.characters[slot];
        out.name = gameTextToUtf8(character.name);
        out.race = gameTextToUtf8(character.GetRaceName());
        out.className = gameTextToUtf8(localization->expand(localization->className(character.classType)));
        out.face = character.uCurrentFace;
        out.voice = character.uVoiceID;
        out.stats = {
            character.GetActualMight(),
            character.GetActualIntelligence(),
            character.GetActualPersonality(),
            character.GetActualEndurance(),
            character.GetActualAccuracy(),
            character.GetActualSpeed(),
            character.GetActualLuck(),
        };
        for (int index = 0; index < out.skills.size(); ++index)
            out.skills[index] = gameTextToUtf8(localization->skillName(character.GetSkillIdxByOrder(index)));
    }
    result.activeSlot = std::clamp((pGUIWindow_CurrentMenu->pCurrentPosActiveItem -
                                    pGUIWindow_CurrentMenu->pStartingPosActiveItem) / 7, 0, 3);
    result.bonus = CharacterCreation_GetUnspentAttributePointCount();
    return result;
}

BlazonBridge::PartyCreationFocus BlazonBridge::partyCreationPointerFocus(
    const GUIWindow &window, const PartyCreationState &state) const {
    Pointi pointer = EngineIocContainer::ResolveMouse()->position();

    for (GUIButton *button : window.vButtons) {
        if (!button->Contains(pointer))
            continue;
        int slot = std::min(static_cast<int>(button->msg_param), 3);
        switch (button->msg) {
        case UIMSG_PlayerCreationChangeName:
            return {fmt::format("name:{}", slot), "Name " + state.characters[slot].name};
        case UIMSG_PlayerCreation_FacePrev:
            return {fmt::format("portrait-prev:{}", slot), "previous portrait"};
        case UIMSG_PlayerCreation_FaceNext:
            return {fmt::format("portrait-next:{}", slot), "next portrait"};
        case UIMSG_PlayerCreation_VoicePrev:
            return {fmt::format("voice-prev:{}", slot), "previous voice"};
        case UIMSG_PlayerCreation_VoiceNext:
            return {fmt::format("voice-next:{}", slot), "next voice"};
        case UIMSG_48:
        case UIMSG_49:
        case UIMSG_PlayerCreationRemoveUpSkill:
        case UIMSG_PlayerCreationRemoveDownSkill: {
            int order = std::to_underlying(button->msg) - std::to_underlying(UIMSG_48);
            std::string skill = state.characters[slot].skills[order];
            return {fmt::format("skill:{}:{}", slot, order), "Skill " + skill};
        }
        case UIMSG_0: {
            int attribute = button->msg_param % 7;
            slot = BlazonPartyCreation::statSlot(button->msg_param);
            std::string label = gameTextToUtf8(localization->attributeName(static_cast<Attribute>(attribute)));
            return {fmt::format("stat:{}:{}", slot, attribute),
                    fmt::format("{} {}", label, state.characters[slot].stats[attribute])};
        }
        case UIMSG_PlayerCreationSelectClass: {
            Class classType = static_cast<Class>(button->msg_param);
            std::string name = gameTextToUtf8(localization->expand(localization->className(classType)));
            bool chosen = state.characters[state.activeSlot].className == name;
            return {fmt::format("class:{}", button->msg_param),
                    "Class " + name + (chosen ? ", chosen" : "")};
        }
        case UIMSG_PlayerCreationSelectActiveSkill: {
            Character &character = pParty->pCharacters[state.activeSlot];
            Skill skill = character.GetSkillIdxByOrder(button->msg_param + 4);
            std::string name = gameTextToUtf8(localization->skillName(skill));
            bool chosen = static_cast<bool>(character.pActiveSkills[skill]);
            return {fmt::format("available-skill:{}", button->msg_param),
                    "Available skill " + name + (chosen ? ", chosen" : "")};
        }
        case UIMSG_PlayerCreationClickOK:
            return {"ok", gameTextToUtf8(localization->str(LSTR_OK_BUTTON))};
        case UIMSG_PlayerCreationClickReset:
            return {"clear", gameTextToUtf8(localization->str(LSTR_CLEAR_BUTTON))};
        case UIMSG_PlayerCreationClickMinus: {
            int attribute = window.pCurrentPosActiveItem - window.pStartingPosActiveItem;
            attribute %= 7;
            return {"decrease", "decrease " + gameTextToUtf8(localization->attributeName(static_cast<Attribute>(attribute)))};
        }
        case UIMSG_PlayerCreationClickPlus: {
            int attribute = window.pCurrentPosActiveItem - window.pStartingPosActiveItem;
            attribute %= 7;
            return {"increase", "increase " + gameTextToUtf8(localization->attributeName(static_cast<Attribute>(attribute)))};
        }
        default:
            break;
        }
    }

    if (Recti(543, 393, 70, 35).contains(pointer))
        return {"bonus", fmt::format("Bonus points {}", state.bonus)};

    for (int slot = 0; slot < state.characters.size(); ++slot) {
        int x = 158 * slot;
        const PartyCharacterState &character = state.characters[slot];
        if (Recti(x + 85, 24, 68, 35).contains(pointer))
            return {fmt::format("race:{}", slot), "Race " + character.race};
        if (Recti(x + 17, 35, 65, 65).contains(pointer))
            return {fmt::format("portrait:{}", slot), fmt::format("Portrait {}, {}", character.face + 1, character.race)};
        if (Recti(x + 85, 94, 68, 25).contains(pointer))
            return {fmt::format("slot-class:{}", slot), "Class " + character.className};
        if (Recti(x + 5, 21, 153, 365).contains(pointer))
            return {fmt::format("slot:{}", slot),
                    fmt::format("Slot {} of 4, {}, {} {}", slot + 1, character.name, character.race, character.className)};
    }
    return {};
}

BlazonBridge::PartyCreationFocus BlazonBridge::partyCreationKeyboardFocus(
    const GUIWindow &window, const PartyCreationState &state) const {
    int offset = window.pCurrentPosActiveItem - window.pStartingPosActiveItem;
    if (offset < 0 || offset >= 28)
        return {};
    int slot = offset / 7;
    int attribute = offset % 7;
    std::string label = gameTextToUtf8(localization->attributeName(static_cast<Attribute>(attribute)));
    return {fmt::format("stat:{}:{}", slot, attribute),
            fmt::format("{} {}", label, state.characters[slot].stats[attribute])};
}

std::string BlazonBridge::partyCreationChange(const PartyCreationState &before,
                                              const PartyCreationState &after) const {
    auto slotSummary = [](int slot, const PartyCharacterState &character) {
        return fmt::format("Slot {} of 4, {}, {} {}", slot + 1, character.name, character.race, character.className);
    };
    if (before.activeSlot != after.activeSlot)
        return slotSummary(after.activeSlot, after.characters[after.activeSlot]);

    std::vector<int> changedSlots;
    for (int slot = 0; slot < after.characters.size(); ++slot) {
        if (before.characters[slot] != after.characters[slot])
            changedSlots.push_back(slot);
    }
    if (changedSlots.size() > 1)
        return "Party cleared. " + slotSummary(after.activeSlot, after.characters[after.activeSlot]);
    if (changedSlots.empty())
        return before.bonus == after.bonus ? std::string() : fmt::format("Bonus points {}", after.bonus);

    int slot = changedSlots.front();
    const PartyCharacterState &oldCharacter = before.characters[slot];
    const PartyCharacterState &character = after.characters[slot];
    std::string prefix = slot == after.activeSlot ? std::string() : fmt::format("Slot {}, ", slot + 1);
    if (oldCharacter.face != character.face) {
        std::string change = fmt::format("Race {}, portrait {}", character.race, character.face + 1);
        if (oldCharacter.name != character.name)
            change += ", Name " + character.name;
        return prefix + change;
    }
    if (oldCharacter.voice != character.voice)
        return prefix + fmt::format("Voice {}", character.voice + 1);
    if (oldCharacter.className != character.className)
        return prefix + "Class " + character.className;

    std::string skillChange = BlazonPartyCreation::skillChange(
        oldCharacter.skills, character.skills, gameTextToUtf8(localization->skillName(SKILL_INVALID)));
    if (!skillChange.empty())
        return prefix + skillChange;
    if (oldCharacter.name != character.name)
        return prefix + "Name " + character.name;
    for (int attribute = 0; attribute < character.stats.size(); ++attribute) {
        if (oldCharacter.stats[attribute] == character.stats[attribute])
            continue;
        return prefix + fmt::format("{} {}, bonus points {}",
                                    gameTextToUtf8(localization->attributeName(static_cast<Attribute>(attribute))),
                                    character.stats[attribute], after.bonus);
    }
    return before.bonus == after.bonus ? std::string() : fmt::format("Bonus points {}", after.bonus);
}

bool BlazonBridge::emitPartyCreationState(const PartyCreationState &state) {
    const PartyCharacterState &character = state.characters[state.activeSlot];
    std::string key = fmt::format("{}\x1f{}\x1f{}\x1f{}\x1f{}", state.activeSlot, character.name,
                                  character.race, character.className, state.bonus);
    for (int value : character.stats)
        key += "\x1f" + std::to_string(value);
    for (const std::string &skill : character.skills)
        key += "\x1f" + skill;
    if (key == _partyCreationStateKey)
        return true;

    std::string instance = "mm7/party-creation/" + std::to_string(_partyCreationInstance);
    std::string collection = instance + "/character";
    std::string subjectId = "mm7/party-creation/slot/" + std::to_string(state.activeSlot + 1);
    Json fields = Json::array();
    fields.push_back(makeTextField(instance, collection, "party_creation_instance", subjectId, "race",
                                   "Character::GetRaceName", "GUIWindow_PartyCreation::Update", character.race));
    fields.push_back(makeTextField(instance, collection, "party_creation_instance", subjectId, "class",
                                   "Localization::className", "GUIWindow_PartyCreation::Update", character.className));
    static constexpr std::array<const char *, 7> statKeys = {
        "might", "intellect", "personality", "endurance", "accuracy", "speed", "luck",
    };
    for (int attribute = 0; attribute < statKeys.size(); ++attribute) {
        fields.push_back(makeIntegerField(instance, collection, "party_creation_instance", subjectId,
                                          statKeys[attribute], "Character::GetActualAttribute",
                                          "GUIWindow_PartyCreation::Update",
                                          gameTextToUtf8(localization->attributeName(static_cast<Attribute>(attribute))),
                                          character.stats[attribute]));
    }
    std::string skills;
    for (const std::string &skill : character.skills) {
        if (!skills.empty())
            skills += ", ";
        skills += skill;
    }
    fields.push_back(makeTextField(instance, collection, "party_creation_instance", subjectId, "skills",
                                   "Character::GetSkillIdxByOrder and Localization::skillName",
                                   "GUIWindow_PartyCreation::Update", skills));
    fields.push_back(makeField(instance, collection, "party_creation_instance", subjectId.c_str(), "bonus",
                               "CharacterCreation_GetUnspentAttributePointCount", "GUIWindow_PartyCreation::Update",
                               Json{{"type", "integer"}, {"integer", state.bonus}, {"display", std::to_string(state.bonus)}}));
    std::string operations = makeCollection(_sourceRun, instance, collection, "party_creation_instance",
                                            kPartyCreationStateCollectionKind, subjectId, "character",
                                            "Party::pCharacters", character.name,
                                            "GUIWindow_PartyCreation::Update", std::move(fields));
    if (!sendTransaction(operations))
        return false;
    _partyCreationStateKey = std::move(key);
    _partyCreationOperations = std::move(operations);
    return true;
}

bool BlazonBridge::emitPartyCreationEntry(const PartyCreationState &state) {
    if (_partyCreationEntryEmitted)
        return true;
    const PartyCharacterState &character = state.characters[state.activeSlot];
    std::string instance = "mm7/party-creation/" + std::to_string(_partyCreationInstance);
    std::string collection = instance + "/entry";
    Json fields = Json::array();
    fields.push_back(makeField(instance, collection, "party_creation_instance", kPartyCreationSubjectId,
                               "entry_slot", "GUIWindow::pCurrentPosActiveItem", "GUIWindow_PartyCreation::Update",
                               Json{{"type", "integer"}, {"integer", state.activeSlot + 1},
                                    {"display", std::to_string(state.activeSlot + 1)}}));
    fields.push_back(makeTextField(instance, collection, "party_creation_instance", kPartyCreationSubjectId,
                                   "entry_name", "Character::name", "GUIWindow_PartyCreation::Update", character.name));
    fields.push_back(makeTextField(instance, collection, "party_creation_instance", kPartyCreationSubjectId,
                                   "entry_race", "Character::GetRaceName", "GUIWindow_PartyCreation::Update", character.race));
    fields.push_back(makeTextField(instance, collection, "party_creation_instance", kPartyCreationSubjectId,
                                   "entry_class", "Localization::className", "GUIWindow_PartyCreation::Update", character.className));
    std::string operations = makeCollection(_sourceRun, instance, collection, "party_creation_instance",
                                            kPartyCreationEntryCollectionKind, kPartyCreationSubjectId, "screen",
                                            "GUIWindow_PartyCreation", "Create Party",
                                            "GUIWindow_PartyCreation::Update", std::move(fields));
    if (!sendTransaction(operations))
        return false;
    _partyCreationEntryEmitted = true;
    _partyCreationEntryOperations = std::move(operations);
    return true;
}

bool BlazonBridge::emitPartyCreationFocus(const PartyCreationFocus &focus) {
    if (_partyCreationFocusInstance != 0) {
        endLifetime("party_creation_focus_instance",
                    "mm7/party-creation/focus/" + std::to_string(_partyCreationFocusInstance));
        _partyCreationFocusInstance = 0;
        _partyCreationFocusOperations.clear();
    }
    if (focus.text.empty())
        return true;

    uint64_t number = ++_instanceSequence;
    std::string instance = "mm7/party-creation/focus/" + std::to_string(number);
    std::string collection = instance + "/state";
    Json fields = Json::array();
    fields.push_back(makeTextField(instance, collection, "party_creation_focus_instance", kPartyCreationSubjectId,
                                   "focus", "GUIWindow::vButtons and pointer or keyboard focus",
                                   "GUIWindow_PartyCreation::Update", focus.text));
    std::string operations = makeCollection(_sourceRun, instance, collection, "party_creation_focus_instance",
                                            kPartyCreationFocusCollectionKind, kPartyCreationSubjectId, "screen",
                                            "GUIWindow_PartyCreation", "Create Party",
                                            "GUIWindow_PartyCreation::Update", std::move(fields));
    if (!sendTransaction(operations))
        return false;
    _partyCreationFocusInstance = number;
    _partyCreationFocusOperations = std::move(operations);
    return true;
}

bool BlazonBridge::emitPartyCreationChange(const std::string &text) {
    if (text.empty())
        return true;
    std::string instance = "mm7/party-creation/" + std::to_string(_partyCreationInstance);
    std::string collection = instance + "/change";
    Json fields = Json::array();
    fields.push_back(makeTextField(instance, collection, "party_creation_instance", kPartyCreationSubjectId,
                                   "change", "difference between consecutive party creation states",
                                   "GUIWindow_PartyCreation::Update", text));
    std::string operations = makeCollection(_sourceRun, instance, collection, "party_creation_instance",
                                            kPartyCreationChangeCollectionKind, kPartyCreationSubjectId, "screen",
                                            "GUIWindow_PartyCreation", "Create Party",
                                            "GUIWindow_PartyCreation::Update", std::move(fields));
    if (!sendTransaction(operations))
        return false;
    _partyCreationChangeOperations = std::move(operations);
    return true;
}

void BlazonBridge::observePartyCreation(GUIWindow &window) {
    if (!_enabled)
        return;
    if (_partyCreationInstance == 0) {
        _partyCreationInstance = ++_instanceSequence;
        _partyCreationStateSeen = false;
        _partyCreationEntryEmitted = false;
        _partyCreationStateKey.clear();
        _partyCreationPointerKey.clear();
        _partyCreationKeyboardKey.clear();
        _partyCreationOperations.clear();
        _partyCreationEntryOperations.clear();
        _partyCreationFocusOperations.clear();
        _partyCreationChangeOperations.clear();
    }

    PartyCreationState state = partyCreationState();
    emitPartyCreationState(state);
    emitPartyCreationEntry(state);
    PartyCreationFocus pointerFocus = partyCreationPointerFocus(window, state);
    PartyCreationFocus keyboardFocus = partyCreationKeyboardFocus(window, state);
    if (!_partyCreationStateSeen) {
        _partyCreationStateSeen = true;
        _partyCreationState = state;
        _partyCreationPointerKey = pointerFocus.key;
        _partyCreationKeyboardKey = keyboardFocus.key;
        return;
    }

    if (state != _partyCreationState) {
        std::string change = partyCreationChange(_partyCreationState, state);
        if (emitPartyCreationChange(change))
            _partyCreationState = state;
        _partyCreationPointerKey = pointerFocus.key;
        _partyCreationKeyboardKey = keyboardFocus.key;
        return;
    }

    if (pointerFocus.key != _partyCreationPointerKey) {
        if (emitPartyCreationFocus(pointerFocus)) {
            _partyCreationPointerKey = pointerFocus.key;
            _partyCreationKeyboardKey = keyboardFocus.key;
        }
    } else if (keyboardFocus.key != _partyCreationKeyboardKey) {
        if (emitPartyCreationFocus(keyboardFocus)) {
            _partyCreationPointerKey = pointerFocus.key;
            _partyCreationKeyboardKey = keyboardFocus.key;
        }
    }
}

void BlazonBridge::endPartyCreation() {
    if (!_enabled || _partyCreationInstance == 0)
        return;
    if (_partyCreationFocusInstance != 0) {
        endLifetime("party_creation_focus_instance",
                    "mm7/party-creation/focus/" + std::to_string(_partyCreationFocusInstance));
        _partyCreationFocusInstance = 0;
    }
    endLifetime("party_creation_instance", "mm7/party-creation/" + std::to_string(_partyCreationInstance));
    _partyCreationInstance = 0;
    _partyCreationStateSeen = false;
    _partyCreationEntryEmitted = false;
    _partyCreationStateKey.clear();
    _partyCreationPointerKey.clear();
    _partyCreationKeyboardKey.clear();
    _partyCreationOperations.clear();
    _partyCreationEntryOperations.clear();
    _partyCreationFocusOperations.clear();
    _partyCreationChangeOperations.clear();
}

bool BlazonBridge::emitPortraitVitals(int characterSlot, const std::string &instance,
                                      const char *lifetimeKind, const char *collectionKind,
                                      const char *hook, std::string *operations) {
    Character &character = pParty->pCharacters[characterSlot];
    Condition condition = character.GetMajorConditionIdx();
    std::string conditionName = gameTextToUtf8(localization->characterConditionName(condition));
    BlazonPortraits::ConditionText conditionText = BlazonPortraits::conditionText(condition, conditionName);
    std::string characterName = gameTextToUtf8(character.name);
    std::string lineStart = conditionText.prefix + characterName;
    std::string lineEnd = std::to_string(character.GetMaxMana()) + conditionText.suffix;
    std::string collection = instance + "/vitals";
    std::string subjectId = "mm7/party/character/slot/" + std::to_string(characterSlot + 1);
    Json fields = Json::array();
    fields.push_back(makeTextField(instance, collection, lifetimeKind, subjectId, "line_start",
                                   "Character::name and optional leading major condition", hook, lineStart));
    fields.push_back(makeField(instance, collection, lifetimeKind, subjectId.c_str(), "health",
                               "Character::GetHealth", hook,
                               Json{{"type", "integer"}, {"integer", character.GetHealth()},
                                    {"display", std::to_string(character.GetHealth())}}));
    fields.push_back(makeField(instance, collection, lifetimeKind, subjectId.c_str(), "health_max",
                               "Character::GetMaxHealth", hook,
                               Json{{"type", "integer"}, {"integer", character.GetMaxHealth()},
                                    {"display", std::to_string(character.GetMaxHealth())}}));
    fields.push_back(makeField(instance, collection, lifetimeKind, subjectId.c_str(), "mana",
                               "Character::GetMana", hook,
                               Json{{"type", "integer"}, {"integer", character.GetMana()},
                                    {"display", std::to_string(character.GetMana())}}));
    fields.push_back(makeField(instance, collection, lifetimeKind, subjectId.c_str(), "mana_max",
                               "Character::GetMaxMana", hook,
                               Json{{"type", "integer"}, {"integer", character.GetMaxMana()},
                                    {"display", std::to_string(character.GetMaxMana())}}));
    fields.push_back(makeField(instance, collection, lifetimeKind, subjectId.c_str(), "condition",
                               "Character::GetMajorConditionIdx and Localization::characterConditionName", hook,
                               Json{{"type", "enum"}, {"code", std::to_string(std::to_underlying(condition))},
                                    {"display", conditionName}}));
    fields.push_back(makeTextField(instance, collection, lifetimeKind, subjectId, "line_end",
                                   "Character::GetMaxMana and optional trailing major condition", hook, lineEnd));
    std::string next = makeCollection(_sourceRun, instance, collection, lifetimeKind, collectionKind,
                                      subjectId, "character", "Party::pCharacters",
                                      characterName, hook, std::move(fields));
    if (!sendTransaction(next))
        return false;
    *operations = std::move(next);
    return true;
}

void BlazonBridge::observePortraitHover(const GUIWindow &window, bool visible) {
    if (!_enabled)
        return;
    int characterSlot = -1;
    if (visible) {
        Pointi pointer = EngineIocContainer::ResolveMouse()->position();
        for (GUIButton *button : window.vButtons) {
            if (button->uButtonType == BUTTON_TYPE_CHARACTER && button->msg == UIMSG_SelectCharacter &&
                button->Contains(pointer)) {
                characterSlot = static_cast<int>(button->msg_param) - 1;
                break;
            }
        }
    }
    if (characterSlot == _portraitHoverSlot) {
        if (_portraitHoverInstance != 0 && _portraitHoverOperations.empty()) {
            emitPortraitVitals(characterSlot, "mm7/portrait-hover/" + std::to_string(_portraitHoverInstance),
                               "portrait_hover_instance", kPortraitVitalsCollectionKind,
                               "Engine::DrawGUI", &_portraitHoverOperations);
        }
        return;
    }
    endPortraitHover();
    _portraitHoverSlot = characterSlot;
    if (characterSlot < 0)
        return;
    _portraitHoverInstance = ++_instanceSequence;
    emitPortraitVitals(characterSlot, "mm7/portrait-hover/" + std::to_string(_portraitHoverInstance),
                       "portrait_hover_instance", kPortraitVitalsCollectionKind,
                       "Engine::DrawGUI", &_portraitHoverOperations);
}

void BlazonBridge::endPortraitHover() {
    if (_portraitHoverInstance != 0) {
        endLifetime("portrait_hover_instance",
                    "mm7/portrait-hover/" + std::to_string(_portraitHoverInstance));
    }
    _portraitHoverInstance = 0;
    _portraitHoverSlot = -1;
    _portraitHoverOperations.clear();
}

void BlazonBridge::observePortraitSelection(int characterSlot) {
    if (!_enabled)
        return;
    assert(characterSlot >= 0 && characterSlot < pParty->pCharacters.size());
    endPortraitSelection();
    _portraitSelectionInstance = ++_instanceSequence;
    emitPortraitVitals(characterSlot,
                       "mm7/portrait-selection/" + std::to_string(_portraitSelectionInstance),
                       "portrait_selection_instance", kPortraitSelectionVitalsCollectionKind,
                       "GameUI_OnPlayerPortraitLeftClick", &_portraitSelectionOperations);
}

void BlazonBridge::endPortraitSelection() {
    if (_portraitSelectionInstance != 0) {
        endLifetime("portrait_selection_instance",
                    "mm7/portrait-selection/" + std::to_string(_portraitSelectionInstance));
    }
    _portraitSelectionInstance = 0;
    _portraitSelectionOperations.clear();
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
        _popupOperations.clear();
    } else if (!holding && _popupHolding) {
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
    std::string key = gameTextToUtf8(text);
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
        _popupOperations = std::move(operations);
    }
}

void BlazonBridge::endPopup() {
    std::string instance = "mm7/popup/" + std::to_string(_popupInstance);
    endLifetime("popup_instance", instance);
    _popupEmitted = false;
    _popupOperations.clear();
}

void BlazonBridge::sendInput(const char *action) {
    if (!_enabled)
        return;
    if (std::strcmp(action, "stop") == 0)
        flushPendingEnds();
    else
        sendResync();
    uint64_t sequence = _inputSequence + 1;
    Json input = Json{
        {"type", "input"},
        {"action", action},
        {"source_run", _sourceRun},
        {"screen", static_cast<int>(current_screen_type)},
        {"input_id", _sourceRun + ":input:" + std::to_string(sequence)},
    };
    if (sendDatagram(input.dump()))
        _inputSequence = sequence;
}

bool BlazonBridge::sendTransaction(const std::string &operationsJson, bool resync) {
    if (!flushPendingEnds())
        return false;
    return sendTransactionDatagram(operationsJson, resync);
}

bool BlazonBridge::sendTransactionDatagram(const std::string &operationsJson, bool resync) {
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
    if (resync)
        transaction["resync"] = true;
    if (!sendDatagram(transaction.dump(-1, ' ', false, Json::error_handler_t::replace)))
        return false;
    _transactionSequence = sequence;
    return true;
}

bool BlazonBridge::sendResync() {
    if (!flushPendingEnds())
        return false;
    Json operations = Json::array();
    appendOperations(operations, _currentOperations);
    appendOperations(operations, _popupOperations);
    appendOperations(operations, _eventOperations);
    appendOperations(operations, _messageOperations);
    appendOperations(operations, _dialogueOperations);
    appendOperations(operations, _dialogueFocusOperations);
    appendOperations(operations, _houseOperations);
    appendOperations(operations, _houseFocusOperations);
    appendOperations(operations, _partyCreationOperations);
    appendOperations(operations, _partyCreationEntryOperations);
    appendOperations(operations, _partyCreationFocusOperations);
    appendOperations(operations, _partyCreationChangeOperations);
    appendOperations(operations, _portraitHoverOperations);
    appendOperations(operations, _portraitSelectionOperations);
    if (operations.empty())
        return true;
    return sendTransactionDatagram(operations.dump(-1, ' ', false, Json::error_handler_t::replace), true);
}

bool BlazonBridge::flushPendingEnds() {
    while (!_pendingEnds.empty()) {
        if (!sendTransactionDatagram(_pendingEnds.front(), false))
            return false;
        _pendingEnds.erase(_pendingEnds.begin());
    }
    return true;
}

void BlazonBridge::endLifetime(const char *lifetimeKind, const std::string &instance) {
    _pendingEnds.push_back(makeEnd(lifetimeKind, instance));
    flushPendingEnds();
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
