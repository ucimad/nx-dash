
#pragma once
#include <switch.h>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>
#include <algorithm>

// NX Dash: time played in the current game session.
// Counts only while the game is in the foreground: time in the menu is excluded, and the clock used
// (the system tick) is expected not to advance while the console sleeps.
// uMenu is not running while a game is, so the state lives in a small file.

namespace ul::menu::ui::session {

    constexpr const char Path[] = "sdmc:/ulaunch/nxdash_session.bin";
    constexpr u64 Magic = 0x3253534E4458444E; // "NDXDNSS2"

    struct Record {
        u64 magic;
        u64 played_ns;
        u64 resumed_tick; // 0 = not running
        u64 app_id;       // 0 = homebrew / unknown
    };

    // ---- Per-day play log kept by NX Dash itself (for the streak and the weekly chart on the back of the case) ----
    // Only time banked when coming back to the menu is counted, so a game that is still running when the console
    // powers off loses its last stretch. Totals shown to the user come from the console's own statistics when available.

    constexpr const char StatsPath[] = "sdmc:/ulaunch/nxdash_stats.bin";
    constexpr u32 StatsMagic = 0x3153444E; // "NDS1"
    constexpr u32 StatsKeepDays = 120;
    constexpr size_t StatsMaxRows = 4000;

    struct StatRow {
        u64 app_id;
        u32 day;      // days since 1970-01-01, local time
        u32 seconds;
        u32 launches;
        u32 pad;
    };

    inline u32 Today() {
        const auto now = ::time(nullptr);
        const auto lt = localtime(&now);
        if(lt == nullptr) {
            return (u32)(now / 86400);
        }
        // Days from civil date (Howard Hinnant's algorithm)
        s32 y = lt->tm_year + 1900;
        const u32 m = lt->tm_mon + 1;
        const u32 d = lt->tm_mday;
        y -= (m <= 2);
        const s32 era = ((y >= 0) ? y : (y - 399)) / 400;
        const u32 yoe = (u32)(y - era * 400);
        const u32 doy = (153 * (m + ((m > 2) ? -3 : 9)) + 2) / 5 + d - 1;
        const u32 doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return (u32)(era * 146097 + (s32)doe - 719468);
    }

    inline std::vector<StatRow> &Stats() {
        static std::vector<StatRow> rows;
        static bool loaded = false;
        if(!loaded) {
            loaded = true;
            auto f = fopen(StatsPath, "rb");
            if(f != nullptr) {
                u32 header[2] = {};
                if((fread(header, sizeof(header), 1, f) == 1) && (header[0] == StatsMagic) && (header[1] <= StatsMaxRows)) {
                    rows.resize(header[1]);
                    if((header[1] > 0) && (fread(rows.data(), sizeof(StatRow), header[1], f) != header[1])) {
                        rows.clear();
                    }
                }
                fclose(f);
            }
        }
        return rows;
    }

    inline void SaveStats() {
        auto &rows = Stats();
        const auto today = Today();
        rows.erase(std::remove_if(rows.begin(), rows.end(), [&](const StatRow &row) {
            return (row.day + StatsKeepDays) < today;
        }), rows.end());
        if(rows.size() > StatsMaxRows) {
            rows.erase(rows.begin(), rows.begin() + (rows.size() - StatsMaxRows));
        }
        auto f = fopen(StatsPath, "wb");
        if(f != nullptr) {
            const u32 header[2] = { StatsMagic, (u32)rows.size() };
            fwrite(header, sizeof(header), 1, f);
            if(!rows.empty()) {
                fwrite(rows.data(), sizeof(StatRow), rows.size(), f);
            }
            fclose(f);
        }
    }

