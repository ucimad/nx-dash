
#pragma once
#include <switch.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <map>
#include <string>

// NX Dash feature switches: sdmc:/ulaunch/nxdash.ini, one "key=0" or "key=1" per line.
// Everything is on unless the file turns it off, so a feature that misbehaves on the console
// can be disabled by editing one line instead of rebuilding.

namespace ul::nxdash {

    constexpr const char ConfigPath[] = "sdmc:/ulaunch/nxdash.ini";

    inline std::map<std::string, int> &ConfigValues() {
        static std::map<std::string, int> values;
        static bool loaded = false;
        if(!loaded) {
            loaded = true;
            auto f = fopen(ConfigPath, "r");
            if(f != nullptr) {
                char line[128];
                while(fgets(line, sizeof(line), f) != nullptr) {
                    if((line[0] == '#') || (line[0] == ';')) {
                        continue;
                    }
                    auto eq = strchr(line, '=');
                    if(eq == nullptr) {
                        continue;
                    }
                    std::string key(line, eq - line);
                    while(!key.empty() && ((key.back() == ' ') || (key.back() == '\t'))) {
                        key.pop_back();
                    }
                    while(!key.empty() && ((key.front() == ' ') || (key.front() == '\t'))) {
                        key.erase(key.begin());
                    }
                    if(!key.empty()) {
                        values[key] = atoi(eq + 1);
                    }
                }
                fclose(f);
            }
        }
        return values;
    }

    inline bool On(const char *key, const bool def = true) {
        const auto &values = ConfigValues();
        const auto it = values.find(key);
        return (it == values.end()) ? def : (it->second != 0);
    }

    inline int Value(const char *key, const int def) {
        const auto &values = ConfigValues();
        const auto it = values.find(key);
        return (it == values.end()) ? def : it->second;
    }

    // True while one of the NX Dash overlays owns the screen (pause sheet, lock screen, launch, duel, case back...)
    inline bool g_Modal = false;

}
