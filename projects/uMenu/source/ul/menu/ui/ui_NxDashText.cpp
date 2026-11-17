#include <ul/menu/ui/ui_NxDashText.hpp>
#include <ul/menu/ui/ui_Common.hpp>
#include <ul/cfg/cfg_Config.hpp>
#include <ul/fs/fs_Stdio.hpp>
#include <ul/util/util_Json.hpp>
#include <cstdio>
#include <cstring>
#include <map>

extern ul::menu::ui::GlobalSettings g_GlobalSettings;

namespace ul::menu::ui::nxt {

    namespace {

        constexpr const char LanguageFilePath[] = "sdmc:/ulaunch/nxdash_lang.ini";

        std::string UidKey(const AccountUid &uid) {
            char buf[40];
            snprintf(buf, sizeof(buf), "%016lX%016lX", uid.uid[0], uid.uid[1]);
            return buf;
        }

        std::map<std::string, std::string> ReadAll() {
            std::map<std::string, std::string> values;
            auto f = fopen(LanguageFilePath, "r");
            if(f != nullptr) {
                char line[128];
                while(fgets(line, sizeof(line), f) != nullptr) {
                    auto eq = strchr(line, '=');
                    if(eq == nullptr) {
                        continue;
                    }
                    std::string key(line, eq - line);
                    std::string val(eq + 1);
                    while(!val.empty() && ((val.back() == '\n') || (val.back() == '\r') || (val.back() == ' '))) {
                        val.pop_back();
                    }
                    values[key] = val;
                }
                fclose(f);
            }
            return values;
        }

    }

    std::string GetProfileLanguage(const AccountUid &uid) {
        if(!accountUidIsValid(&uid)) {
            return "";
        }
        const auto values = ReadAll();
        const auto it = values.find(UidKey(uid));
        return (it == values.end()) ? "" : it->second;
    }

    void SetProfileLanguage(const AccountUid &uid, const std::string &code) {
        if(!accountUidIsValid(&uid)) {
            return;
        }
        auto values = ReadAll();
        if(code.empty()) {
            values.erase(UidKey(uid));
        }
        else {
            values[UidKey(uid)] = code;
        }
        auto f = fopen(LanguageFilePath, "w");
        if(f != nullptr) {
            for(const auto &[key, val] : values) {
                fprintf(f, "%s=%s\n", key.c_str(), val.c_str());
            }
            fclose(f);
        }
    }

    void ApplyProfileLanguage() {
        const auto code = GetProfileLanguage(g_GlobalSettings.system_status.selected_user);
        if(code.empty()) {
            cfg::LoadLanguageJsons(MenuLanguagesPath, g_GlobalSettings.main_lang, g_GlobalSettings.default_lang);
        }
        else {
            util::JSON lang;
            if(R_SUCCEEDED(util::LoadJSONFromFile(lang, fs::JoinPath(MenuLanguagesPath, code) + ".json")) || R_SUCCEEDED(util::LoadJSONFromFile(lang, fs::JoinPath(BuiltinMenuLanguagesPath, code) + ".json"))) {
                g_GlobalSettings.main_lang = lang;
            }
        }
        ResetWeekdayList();
        g_LangGen++;
    }

}