    inline void AddStat(const u64 app_id, const u32 seconds, const u32 launches) {
        if((app_id == 0) || ((seconds == 0) && (launches == 0))) {
            return;
        }
        auto &rows = Stats();
        const auto today = Today();
        for(auto &row : rows) {
            if((row.app_id == app_id) && (row.day == today)) {
                row.seconds += seconds;
                row.launches += launches;
                SaveStats();
                return;
            }
        }
        rows.push_back({ app_id, today, seconds, launches, 0 });
        SaveStats();
    }

    struct AppStats {
        u32 week[7] = {};       // seconds per day, oldest first, today last
        u32 streak = 0;         // consecutive days played, ending today or yesterday
        u64 total_seconds = 0;
        u32 launches = 0;
        s32 days_since_last = -1;
    };

    inline AppStats GetAppStats(const u64 app_id) {
        AppStats out;
        const auto today = Today();
        std::vector<u32> days;
        for(const auto &row : Stats()) {
            if(row.app_id != app_id) {
                continue;
            }
            out.total_seconds += row.seconds;
            out.launches += row.launches;
            if(row.day <= today) {
                const auto ago = today - row.day;
                if(ago < 7) {
                    out.week[6 - ago] += row.seconds;
                }
                if((out.days_since_last < 0) || ((s32)ago < out.days_since_last)) {
                    out.days_since_last = (s32)ago;
                }
                days.push_back(row.day);
            }
        }
        // Streak: walk back from today (or yesterday, if not played yet today)
        auto played_on = [&](const u32 day) {
            return std::find(days.begin(), days.end(), day) != days.end();
        };
        auto day = today;
        if(!played_on(day) && (day > 0)) {
            day--;
        }
        while(played_on(day)) {
            out.streak++;
            if(day == 0) {
                break;
            }
            day--;
        }
        return out;
    }

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

    // ---- Last games played (most recent first, at most 3) ----

    constexpr const char RecentPath[] = "sdmc:/ulaunch/nxdash_recent.bin";
    constexpr size_t RecentMax = 5;
    // Only the newest few get the violet case
    constexpr size_t RecentHighlight = 3;

    inline std::vector<u64> &Recents() {
        static std::vector<u64> recents;
        static bool loaded = false;
        if(!loaded) {
            loaded = true;
            auto f = fopen(RecentPath, "rb");
            if(f != nullptr) {
                u64 id = 0;
                while((recents.size() < RecentMax) && (fread(&id, sizeof(id), 1, f) == 1)) {
                    if(id != 0) {
                        recents.push_back(id);
                    }
                }
                fclose(f);
            }
        }
        return recents;
    }

    // Position in the recent list (0 = last played), or -1
    inline s32 RecentRank(const u64 app_id) {
        const auto &recents = Recents();
        const auto it = std::find(recents.begin(), recents.end(), app_id);
        return (it == recents.end()) ? -1 : (s32)(it - recents.begin());
    }

    inline bool IsRecent(const u64 app_id) {
        const auto rank = RecentRank(app_id);
        return (rank >= 0) && (rank < (s32)RecentHighlight);
    }

    inline void PushRecent(const u64 app_id) {
        if(app_id == 0) {
            return;
        }
        auto &recents = Recents();
        recents.erase(std::remove(recents.begin(), recents.end(), app_id), recents.end());
        recents.insert(recents.begin(), app_id);
        if(recents.size() > RecentMax) {
            recents.resize(RecentMax);
        }
        auto f = fopen(RecentPath, "wb");
        if(f != nullptr) {
            fwrite(recents.data(), sizeof(u64), recents.size(), f);
            fclose(f);
        }
    }

    // A game was just launched (app_id 0 = homebrew, not tracked as recent)
    inline void Start(const u64 app_id = 0) {
        Save({ Magic, 0, armGetSystemTick(), app_id });
        PushRecent(app_id);
        AddStat(app_id, 0, 1);
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
                const auto delta_ns = armTicksToNs(now - rec.resumed_tick);
                rec.played_ns += delta_ns;
                AddStat(rec.app_id, (u32)(delta_ns / 1'000'000'000ul), 0);
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
