#pragma once

#include <cstdint>
#include <string>
#include <string_view>

/**
 * Blazon bridge for OpenEnroth.
 *
 * Inert unless the BLAZON_SOCKET environment variable names a Unix datagram
 * socket that a Blazon runtime is listening on. Emits evidence-bearing
 * semantic pieces (schema blazon.semantic-pieces/draft-1) exactly as the
 * ScummVM Xeen bridge does; it never speaks and never reads input.
 *
 * First scope, 2026-08-27: the text under the pointer, observed once per
 * frame at the status bar. This chokepoint carries the string only; the
 * identity-bearing hooks in GameUI_WritePointedObjectStatusString come next.
 */
class BlazonBridge {
 public:
    static BlazonBridge &instance();

    bool enabled() const { return _enabled; }

    /**
     * Per-frame observation of the permanent (hover) status-bar text. A change
     * of text ends the previous hover instance and begins a new one; an empty
     * text ends the current instance without beginning another.
     */
    void observeStatus(std::string_view text);

 private:
    BlazonBridge();
    ~BlazonBridge();
    BlazonBridge(const BlazonBridge &) = delete;
    BlazonBridge &operator=(const BlazonBridge &) = delete;

    bool sendDatagram(const std::string &payload);
    bool sendTransaction(const std::string &operationsJson);
    void beginPointer(const std::string &text);
    void endPointer();

    std::string _socketPath;
    std::string _sourceRun;
    int _socket = -1;
    bool _enabled = false;
    bool _warned = false;
    uint64_t _transactionSequence = 0;
    uint64_t _instanceSequence = 0;
    uint64_t _currentInstance = 0;
    std::string _currentText;
};
