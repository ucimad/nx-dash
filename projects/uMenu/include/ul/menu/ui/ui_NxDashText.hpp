
#pragma once
#include <switch.h>
#include <string>

// NX Dash text: every string NX Dash shows comes from the uMenu language files ("nx_*" keys),
// so the menu follows the console language, or the language chosen for the current profile.

namespace ul::menu::ui {

    std::string GetLanguageString(const std::string &name);

    void ResetWeekdayList();

}

namespace ul::menu::ui::nxt {

    // Bumped whenever the language changes, so cached text gets rebuilt
    inline u32 g_LangGen = 0;

    inline std::string T(const std::string &key) {
        return GetLanguageString("nx_" + key);
    }

    // Replaces "{name}" with a value
    inline std::string Sub(std::string text, const char *name, const std::string &value) {
        const auto tag = std::string("{") + name + "}";
        for(auto pos = text.find(tag); pos != std::string::npos; pos = text.find(tag, pos + value.length())) {
            text.replace(pos, tag.length(), value);
        }
        return text;
    }

    inline std::string Played(const u64 seconds) {
        const auto mins = seconds / 60;
        if(mins < 1) {
            return T("t_lt1");
        }
        if(mins < 60) {
            return Sub(T("t_min"), "m", std::to_string(mins));
        }
        return Sub(Sub(T("t_hmin"), "h", std::to_string(mins / 60)), "m", std::to_string(mins % 60));
    }

    // Profile language: "" follows the console, otherwise a language file code ("en", "es"...)
    std::string GetProfileLanguage(const AccountUid &uid);
    void SetProfileLanguage(const AccountUid &uid, const std::string &code);
    void ApplyProfileLanguage();

}
