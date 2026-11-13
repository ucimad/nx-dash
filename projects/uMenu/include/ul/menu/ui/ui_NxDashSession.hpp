
#pragma once
#include <switch.h>
#include <cstdio>
#include <string>

// NX Dash: time played in the current game session.
// Counts only while the game is in the foreground: time in the menu is excluded, and the clock used
// (the system tick) is expected not to advance while the console sleeps.
// uMenu is not running while a game is, so the state lives in a small file.

namespace ul::menu::ui::session {

    constexpr const char Path[] = "sdmc:/ulaunch/nxdash_session.bin";
    constexpr u64 Magic = 0x3153534E4458444E; // "NDXDNSS1"

    struct Record {
        u64 magic;
        u64 played_ns;
        u64 resumed_tick; // 0 = not running
    };

    inline bool Load(Record &rec) {
        auto f = fopen(Path, "rb");
        if(f == nullptr) {
            return false;
        }
        const auto ok = (fread(&rec, sizeof(rec), 1, f) == 1) && (rec.magic == Magic);
        fclose(f);
        return ok;
    }

    inline void Save(const Record &rec) {
        auto f = fopen(Path, "wb");
        if(f != nullptr) {
            fwrite(&rec, sizeof(rec), 1, f);
            fclose(f);
        }
    }

    // A game was just launched
    inline void Start() {
        Save({ Magic, 0, armGetSystemTick() });
    }

    // Going back into the suspended game
    inline void Resume() {
        Record rec;
        if(Load(rec)) {
            rec.resumed_tick = armGetSystemTick();
            Save(rec);
        }
    }

    // Back in the menu with a game suspended: bank the time played since the last launch/resume
    inline void Pause() {
        Record rec;
        if(Load(rec) && (rec.resumed_tick != 0)) {
            const auto now = armGetSystemTick();
            if(now >= rec.resumed_tick) {
                rec.played_ns += armTicksToNs(now - rec.resumed_tick);
            }
            rec.resumed_tick = 0;
            Save(rec);
        }
    }

    inline void Clear() {
        remove(Path);
    }

    // Seconds played this session, or -1 when unknown
    inline s64 GetSeconds() {
        Record rec;
        if(!Load(rec)) {
            return -1;
        }
        return (s64)(rec.played_ns / 1'000'000'000ul);
    }

    inline std::string Format(const s64 secs) {
        if(secs < 60) {
            return "Menos de 1 min de juego";
        }
        const auto mins = secs / 60;
        if(mins < 60) {
            return std::to_string(mins) + " min de juego";
        }
        return std::to_string(mins / 60) + " h " + std::to_string(mins % 60) + " min de juego";
    }

}
