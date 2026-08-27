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

#include <nlohmann/json.hpp>

#include "Library/Logger/Logger.h"

namespace {

using Json = nlohmann::json;

constexpr const char *kSchema = "blazon.semantic-pieces/draft-1";
constexpr const char *kBuild = "8673e3e7+blazon-pointer";
constexpr const char *kCollectionKind = "mm7.under_pointer.state";
constexpr const char *kSubjectId = "mm7/pointer";

Json makeLifetime(const char *kind, const std::string &id) {
    return Json{{"kind", kind}, {"id", id}};
}

Json makeProvenance(const char *hook) {
    return Json{{"source", "OpenEnroth"}, {"build", kBuild}, {"hook", hook}};
}

Json makePieceBase(const std::string &id, const char *kind, const Json &lifetime, const char *origin) {
    return Json{
        {"id", id},
        {"kind", kind},
        {"lifetime", lifetime},
        {"origin", origin},
        {"authority", "game"},
        {"currency", "current"},
        {"provenance", makeProvenance("StatusBar::draw")},
    };
}

Json makeField(const std::string &instance, const std::string &collection, const char *key,
               const char *origin, Json value) {
    Json field = makePieceBase(instance + "/field/" + key, "field", makeLifetime("hover_instance", instance), origin);
    field["key"] = key;
    field["authority_scope"] = "collection";
    field["relationships"] = Json{{"collection", collection}, {"subject", kSubjectId}};
    field["value"] = std::move(value);
    return field;
}

std::string dump(const Json &json) {
    // MM7 strings are cp1252; anything outside UTF-8 becomes U+FFFD rather than an exception.
    return json.dump(-1, ' ', false, Json::error_handler_t::replace);
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
    std::string collection = instance + "/state";

    Json subject = makePieceBase(kSubjectId, "pointer", makeLifetime("game_session", _sourceRun), "Io::Mouse");
    subject["value"] = Json{{"type", "text"}, {"text", "pointer"}};

    Json upsert = Json{{"op", "upsert"}, {"pieces", Json::array({subject})}};

    Json fields = Json::array();
    fields.push_back(makeField(instance, collection, "text", "StatusBar::_statusString",
                               Json{{"type", "text"}, {"text", text}, {"display", text}}));
    fields.push_back(makeField(instance, collection, "kind", "StatusBar::_statusString",
                               Json{{"type", "enum"}, {"code", "status"}, {"display", "status"}}));

    Json replace = Json{
        {"op", "replace"},
        {"collection", Json{
            {"id", collection},
            {"kind", kCollectionKind},
            {"complete", true},
            {"subject", kSubjectId},
            {"lifetime", makeLifetime("hover_instance", instance)},
        }},
        {"pieces", fields},
    };

    if (sendTransaction(dump(Json::array({upsert, replace}))))
        _currentInstance = number;
}

void BlazonBridge::endPointer() {
    std::string instance = "mm7/under-pointer/" + std::to_string(_currentInstance);
    Json end = Json{{"op", "end_lifetime"}, {"lifetime", makeLifetime("hover_instance", instance)}};
    sendTransaction(dump(Json::array({end})));
    _currentInstance = 0;
}

bool BlazonBridge::sendTransaction(const std::string &operationsJson) {
    // The sequence is reserved only when the datagram actually leaves, so a
    // dropped send never opens a permanent gap in the run.
    uint64_t sequence = _transactionSequence + 1;
    Json transaction = Json{
        {"type", "transaction"},
        {"schema", kSchema},
        {"source_run", _sourceRun},
        {"sequence", sequence},
        {"transaction_id", _sourceRun + ":transaction:" + std::to_string(sequence)},
        {"operations", Json::parse(operationsJson)},
    };
    if (!sendDatagram(dump(transaction)))
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
