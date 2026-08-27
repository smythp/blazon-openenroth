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

#include "Library/Logger/Logger.h"

namespace {

using Json = nlohmann::json;

constexpr const char *kSchema = "blazon.semantic-pieces/draft-1";
constexpr const char *kBuild = "8673e3e7+blazon-pointer";
constexpr const char *kPointerCollectionKind = "mm7.under_pointer.state";
constexpr const char *kPopupCollectionKind = "mm7.popup.state";
constexpr const char *kSubjectId = "mm7/pointer";

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
               const char *key, const char *origin, const char *hook, Json value) {
    Json field = makePieceBase(instance + "/field/" + key, "field", makeLifetime(lifetimeKind, instance), origin, hook);
    field["key"] = key;
    field["authority_scope"] = "collection";
    field["relationships"] = Json{{"collection", collection}, {"subject", kSubjectId}};
    field["value"] = std::move(value);
    return field;
}

Json makeSubject(const std::string &sourceRun, const char *hook) {
    Json subject = makePieceBase(kSubjectId, "pointer", makeLifetime("game_session", sourceRun), "Io::Mouse", hook);
    subject["value"] = Json{{"type", "text"}, {"text", "pointer"}};
    return subject;
}

// One complete text collection: the subject plus a text field and a kind field.
std::string makeTextCollection(const std::string &sourceRun, const std::string &instance, const char *lifetimeKind,
                               const char *collectionKind, const char *origin, const char *hook,
                               const std::string &text, const char *kindCode) {
    std::string collection = instance + "/state";
    Json upsert = Json{{"op", "upsert"}, {"pieces", Json::array({makeSubject(sourceRun, hook)})}};
    Json fields = Json::array();
    fields.push_back(makeField(instance, collection, lifetimeKind, "text", origin, hook,
                               Json{{"type", "text"}, {"text", text}, {"display", text}}));
    fields.push_back(makeField(instance, collection, lifetimeKind, "kind", origin, hook,
                               Json{{"type", "enum"}, {"code", kindCode}, {"display", kindCode}}));
    Json replace = Json{
        {"op", "replace"},
        {"collection", Json{
            {"id", collection},
            {"kind", collectionKind},
            {"complete", true},
            {"subject", kSubjectId},
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
    _enabled = true;
    MM_LOG(LOG_INFO, "Blazon: bridge enabled, socket {}, source run {}", _socketPath, _sourceRun);
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
                                                "StatusBar::_statusString", "StatusBar::draw", text, "status");
    if (sendTransaction(operations))
        _currentInstance = number;
}

void BlazonBridge::endPointer() {
    std::string instance = "mm7/under-pointer/" + std::to_string(_currentInstance);
    sendTransaction(makeEnd("hover_instance", instance));
    _currentInstance = 0;
}

void BlazonBridge::observePopupHold(bool holding) {
    if (!_enabled)
        return;
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

void BlazonBridge::captureText(std::string_view text, bool title) {
    if (!_inPopupFrame)
        return;
    std::string plain = stripFontCodes(text);
    if (plain.empty())
        return;
    (title ? _popupTitles : _popupBody).push_back(std::move(plain));
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
        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        std::strncpy(address.sun_path, _socketPath.c_str(), sizeof(address.sun_path) - 1);
        if (connect(_socket, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) < 0) {
            if (!_warned) {
                MM_LOG(LOG_WARNING, "Blazon: cannot connect to {}: {}", _socketPath, std::strerror(errno));
                _warned = true;
            }
            close(_socket);
            _socket = -1;
            return false;
        }
        _warned = false;
    }
    ssize_t sent = send(_socket, payload.data(), payload.size(), MSG_DONTWAIT);
    if (sent != static_cast<ssize_t>(payload.size())) {
        if (!_warned) {
            MM_LOG(LOG_WARNING, "Blazon: datagram send failed: {}", std::strerror(errno));
            _warned = true;
        }
        close(_socket);
        _socket = -1;
        return false;
    }
    return true;
}
