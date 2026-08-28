#pragma once

#include <string>
#include <string_view>

#include "Utility/String/Format.h"

namespace BlazonCharacterRecord {

inline std::string currentAndBase(std::string_view label, int current, int base) {
    if (current == base)
        return fmt::format("{} {}", label, current);
    return fmt::format("{} {} of {}", label, current, base);
}

inline std::string currentAndMaximum(std::string_view label, int current, int maximum) {
    return fmt::format("{} {} of {}", label, current, maximum);
}

inline std::string skill(std::string_view name, int level, std::string_view mastery) {
    return fmt::format("{} {} {}", name, level, mastery);
}

}  // namespace BlazonCharacterRecord
