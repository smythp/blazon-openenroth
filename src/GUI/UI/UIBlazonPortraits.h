#pragma once

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

#include "Engine/Objects/CharacterEnums.h"

namespace BlazonPortraits {

struct ConditionText {
    std::string prefix;
    std::string suffix;
};

inline ConditionText conditionText(Condition condition, std::string_view name) {
    if (condition == CONDITION_GOOD)
        return {};
    static constexpr std::array first = {
        CONDITION_SLEEP,
        CONDITION_UNCONSCIOUS,
        CONDITION_DEAD,
        CONDITION_ERADICATED,
    };
    if (std::find(first.begin(), first.end(), condition) != first.end())
        return {std::string(name) + ", ", {}};
    return {{}, ", " + std::string(name)};
}

}  // namespace BlazonPortraits
