#pragma once

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

namespace BlazonPartyCreation {

inline int statSlot(int messageParam) {
    return std::clamp(messageParam / 7, 0, 3);
}

inline std::string skillChange(const std::array<std::string, 4> &before,
                               const std::array<std::string, 4> &after,
                               std::string_view emptySkill) {
    auto contains = [](const std::array<std::string, 4> &skills, const std::string &skill) {
        return std::find(skills.begin(), skills.end(), skill) != skills.end();
    };
    for (const std::string &skill : after) {
        if (skill != emptySkill && !contains(before, skill))
            return "Skill " + skill + " chosen";
    }
    for (const std::string &skill : before) {
        if (skill != emptySkill && !contains(after, skill))
            return "Skill " + skill + " removed";
    }
    return {};
}

}  // namespace BlazonPartyCreation
