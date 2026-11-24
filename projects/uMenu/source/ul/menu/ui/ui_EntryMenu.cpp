#include <ul/menu/ui/ui_NxDashSession.hpp>
#include <ul/menu/bt/bt_Manager.hpp>
#include <ctime>
#include <cstring>
#include <ul/menu/ui/ui_EntryMenu.hpp>
#include <ul/menu/ui/ui_NxDashText.hpp>
#include <algorithm>
#include <functional>
#include <cmath>
#include <ul/menu/ui/ui_MainMenuLayout.hpp>
#include <ul/menu/ui/ui_MenuApplication.hpp>
#include <ul/menu/smi/smi_Commands.hpp>
#include <ul/util/util_String.hpp>
#include <ul/acc/acc_Accounts.hpp>
#include <ul/util/util_Json.hpp>

extern ul::menu::ui::GlobalSettings g_GlobalSettings;
extern ul::menu::ui::MenuApplication::Ref g_MenuApplication;

namespace ul::menu::ui {

    namespace {

        u32 g_EntryHeightCount = 0;

        void SetEntryHeightCount(const u32 count) {
            UL_ASSERT_TRUE(g_GlobalSettings.config.SetEntry(cfg::ConfigEntryId::MenuEntryHeightCount, static_cast<u64>(count)));
            g_GlobalSettings.SaveConfig();

            g_EntryHeightCount = count;
        }

        inline bool IsEntryHomebrewTakeoverApplication(const Entry &entry) {
            return entry.Is<EntryType::Application>() && (entry.app_info.record.id == g_GlobalSettings.cache_hb_takeover_app_id);
        }

        inline bool IsEntryGameCardApplicationInserted(const Entry &entry) {
            return entry.Is<EntryType::Application>() && entry.app_info.IsGameCardInserted();
        }

        inline bool IsEntryGameCardApplicationNotInserted(const Entry &entry) {
            return entry.Is<EntryType::Application>() && entry.app_info.IsGameCardNotInserted();
        }

        inline bool IsEntryApplicationNeedsVerify(const Entry &entry) {
            return entry.Is<EntryType::Application>() && entry.app_info.NeedsVerify();
        }

        inline bool IsEntryApplicationNotUpdated(const Entry &entry) {
            return entry.Is<EntryType::Application>() && entry.app_info.IsNotUpdated();
        }
        
        inline bool IsEntryApplicationNotLaunchable(const Entry &entry) {
            return entry.Is<EntryType::Application>() && !entry.app_info.CanBeLaunched();
        }

        /*
        inline void SetXOffset(const s32 x_offset) {
            g_MenuApplication->GetMainMenuLayout()->SetBackgroundImageXOffset(x_offset % 1920);
        }
        */

    }

    void EntryMenu::SwipeSingleLeft() {
        const auto old_entry_idx = this->cur_entry_idx;
        this->base_entry_idx_x--;
        this->cur_entry_idx -= g_EntryHeightCount;
        this->cur_entry_change_started_cb();
        this->NotifyFocusedEntryChanged(old_entry_idx);
    }

    void EntryMenu::SwipeSingleRight() {
        const auto old_entry_idx = this->cur_entry_idx;
        this->base_entry_idx_x++;
        this->cur_entry_idx += g_EntryHeightCount;
        this->cur_entry_change_started_cb();
        this->NotifyFocusedEntryChanged(old_entry_idx);
    }

    void EntryMenu::SwipeToPreviousPage() {
        const auto old_entry_idx = this->cur_entry_idx;
        const auto cur_idx_x = this->cur_entry_idx / g_EntryHeightCount;
        const auto cur_idx_y = this->cur_entry_idx % g_EntryHeightCount;

        if(cur_idx_x > 0) {
            if(this->base_entry_idx_x < this->entry_width_count) {
                this->base_entry_idx_x = 0;
            }
            else {
                this->base_entry_idx_x -= this->entry_width_count;
            }
            
            if(this->cur_entry_idx < this->entry_page_count) {
                this->cur_entry_idx = cur_idx_y;
            }
            else {
                this->cur_entry_idx -= this->entry_page_count;
            }

            this->NotifyFocusedEntryChanged(old_entry_idx);
        }
    }

    void EntryMenu::SwipeToNextPage() {
        const auto old_entry_idx = this->cur_entry_idx;
        this->base_entry_idx_x += this->entry_width_count;
        this->cur_entry_idx += this->entry_width_count * g_EntryHeightCount;
        this->NotifyFocusedEntryChanged(old_entry_idx);
    }

    void EntryMenu::SwipeRewind() {
        const auto old_entry_idx = this->cur_entry_idx;
        this->base_entry_idx_x = 0;
        this->cur_entry_idx = 0;

        this->NotifyFocusedEntryChanged(old_entry_idx);
    }

    pu::sdl2::TextureHandle::Ref EntryMenu::LoadEntryIconTexture(const Entry &entry) {
        auto icon_path = entry.control.icon_path;
        if(icon_path.empty()) {
            if(entry.Is<EntryType::Application>()) {
                return LoadApplicationIconTexture(entry.app_info.app_id);
            }
            else if(entry.Is<EntryType::Homebrew>()) {
                icon_path = "ui/Main/EntryIcon/DefaultHomebrew";
            }
            else if(entry.Is<EntryType::Folder>()) {
                return this->folder_entry_icon;
            }
            else if(entry.Is<EntryType::SpecialEntryMiiEdit>()) {
                icon_path = "ui/Main/EntryIcon/MiiEdit";
            }
            else if(entry.Is<EntryType::SpecialEntryWebBrowser>()) {
                icon_path = "ui/Main/EntryIcon/WebBrowser";
            }
            else if(entry.Is<EntryType::SpecialEntryUserPage>()) {
                // Not a theme asset
                return GetSelectedUserIconTexture();
            }
            else if(entry.Is<EntryType::SpecialEntrySettings>()) {
                icon_path = "ui/Main/EntryIcon/Settings";
            }
            else if(entry.Is<EntryType::SpecialEntryThemes>()) {
                icon_path = "ui/Main/EntryIcon/Themes";
            }
            else if(entry.Is<EntryType::SpecialEntryControllers>()) {
                icon_path = "ui/Main/EntryIcon/Controllers";
            }
            else if(entry.Is<EntryType::SpecialEntryAlbum>()) {
                icon_path = "ui/Main/EntryIcon/Album";
            }
            else if(entry.Is<EntryType::SpecialEntryAmiibo>()) {
                icon_path = "ui/Main/EntryIcon/Amiibo";
            }
            return TryFindLoadImageHandle(icon_path);
        }
        else {
            return pu::sdl2::TextureHandle::New(pu::ui::render::LoadImageFromFile(icon_path));
        }
    }

    void EntryMenu::CleanOverTexts() {
        for(auto &over_text_tex: this->entry_over_texts) {
            pu::ui::render::DeleteTexture(over_text_tex);
        }
    }
    
    void EntryMenu::LoadOverText(const u32 idx) {
        if(idx < this->cur_entries.size()) {
            const auto &entry = this->cur_entries.at(idx);
            if(entry.Is<EntryType::Folder>()) {
                auto &over_text_tex = this->entry_over_texts.at(idx);
                if(over_text_tex == nullptr) {
                    const auto side_margin = (u32)(((double)this->entry_size / (double)MinScalableEntrySize) * (double)DefaultOverTextSideMargin);
                    this->entry_over_texts.at(idx) = pu::ui::render::RenderText(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Medium), entry.folder_info.name, g_MenuApplication->GetTextColor(), this->entry_size - 2 * side_margin);
                }
            }
        }
    }

    bool EntryMenu::LoadEntry(const u32 idx) {
        if(idx < this->cur_entries.size()) {
            const auto &entry = this->cur_entries.at(idx);
            if(!entry.Is<EntryType::Invalid>()) {
                this->entry_icons.at(idx) = this->LoadEntryIconTexture(entry);
                this->LoadOverText(idx);

                auto &entry_alpha = this->load_img_entry_alphas.at(idx);
                entry_alpha = 0;
                auto &entry_alpha_incr = this->load_img_entry_incrs.at(idx);
                entry_alpha_incr.StartFromZero(EntryShowIncrementSteps, 0xFF);
                return true;
            }
        }

        return false;
    }

    void EntryMenu::NotifyFocusedEntryChanged(const s32 prev_entry_idx, const s32 is_prev_entry_suspended_override) {
        auto has_prev_entry = prev_entry_idx >= 0;
        bool is_prev_entry_suspended = false;
        if(is_prev_entry_suspended_override >= 0) {
            has_prev_entry = true;
            is_prev_entry_suspended = static_cast<bool>(is_prev_entry_suspended_override);
        }
        else if(prev_entry_idx >= 0) {
            if(((u32)prev_entry_idx < this->cur_entries.size())) {
                const auto &prev_entry = this->cur_entries.at(prev_entry_idx);
                if(prev_entry.type != EntryType::Invalid) {
                    is_prev_entry_suspended = g_GlobalSettings.IsEntrySuspended(prev_entry);
                }
            }
        }

        const auto is_cur_entry_suspended = this->IsFocusedNonemptyEntry() && g_GlobalSettings.IsEntrySuspended(this->GetFocusedEntry());
        this->cur_entry_changed_cb(has_prev_entry, is_prev_entry_suspended, is_cur_entry_suspended);
    }

    void EntryMenu::SetFocusedEntryIndex(const u32 idx) {
        const auto idx_x = idx / g_EntryHeightCount;

        // Set base at the start of the corresponding page
        this->base_entry_idx_x = (idx_x / this->entry_width_count) * this->entry_width_count;
        this->cur_entry_idx = idx;
    }

    Entry &EntryMenu::GetEntry(const s32 index) {
        UL_ASSERT_TRUE(index >= 0);
        UL_ASSERT_TRUE((u32)index < this->cur_entries.size());

        auto &entry = this->cur_entries.at(index);
        UL_ASSERT_TRUE(!entry.Is<EntryType::Invalid>());

        return entry;
    }

    EntryMenu::EntryMenu(const s32 x, const s32 y, const std::string &path, FocusedEntryInputPressedCallback cur_entry_input_cb, FocusedEntryChangedCallback cur_entry_changed_cb, FocusedEntryChangeStartedCallback cur_entry_change_started_cb) : x(x), y(y), cur_entry_idx(0), entry_idx_stack(), cur_entry_input_cb(cur_entry_input_cb), cur_entry_changed_cb(cur_entry_changed_cb), cur_entry_change_started_cb(cur_entry_change_started_cb), enabled(true), selected_entry_idx(-1), empty_entry_icon(nullptr), selected_entry_icon(nullptr), entry_h_margin(EntryVerticalMargin), entries_to_add(), entries_to_remove(), pending_load_img_entry_start_idx(UINT32_MAX), pending_load_img_entry_ext_idx(UINT32_MAX), pending_load_img_done(false), entry_page_count(0), entry_total_count(0), swipe_mode(SwipeMode::None), entries_base_swipe_neg_offset(0), entries_base_swipe_neg_offset_incr(), extra_entry_swipe_show_h_count(0) {
        this->cursor_over_icon = TryFindLoadImage("ui/Main/OverIcon/Cursor");
        this->cursor_size = 0;
        this->border_over_icon = TryFindLoadImage("ui/Main/OverIcon/Border");
        this->border_size = 0;
        this->suspended_over_icon = TryFindLoadImage("ui/Main/OverIcon/Suspended");
        this->suspended_size = 0;
        this->selected_over_icon = TryFindLoadImage("ui/Main/OverIcon/Selected");
        this->selected_size = 0;
        this->hb_takeover_app_over_icon = TryFindLoadImage("ui/Main/OverIcon/HomebrewTakeoverApplication");
        this->hb_takeover_app_size = 0;
        this->gamecard_over_icon = TryFindLoadImage("ui/Main/OverIcon/Gamecard");
        this->gamecard_size = 0;
        this->corrupted_over_icon = TryFindLoadImage("ui/Main/OverIcon/Corrupted");
        this->corrupted_size = 0;
        this->not_launchable_over_icon = TryFindLoadImage("ui/Main/OverIcon/NotLaunchable");
        this->not_launchable_size = 0;
        this->needs_update_over_icon = TryFindLoadImage("ui/Main/OverIcon/NeedsUpdate");
        this->needs_update_size = 0;

        this->empty_entry_icon = TryFindLoadImageHandle("ui/Main/EntryIcon/Empty");
        this->folder_entry_icon = TryFindLoadImageHandle("ui/Main/EntryIcon/Folder");

        this->cur_path = path;

        this->base_entry_idx_x = 0;
        this->cursor_transition_x = 0;
        this->cursor_transition_y = 0;
        this->cursor_transition_x_incr = {};
        this->cursor_transition_y_incr = {};
        this->after_transition_base_entry_idx_x = -1;
        this->after_transition_entry_idx = -1;
    }

    void EntryMenu::Initialize(const u32 last_idx, const std::string &new_path) {
        UL_LOG_INFO("EntryMenu::Initialize start...");
        const auto time = std::chrono::system_clock::now();
        u64 entry_height_count;
        UL_ASSERT_TRUE(g_GlobalSettings.config.GetEntry(cfg::ConfigEntryId::MenuEntryHeightCount, entry_height_count));
        g_EntryHeightCount = entry_height_count;
        this->ComputeSizes(g_EntryHeightCount);

        if(ShelfMode && !this->defer_used && nxdash::On("fast_home") && g_GlobalSettings.IsSuspended() && HasScreenCaptureBackground()) {
            // Back from a game with HOME: the pause sheet only needs the paused game, so the entry list
            // (the slowest part of coming back) is read once the sheet is already on screen
            this->defer_used = true;
            this->defer_pending = true;
            this->defer_last_idx = last_idx;
            this->defer_path = new_path;
            this->defer_frames = 0;
            this->pending_load_img_done = true;
            UL_LOG_INFO("EntryMenu::Initialize deferred (fast HOME)");
            return;
        }
        this->defer_used = true;

        this->entry_idx_stack.push(last_idx);
        this->MoveTo(new_path, true);
        UL_LOG_INFO("EntryMenu initialized in %d ms", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - time).count());
    }

    bool EntryMenu::IsInRoot() {
        return this->cur_path.length() == GetActiveMenuPath().length();
    }

    void EntryMenu::OnRender(pu::ui::render::Renderer::Ref &drawer, const s32 x, const s32 y) {
        const auto time_start = std::chrono::steady_clock::now();
        if(this->lang_gen != nxt::g_LangGen) {
            // The profile language changed: rebuild shelf names and info text
            this->lang_gen = nxt::g_LangGen;
            this->shelf_cats_sig.clear();
            this->ShelfClearInfo();
        }

        const auto loaded_pending_load_img_entry_ext_idx = this->pending_load_img_entry_ext_idx;
        if(!this->pending_load_img_done) {
            const auto has_left = this->pending_load_img_entry_start_idx >= this->pending_load_img_entry_ext_idx;
            if(has_left) {
                this->LoadEntry(this->pending_load_img_entry_start_idx - this->pending_load_img_entry_ext_idx);
            }

            const auto right_load_idx = this->pending_load_img_entry_start_idx + this->pending_load_img_entry_ext_idx;
            const auto has_right = right_load_idx < this->cur_entries.size();
            if(has_right) {
                this->LoadEntry(right_load_idx);
            }

            if(!has_left && !has_right) {
                this->pending_load_img_done = true;
                this->pending_load_img_entry_ext_idx = UINT32_MAX;
            }
            else {
                this->pending_load_img_entry_ext_idx++;
            }
        }

        if(this->entries_base_swipe_neg_offset_incr.Increment(this->entries_base_swipe_neg_offset)) {
            this->entries_base_swipe_neg_offset = 0;
            switch(this->swipe_mode) {
                case SwipeMode::LeftSingle: {
                    break;
                }
                case SwipeMode::RightSingle: {
                    this->SwipeSingleRight();
                    break;
                }
                case SwipeMode::LeftPage: {
                    break;
                }
                case SwipeMode::RightPage: {
                    this->SwipeToNextPage();
                    break;
                }
                case SwipeMode::Rewind: {
                    break;
                }
                default:
                    break;
            }
            this->swipe_mode = SwipeMode::None;
            this->extra_entry_swipe_show_h_count = 0;
        }

        if(ShelfMode) {
            this->ShelfRender(drawer, x, y);
            return;
        }

        const auto cur_actual_entry_idx = this->IsCursorInTransition() ? this->pre_transition_entry_idx : this->cur_entry_idx;
        const auto base_idx = this->base_entry_idx_x * g_EntryHeightCount;

        /*
        const auto x_offset_per_entry = this->entry_size + this->entry_h_margin;
        const auto x_offset = - (((s32)x_offset_per_entry * (s32)this->base_entry_idx_x) + (s32)this->entries_base_swipe_neg_offset);
        SetXOffset((s32)(((double)x_offset) * 1.0f));
        */

        // For the swipe animations, additional items are rendered (some will be outside the visible range)
        const auto entry_render_count = this->entry_page_count + this->extra_entry_swipe_show_h_count * g_EntryHeightCount;
        bool retried_load_app_icon = false;
        for(u32 i = 0; i < entry_render_count; i++) {
            const auto entry_i = base_idx + i;

            pu::sdl2::TextureHandle::Ref entry_icon = {};
            if(entry_i < this->entry_icons.size()) {
                entry_icon = this->entry_icons.at(entry_i);
            }

            const auto is_empty = (entry_i >= this->entry_icons.size()) || (entry_i >= this->cur_entries.size()) || ((entry_i < this->cur_entries.size()) && this->cur_entries.at(entry_i).Is<EntryType::Invalid>());

            const auto idx_x = i / g_EntryHeightCount;
            const auto idx_y = i % g_EntryHeightCount;
            const auto entry_x = x + HorizontalSideMargin + idx_x * (this->entry_size + this->entry_h_margin);
            const auto entry_y = y + this->v_side_margin + idx_y * (this->entry_size + EntryVerticalMargin);

            drawer->RenderTexture(this->empty_entry_icon->Get(), (s32)entry_x - (s32)this->entries_base_swipe_neg_offset, entry_y, pu::ui::render::TextureRenderOptions({}, this->entry_size, this->entry_size, {}, {}, {}));

            if(!is_empty) {
                auto &entry = this->cur_entries.at(entry_i);

                // Special check for applications with the icon not yet loaded
                if(this->pending_load_img_done && entry.Is<EntryType::Application>() && (entry_icon == nullptr) && !retried_load_app_icon) {
                    // Try loading the icon again
                    entry_icon = LoadApplicationIconTexture(entry.app_info.app_id);
                    if(entry_icon != nullptr) {
                        entry.TryLoadNacp();
                        this->entry_icons.at(entry_i) = entry_icon;
                    }
                    else {
                        entry_icon = GetDefaultApplicationIconTexture();
                    }

                    retried_load_app_icon = true;
                }

                auto &entry_alpha = this->load_img_entry_alphas.at(entry_i);
                auto &entry_alpha_incr = this->load_img_entry_incrs.at(entry_i);
                entry_alpha_incr.Increment(entry_alpha);

                #define _DRAW_OVER_ICON(name) { \
                    const auto over_icon_x = (s32)(entry_x - (std::abs((s32)(this->name##_size - this->entry_size)) / 2)) - (s32)this->entries_base_swipe_neg_offset; \
                    const auto over_icon_y = entry_y - (std::abs((s32)(this->name##_size - this->entry_size)) / 2); \
                    drawer->RenderTexture(this->name##_over_icon, over_icon_x, over_icon_y, pu::ui::render::TextureRenderOptions({}, this->name##_size, this->name##_size, {}, {}, {})); \
                }

                if(entry_icon != nullptr) {
                    drawer->RenderTexture(entry_icon->Get(), (s32)entry_x - (s32)this->entries_base_swipe_neg_offset, entry_y, pu::ui::render::TextureRenderOptions(entry_alpha, this->entry_size, this->entry_size, {}, {}, {}));

                    const auto progress = this->entry_progresses.at(entry_i);
                    if(!std::isnan(progress)) {
                        const auto bar_width = this->entry_size;
                        const auto bar_height = (u32)(((double)this->entry_size / (double)MinScalableEntrySize) * (double)DefaultProgressBarHeight);
                        drawer->RenderRectangleFill(ProgressBackgroundColor, (s32)entry_x - (s32)this->entries_base_swipe_neg_offset, entry_y + this->entry_size - bar_height, bar_width, bar_height);

                        const auto progress_width = (u32)((float)bar_width * progress);
                        drawer->RenderRectangleFill(ProgressForegroundColor, (s32)entry_x - (s32)this->entries_base_swipe_neg_offset, entry_y + this->entry_size - bar_height, progress_width, bar_height);
                    }

                    if(entry.Is<EntryType::Homebrew>() || entry.Is<EntryType::Application>() || entry.IsSpecial()) {
                        _DRAW_OVER_ICON(border);
                    }

                    if(IsEntryHomebrewTakeoverApplication(entry)) {
                        _DRAW_OVER_ICON(hb_takeover_app);
                    }

                    if(IsEntryGameCardApplicationInserted(entry)) {
                        _DRAW_OVER_ICON(gamecard);
                    }

                    if(IsEntryApplicationNeedsVerify(entry)) {
                        _DRAW_OVER_ICON(corrupted);
                    }
                    else if(IsEntryApplicationNotLaunchable(entry)) {
                        _DRAW_OVER_ICON(not_launchable);
                    }
                    else if(IsEntryApplicationNotUpdated(entry)) {
                        _DRAW_OVER_ICON(needs_update);
                    }

                    if(g_GlobalSettings.IsEntrySuspended(entry)) {
                        _DRAW_OVER_ICON(suspended);
                    }

                    this->LoadOverText(entry_i);
                    auto over_text_tex = this->entry_over_texts.at(entry_i);
                    if(over_text_tex != nullptr) {
                        const auto text_width = pu::ui::render::GetTextureWidth(over_text_tex);
                        const auto text_height = pu::ui::render::GetTextureHeight(over_text_tex);
                        drawer->RenderTexture(over_text_tex, (s32)entry_x - (s32)this->entries_base_swipe_neg_offset + (u32)((double)(this->entry_size - text_width) / 2.0f), entry_y + (u32)((double)(this->entry_size - text_height) / 2.0f));
                    }

                    if(this->IsEntrySelected(entry_i)) {
                        _DRAW_OVER_ICON(selected);
                    }
                }
            }

            if(loaded_pending_load_img_entry_ext_idx != UINT32_MAX) {
                if((this->pending_load_img_entry_start_idx + loaded_pending_load_img_entry_ext_idx) == this->cur_entry_idx) {
                    this->NotifyFocusedEntryChanged(-1);
                }
                if((this->pending_load_img_entry_start_idx >= loaded_pending_load_img_entry_ext_idx) && ((this->pending_load_img_entry_start_idx - loaded_pending_load_img_entry_ext_idx) == this->cur_entry_idx)) {
                    this->NotifyFocusedEntryChanged(-1);
                }
            }
        }

        const auto cur_actual_entry_idx_x = (cur_actual_entry_idx / g_EntryHeightCount) - this->base_entry_idx_x;
        const auto cur_actual_entry_idx_y = cur_actual_entry_idx % g_EntryHeightCount;

        // Cursor is an special over icon
        const auto cursor_x = x + HorizontalSideMargin + cur_actual_entry_idx_x * (this->entry_size + this->entry_h_margin) - (std::abs((s32)(this->cursor_size - this->entry_size)) / 2) + (this->IsCursorInTransition() ? this->cursor_transition_x : 0);
        const auto cursor_y = y + this->v_side_margin + cur_actual_entry_idx_y * (this->entry_size + EntryVerticalMargin) - (std::abs((s32)(this->cursor_size - this->entry_size)) / 2) + (this->IsCursorInTransition() ? this->cursor_transition_y : 0);
        drawer->RenderTexture(this->cursor_over_icon, cursor_x, cursor_y, pu::ui::render::TextureRenderOptions({}, this->cursor_size, this->cursor_size, {}, {}, {}));

        if((this->selected_entry_idx >= 0) && (this->selected_entry_icon != nullptr)) {
            const auto entry_size_factor = (double)this->entry_size / (double)MinScalableEntrySize;
            const auto over_icon_offset = (u32)(entry_size_factor * (double)DefaultMoveOverIconOffset);
            drawer->RenderTexture(this->selected_entry_icon->Get(), cursor_x - over_icon_offset, cursor_y - over_icon_offset, pu::ui::render::TextureRenderOptions(MoveOverIconAlpha, this->entry_size, this->entry_size, {}, {}, {}));
        }

        if(this->IsCursorInTransition()) {
            if(this->cursor_transition_x_incr.Increment(this->cursor_transition_x) || this->cursor_transition_y_incr.Increment(this->cursor_transition_y)) {
                this->cursor_transition_x = 0;
                this->cursor_transition_y = 0;

                if(this->after_transition_base_entry_idx_x >= 0) {
                    this->base_entry_idx_x = this->after_transition_base_entry_idx_x;
                    this->NotifyFocusedEntryChanged(this->pre_transition_entry_idx);
                }

                if(this->after_transition_entry_idx >= 0) {
                    this->cur_entry_idx = this->after_transition_entry_idx;
                    this->NotifyFocusedEntryChanged(this->pre_transition_entry_idx);
                }

                this->after_transition_base_entry_idx_x = -1;
                this->after_transition_entry_idx = -1;
            }
        }

        const auto time_end = std::chrono::steady_clock::now();
        const auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(time_end - time_start).count();
        UL_LOG_INFO("EntryMenu::OnRender took %d ms", time_diff);
    }

    void EntryMenu::OnInput(const u64 keys_down, const u64 keys_up, const u64 keys_held, const pu::ui::TouchPoint touch_pos) {
        if(ShelfMode && this->defer_pending) {
            this->defer_frames++;
            // Give the pause sheet time to finish arriving; if it was dismissed already, load right away
            const auto sheet_settled = !this->sheet_open || (this->sheet_anim >= 0.985);
            if((this->defer_frames >= 3) && this->sheet_checked && sheet_settled && (this->launch_t < 0.0)) {
                this->DeferredLoadNow();
            }
        }
        if(this->IsCursorInTransition()) {
            return;
        }
        if(!this->enabled) {
            return;
        }

        if(ShelfMode) {
            this->ShelfInput(keys_down, keys_held, touch_pos);
            return;
        }

        const auto cursor_transition_abs_incr_x = this->entry_h_margin + this->entry_size;
        const auto cursor_transition_abs_incr_y = EntryVerticalMargin + this->entry_size;

        if((keys_down & HidNpadButton_Up) || (keys_held & HidNpadButton_StickLUp) || (keys_held & HidNpadButton_StickRUp)) {
            const auto cur_entry_idx_y = this->cur_entry_idx % g_EntryHeightCount;

            if(cur_entry_idx_y > 0) {
                if(this->IsNotInLargeSwipe()) {
                    this->pre_transition_entry_idx = this->cur_entry_idx;
                    this->after_transition_entry_idx = this->cur_entry_idx - 1;
                    this->cursor_transition_y_incr.Start(CursorTransitionIncrementSteps, 0, -cursor_transition_abs_incr_y);
                    this->cur_entry_change_started_cb();
                }
            }
        }
        else if((keys_down & HidNpadButton_Down) || (keys_held & HidNpadButton_StickLDown) || (keys_held & HidNpadButton_StickRDown)) {
            const auto cur_entry_idx_y = this->cur_entry_idx % g_EntryHeightCount;
            if((cur_entry_idx_y + 1) < g_EntryHeightCount) {
                if(this->IsNotInLargeSwipe()) {
                    this->pre_transition_entry_idx = this->cur_entry_idx;
                    this->after_transition_entry_idx = this->cur_entry_idx + 1;
                    this->cursor_transition_y_incr.Start(CursorTransitionIncrementSteps, 0, cursor_transition_abs_incr_y);
                    this->cur_entry_change_started_cb();
                }
            }
        }
        else if((keys_down & HidNpadButton_Left) || (keys_held & HidNpadButton_StickLLeft) || (keys_held & HidNpadButton_StickRLeft)) {
            const auto rel_entry_idx_x = (this->cur_entry_idx / g_EntryHeightCount) - this->base_entry_idx_x;
            if(rel_entry_idx_x > 0) {
                if(this->IsNotInLargeSwipe()) {
                    this->pre_transition_entry_idx = this->cur_entry_idx;
                    this->after_transition_entry_idx = this->cur_entry_idx - g_EntryHeightCount;
                    this->cursor_transition_x_incr.Start(CursorTransitionIncrementSteps, 0, -cursor_transition_abs_incr_x);
                    this->cur_entry_change_started_cb();
                }
            }
            else if((rel_entry_idx_x == 0) && (this->base_entry_idx_x > 0)) {
                if(this->IsNotInSwipe()) {
                    this->SwipeSingleLeft();

                    this->entries_base_swipe_neg_offset = this->entry_size + this->entry_h_margin /* x1 */;
                    this->entries_base_swipe_neg_offset_incr.StartToZero(EntriesSwipeSingleIncrementSteps, this->entries_base_swipe_neg_offset);
                    this->swipe_mode = SwipeMode::LeftSingle;
                }
            }
        }
        else if((keys_down & HidNpadButton_Right) || (keys_held & HidNpadButton_StickLRight) || (keys_held & HidNpadButton_StickRRight)) {
            const auto rel_entry_idx_x = (this->cur_entry_idx / g_EntryHeightCount) - this->base_entry_idx_x;
            if((rel_entry_idx_x + 1) < this->entry_width_count) {
                if(this->IsNotInLargeSwipe()) {
                    this->pre_transition_entry_idx = this->cur_entry_idx;
                    this->after_transition_entry_idx = this->cur_entry_idx + g_EntryHeightCount;
                    this->cursor_transition_x_incr.Start(CursorTransitionIncrementSteps, 0, cursor_transition_abs_incr_x);
                    this->cur_entry_change_started_cb();
                }
            }
            else if((rel_entry_idx_x + 1) == this->entry_width_count) {
                if(this->IsNotInSwipe()) {
                    this->entries_base_swipe_neg_offset = 0;
                    this->entries_base_swipe_neg_offset_incr.StartFromZero(EntriesSwipeSingleIncrementSteps, this->entry_size + this->entry_h_margin /* x1 */);
                    this->swipe_mode = SwipeMode::RightSingle;  
                }
            }
        }
        else {
            this->cur_entry_input_cb(keys_down);
        }
    }

    void EntryMenu::MoveTo(const std::string &new_path, const bool force_pop_idx) {
        UL_LOG_INFO("EntryMenu::MoveTo start...");
        const auto time = std::chrono::system_clock::now();

        if(this->defer_pending) {
            // Something asked for a (re)load before the deferred one ran: this call does that job
            this->defer_pending = false;
            this->entry_idx_stack.push(this->defer_last_idx);
        }
        this->collector.clear();
        this->collector_next = 0;

        const auto reloading = new_path.empty();
        const auto going_back = new_path.length() < this->cur_path.length();
        const auto old_entry_idx = this->cur_entry_idx;

        s32 is_prev_entry_suspended_override = -1;
        if(!reloading) {
            if(this->IsFocusedNonemptyEntry()) {
                is_prev_entry_suspended_override = static_cast<s32>(g_GlobalSettings.IsEntrySuspended(this->GetFocusedEntry()));
            }

            this->cur_path = new_path;
            this->base_entry_idx_x = 0;
            this->cur_entry_idx = 0;
        }

        if((going_back && !this->entry_idx_stack.empty()) || force_pop_idx) {
            const auto idx = this->entry_idx_stack.top();
            this->SetFocusedEntryIndex(idx);
            this->entry_idx_stack.pop();
        }

        this->cur_entries.clear();
        this->cur_entries = LoadEntries(this->cur_path);
        this->OrganizeEntries();
        this->ComputeSizes(g_EntryHeightCount);
    
        for(auto &entry_icon : this->entry_icons) {
            if(entry_icon != this->selected_entry_icon) {
                entry_icon = {};
            }
        }
        for(auto &over_text : this->entry_over_texts) {
            pu::ui::render::DeleteTexture(over_text);
        }
        this->ShelfClearTitles();
        this->shelf_synced = false;

        this->entry_icons.clear();
        this->entry_over_texts.clear();
        this->entry_progresses.clear();
        this->load_img_entry_alphas.clear();
        this->load_img_entry_incrs.clear();
        for(u32 i = 0; i < this->entry_total_count; i++) {
            this->entry_icons.push_back(nullptr);
            this->entry_over_texts.push_back(nullptr);
            this->entry_progresses.push_back(NAN);
            this->load_img_entry_alphas.push_back(0xFF);
            this->load_img_entry_incrs.push_back({});
        }

        this->pending_load_img_entry_start_idx = this->cur_entry_idx;
        this->LoadEntry(this->cur_entry_idx);
        this->NotifyFocusedEntryChanged(-1, is_prev_entry_suspended_override);
        this->pending_load_img_entry_ext_idx = 1;
        this->pending_load_img_done = false;

        if(!reloading && !going_back) {
            this->entry_idx_stack.push(old_entry_idx);
        }
        UL_LOG_INFO("EntryMenu::MoveTo took %d ms", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - time).count());
    }

    void EntryMenu::OrganizeEntries() {
        std::vector<Entry> tmp_entries;
        
        for(const auto &entry: this->cur_entries) {
            if(!entry.Is<EntryType::Invalid>()) {
                while(tmp_entries.size() < (entry.index + 1)) {
                    auto &tmp_entry = tmp_entries.emplace_back();
                    tmp_entry.type = EntryType::Invalid;
                }

                tmp_entries.at(entry.index) = entry;
            }
        }
        
        for(const auto &rem_entry: this->entries_to_remove) {
            UL_LOG_WARN("Removing entry of type %d at %d", (u32)rem_entry.type, rem_entry.index);
            // Otherwise no need to add an entry at all
            if(rem_entry.index < tmp_entries.size()) {
                auto &tmp_entry = tmp_entries.at(rem_entry.index);
                tmp_entry.type = EntryType::Invalid;
            }
            this->entry_icons.at(rem_entry.index) = {};
            pu::ui::render::DeleteTexture(this->entry_over_texts.at(rem_entry.index));
        }

        for(const auto &add_entry: this->entries_to_add) {
            UL_LOG_WARN("Adding entry of type %d at %d", (u32)add_entry.type, add_entry.index);
            if(add_entry.type != EntryType::Invalid) {
                while(tmp_entries.size() < (add_entry.index + 1)) {
                    auto &tmp_entry = tmp_entries.emplace_back();
                    tmp_entry.type = EntryType::Invalid;
                }

                tmp_entries.at(add_entry.index) = add_entry;
            }
        }

        std::swap(this->cur_entries, tmp_entries);
        tmp_entries.clear();

        this->ComputeSizes(g_EntryHeightCount);
        while(this->entry_icons.size() < this->entry_total_count) {
            this->entry_icons.push_back(nullptr);
            this->entry_over_texts.push_back(nullptr);
            this->entry_progresses.push_back(NAN);
            this->load_img_entry_alphas.push_back(0xFF);
            this->load_img_entry_incrs.push_back({});
        }

        for(const auto &add_entry: this->entries_to_add) {
            this->LoadEntry(add_entry.index);
        }

        this->entries_to_add.clear();
        this->entries_to_remove.clear();
    }

    void EntryMenu::OrganizeUpdateEntries(const s32 is_prev_entry_suspended_override) {
        // Force reset all over texts
        for(auto &over_text_tex: this->entry_over_texts) {
            pu::ui::render::DeleteTexture(over_text_tex);
        }

        this->OrganizeEntries();
        this->NotifyFocusedEntryChanged(-1, is_prev_entry_suspended_override);
    }

    void EntryMenu::NotifyEntryAdded(const Entry &entry) {
        this->entries_to_add.push_back(entry);
    }

    void EntryMenu::NotifyEntryRemoved(const Entry &entry) {
        UL_ASSERT_TRUE(entry.index < this->cur_entries.size());

        this->entries_to_remove.push_back(entry);
    }

    bool EntryMenu::IsFocusedNonemptyEntry() {
        return (this->cur_entry_idx < this->cur_entries.size()) && !this->cur_entries.at(this->cur_entry_idx).Is<EntryType::Invalid>();
    }

    void EntryMenu::UpdateEntryProgress(const u32 idx, const float progress) {
        UL_ASSERT_TRUE(idx < this->cur_entries.size());

        this->entry_progresses.at(idx) = progress;
    }

    void EntryMenu::IncrementEntryHeightCount() {
        auto h_count = g_EntryHeightCount;
        h_count++;

        g_MenuApplication->SetBackgroundFade();
        g_MenuApplication->FadeOut();
        if(!this->entry_idx_stack.empty()) {
            this->entry_idx_stack.top() = 0;
        }
        SetEntryHeightCount(h_count);
        this->ComputeSizes(h_count);

        this->base_entry_idx_x = 0;
        this->cur_entry_idx = 0;

        this->CleanOverTexts();
        this->NotifyFocusedEntryChanged(-1);
        g_MenuApplication->FadeIn();
    }

    bool EntryMenu::CanDecrementEntryHeightCount() {
        return g_EntryHeightCount > 1;
    }
    
    void EntryMenu::DecrementEntryHeightCount() {
        auto h_count = g_EntryHeightCount;
        if(h_count > 1) {
            h_count--;

            g_MenuApplication->SetBackgroundFade();
            g_MenuApplication->FadeOut();
            this->ComputeSizes(h_count);

            const auto new_count = h_count * this->entry_width_count;
            if(new_count == 0) {
                h_count++;
                this->ComputeSizes(h_count);
            }
            else {
                if(!this->entry_idx_stack.empty()) {
                    this->entry_idx_stack.top() = 0;
                }
                SetEntryHeightCount(h_count);

                this->base_entry_idx_x = 0;
                this->cur_entry_idx = 0;

                this->CleanOverTexts();
                this->NotifyFocusedEntryChanged(-1);
            }
            g_MenuApplication->FadeIn();
        }
    }

    bool EntryMenu::MoveToPreviousPage() {
        if((this->base_entry_idx_x > 0) && this->IsNotInSwipe()) {
            const auto prev_base_entry_idx_x = this->base_entry_idx_x;
            this->SwipeToPreviousPage();
            this->extra_entry_swipe_show_h_count = std::min(this->entry_width_count, prev_base_entry_idx_x);
            this->entries_base_swipe_neg_offset = (this->entry_size + this->entry_h_margin) * this->extra_entry_swipe_show_h_count;
            this->entries_base_swipe_neg_offset_incr.StartToZero(EntriesSwipePageIncrementSteps, this->entries_base_swipe_neg_offset);
            this->swipe_mode = SwipeMode::LeftPage;
            return true;
        }
        else {
            return false;
        }
    }

    bool EntryMenu::MoveToNextPage() {
        if(this->IsNotInSwipe()) {
            this->extra_entry_swipe_show_h_count = this->entry_width_count;
            this->entries_base_swipe_neg_offset = 0;
            this->entries_base_swipe_neg_offset_incr.StartFromZero(EntriesSwipePageIncrementSteps, (this->entry_size + this->entry_h_margin) * this->extra_entry_swipe_show_h_count);
            this->swipe_mode = SwipeMode::RightPage;  
            return true;
        }
        else {
            return false;
        }
    }

    void EntryMenu::Rewind() {
        if((this->cur_entry_idx > 0) && this->IsNotInSwipe()) {
            const auto prev_base_entry_idx_x = this->base_entry_idx_x;
            this->SwipeRewind();
            if(prev_base_entry_idx_x > 0) {
                // No need to animate anything if we're already in the first page
                this->extra_entry_swipe_show_h_count = prev_base_entry_idx_x;
                this->entries_base_swipe_neg_offset = (this->entry_size + this->entry_h_margin) * this->extra_entry_swipe_show_h_count;
                this->entries_base_swipe_neg_offset_incr.StartToZero(EntriesSwipeRewindIncrementSteps, this->entries_base_swipe_neg_offset);
                this->swipe_mode = SwipeMode::Rewind;
            }
        }
    }

    void EntryMenu::ResetSelection() {
        this->selected_entry_idx = -1;
        this->selected_entry_icon = nullptr;
    }

    void EntryMenu::SetEntrySelected(const u32 idx, const bool selected) {
        this->ResetSelection();
        if(selected) {
            UL_ASSERT_TRUE(idx < this->cur_entries.size());
            this->selected_entry_idx = idx;
            this->selected_entry_icon = this->entry_icons.at(idx);
        }
    }

    bool EntryMenu::IsEntrySelected(const u32 idx) {
        return this->selected_entry_idx == (s32)idx;
    }

    bool EntryMenu::IsAnySelected() {
        return this->selected_entry_idx != -1;
    }

    bool EntryMenu::IsFocusedEntrySuspended() {
        if(!this->IsFocusedNonemptyEntry()) {
            return false;
        }
        else {
            return g_GlobalSettings.IsEntrySuspended(this->GetFocusedEntry());
        }
    }


    // ======================================================================
    // NX Dash shelf mode
    // Cases stand on a shallow arc: the further from the focused entry, the
    // smaller, darker and lower they sit. The focused entry opens up into a
    // full "cover" (its icon), the rest show a spine with the title.
    // ======================================================================

    namespace {

        constexpr pu::ui::Color ShelfCaseColor = { 0xD3, 0x24, 0x1C, 0xFF };
        constexpr pu::ui::Color ShelfLabelColor = { 0xF4, 0xF2, 0xEE, 0xFF };
        constexpr pu::ui::Color ShelfTitleColor = { 0x14, 0x14, 0x14, 0xFF };
        // Last three games played / the game that is running right now
        constexpr pu::ui::Color ShelfRecentColor = { 0x6C, 0x3F, 0xD8, 0xFF };
        constexpr pu::ui::Color ShelfActiveColor = { 0xE9, 0xB3, 0x3C, 0xFF };

        // Pre-rendered shapes: gfx rounded boxes are drawn line by line and were the main per-frame cost
        SDL_Texture *g_Corners[4] = {};
        SDL_Texture *g_SoftShadow = nullptr;
        SDL_Texture *g_Sheen = nullptr;
        SDL_Texture *g_FadeDown = nullptr;
        SDL_Texture *g_FadeUp = nullptr;
        SDL_Texture *g_GlossDown = nullptr;
        SDL_Texture *g_Glow = nullptr;
        bool g_ShapesReady = false;

        SDL_Texture *MakeAlphaTexture(const s32 w, const s32 h, const std::function<double(s32, s32)> &alpha_fn, const u8 gray) {
            auto surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
            if(surface == nullptr) {
                return nullptr;
            }
            SDL_LockSurface(surface);
            for(s32 py = 0; py < h; py++) {
                auto row = reinterpret_cast<u8*>(surface->pixels) + py * surface->pitch;
                for(s32 px = 0; px < w; px++) {
                    row[px * 4 + 0] = gray;
                    row[px * 4 + 1] = gray;
                    row[px * 4 + 2] = gray;
                    row[px * 4 + 3] = (u8)std::clamp(alpha_fn(px, py) * 255.0, 0.0, 255.0);
                }
            }
            SDL_UnlockSurface(surface);
            auto tex = SDL_CreateTextureFromSurface(pu::ui::render::GetMainRenderer(), surface);
            SDL_FreeSurface(surface);
            if(tex != nullptr) {
                SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
            }
            return tex;
        }

        void EnsureShapes() {
            if(g_ShapesReady) {
                return;
            }
            g_ShapesReady = true;
            constexpr s32 R = 48;
            for(s32 c = 0; c < 4; c++) {
                // Circle centre sits on the inner corner of each quadrant
                const double ccx = ((c == 0) || (c == 2)) ? (double)R : 0.0;
                const double ccy = (c < 2) ? (double)R : 0.0;
                g_Corners[c] = MakeAlphaTexture(R, R, [&](const s32 px, const s32 py) {
                    const auto dx = (px + 0.5) - ccx;
                    const auto dy = (py + 0.5) - ccy;
                    return (double)R - std::sqrt(dx * dx + dy * dy) + 0.5;
                }, 0xFF);
            }
            g_SoftShadow = MakeAlphaTexture(128, 48, [](const s32 px, const s32 py) {
                const auto dx = ((px + 0.5) - 64.0) / 64.0;
                const auto dy = ((py + 0.5) - 24.0) / 24.0;
                const auto d2 = dx * dx + dy * dy;
                return (d2 >= 1.0) ? 0.0 : (1.0 - d2) * (1.0 - d2);
            }, 0x00);
            // Vertical scrims: a hard-edged band would show as a line across the ambient light
            g_FadeDown = MakeAlphaTexture(4, 64, [](const s32, const s32 py) {
                const auto v = 1.0 - (py + 0.5) / 64.0;
                return v * v;
            }, 0x06);
            g_FadeUp = MakeAlphaTexture(4, 64, [](const s32, const s32 py) {
                const auto v = (py + 0.5) / 64.0;
                return v * v;
            }, 0x06);
            g_GlossDown = MakeAlphaTexture(4, 64, [](const s32, const s32 py) {
                const auto v = 1.0 - (py + 0.5) / 64.0;
                return v * v;
            }, 0xFF);
            g_Glow = MakeAlphaTexture(128, 48, [](const s32 px, const s32 py) {
                const auto dx = ((px + 0.5) - 64.0) / 64.0;
                const auto dy = ((py + 0.5) - 24.0) / 24.0;
                const auto d2 = dx * dx + dy * dy;
                return (d2 >= 1.0) ? 0.0 : (1.0 - d2) * (1.0 - d2);
            }, 0xFF);
            g_Sheen = MakeAlphaTexture(96, 4, [](const s32 px, const s32) {
                const auto dx = ((px + 0.5) - 48.0) / 48.0;
                const auto v = std::max(0.0, 1.0 - dx * dx);
                return v * v;
            }, 0xFF);
        }

        // Rounded box: four textured corners + three plain fills
        void FillRR(pu::ui::render::Renderer::Ref &drawer, const pu::ui::Color clr, const s32 x, const s32 y, const s32 w, const s32 h, const s32 radius) {
            if((w <= 0) || (h <= 0) || (clr.a == 0)) {
                return;
            }
            EnsureShapes();
            const auto r = std::min({ radius, w / 2, h / 2 });
            if((r <= 2) || (g_Corners[0] == nullptr) || (g_Corners[1] == nullptr) || (g_Corners[2] == nullptr) || (g_Corners[3] == nullptr)) {
                drawer->RenderRectangleFill(clr, x, y, w, h);
                return;
            }
            const s32 xs[4] = { x, x + w - r, x, x + w - r };
            const s32 ys[4] = { y, y, y + h - r, y + h - r };
            for(s32 c = 0; c < 4; c++) {
                SDL_SetTextureColorMod(g_Corners[c], clr.r, clr.g, clr.b);
                drawer->RenderTexture(g_Corners[c], xs[c], ys[c], pu::ui::render::TextureRenderOptions(clr.a, r, r, {}, {}, {}));
            }
            if(w > 2 * r) {
                drawer->RenderRectangleFill(clr, x + r, y, w - 2 * r, r);
                drawer->RenderRectangleFill(clr, x + r, y + h - r, w - 2 * r, r);
            }
            if(h > 2 * r) {
                drawer->RenderRectangleFill(clr, x, y + r, w, h - 2 * r);
            }
        }

        // Fake "liquid glass" (no realtime blur here): bright rim, tinted body, specular line on top
        inline void RenderGlass(pu::ui::render::Renderer::Ref &drawer, const s32 x, const s32 y, const s32 w, const s32 h, const s32 r, const double k, const u8 body_alpha) {
            const auto a = [&](const double v) { return (u8)std::clamp(v * k, 0.0, 255.0); };
            FillRR(drawer, { 0xFF, 0xFF, 0xFF, a(64.0) }, x - 1, y - 1, w + 2, h + 2, r + 1);
            FillRR(drawer, { 0x14, 0x16, 0x1E, a((double)body_alpha) }, x, y, w, h, r);
            FillRR(drawer, { 0xFF, 0xFF, 0xFF, a(16.0) }, x, y, w, h, r);
            if(w > 2 * r + 8) {
                drawer->RenderRectangleFill({ 0xFF, 0xFF, 0xFF, a(70.0) }, x + r, y + 1, w - 2 * r, 1);
            }
        }

        // Below this many games the shelf is a plain row (looping 2-3 cases looks broken)
        constexpr s32 ShelfLoopMin = 7;

        inline double SmoothStep(const double v) {
            const auto c = std::clamp(v, 0.0, 1.0);
            return c * c * (3.0 - 2.0 * c);
        }

    }

    void EntryMenu::ShelfClearTitles() {
        for(auto &tex : this->shelf_title_texts) {
            pu::ui::render::DeleteTexture(tex);
        }
        this->shelf_title_texts.clear();
    }

    void EntryMenu::ShelfRebuildValid() {
        this->ShelfRebuildCategories();

        this->shelf_valid.clear();
        for(u32 i = 0; i < this->cur_entries.size(); i++) {
            if(this->ShelfEntryMatches(this->cur_entries.at(i))) {
                this->shelf_valid.push_back(i);
            }
        }

        if(this->IsInRoot() && (this->shelf_root_filter == ShelfFilter::Recent)) {
            // Most recent first
            std::stable_sort(this->shelf_valid.begin(), this->shelf_valid.end(), [&](const u32 a, const u32 b) {
                return session::RecentRank(this->cur_entries.at(a).app_info.app_id) < session::RecentRank(this->cur_entries.at(b).app_info.app_id);
            });
        }

        if(this->shelf_title_texts.size() != this->cur_entries.size()) {
            this->ShelfClearTitles();
            this->shelf_title_texts.resize(this->cur_entries.size(), nullptr);
        }

        // Keep the shelf position in sync with the real focused entry (e.g. after MoveTo / category changes)
        const auto nv = (s32)this->shelf_valid.size();
        if(nv > 0) {
            const auto cur_pos = ((this->shelf_target % nv) + nv) % nv;
            if(this->shelf_restore_pending || !this->shelf_synced || (this->shelf_valid.at(cur_pos) != this->cur_entry_idx)) {
                s32 found = -1;
                if(this->shelf_restore_pending) {
                    const auto it = this->shelf_cat_pos.find(this->ShelfCatKey());
                    found = (it != this->shelf_cat_pos.end()) ? std::clamp(it->second, 0, nv - 1) : 0;
                }
                else {
                    for(s32 i = 0; i < nv; i++) {
                        if(this->shelf_valid.at(i) == this->cur_entry_idx) {
                            found = i;
                            break;
                        }
                    }
                }
                if(found < 0) {
                    found = 0;
                }
                this->shelf_restore_pending = false;
                this->shelf_target = found;
                this->shelf_s = (double)found;
                this->shelf_synced = true;
                if(this->shelf_valid.at(found) != this->cur_entry_idx) {
                    const auto prev_idx = (s32)this->cur_entry_idx;
                    this->SetFocusedEntryIndex(this->shelf_valid.at(found));
                    this->NotifyFocusedEntryChanged(prev_idx);
                }
            }
        }
        else {
            this->shelf_restore_pending = false;
        }
    }

    std::string EntryMenu::ShelfCatKey() {
        return this->cur_path + "#" + std::to_string(static_cast<u32>(this->IsInRoot() ? this->shelf_root_filter : ShelfFilter::Folder));
    }

    bool EntryMenu::ShelfEntryMatches(const Entry &entry) {
        if(entry.Is<EntryType::Invalid>() || entry.Is<EntryType::Folder>()) {
            return false;
        }
        if(!this->IsInRoot()) {
            return true;
        }
        switch(this->shelf_root_filter) {
            case ShelfFilter::Games:
                return entry.Is<EntryType::Application>();
            case ShelfFilter::Homebrew:
                return entry.Is<EntryType::Homebrew>();
            case ShelfFilter::System:
                return entry.IsSpecial();
            case ShelfFilter::Recent:
                return entry.Is<EntryType::Application>() && (session::RecentRank(entry.app_info.app_id) >= 0);
            default:
                return false;
        }
    }

    void EntryMenu::ShelfRebuildCategories() {
        const auto sig = this->cur_path + ":" + std::to_string(this->cur_entries.size());
        if(sig == this->shelf_cats_sig) {
            return;
        }
        this->shelf_cats_sig = sig;

        const auto &root = GetActiveMenuPath();
        std::vector<Entry> loaded;
        const std::vector<Entry> *root_entries = nullptr;
        if(this->IsInRoot()) {
            root_entries = &this->cur_entries;
        }
        else if(!this->shelf_root_loaded) {
            loaded = LoadEntries(root);
            root_entries = &loaded;
        }
        if(root_entries != nullptr) {
            this->shelf_root_loaded = true;
            this->shelf_root_folders.clear();
            this->shelf_root_has_hb = false;
            this->shelf_root_has_special = false;
            this->shelf_root_has_recent = false;
            for(const auto &entry : *root_entries) {
                if(entry.Is<EntryType::Application>() && (session::RecentRank(entry.app_info.app_id) >= 0)) {
                    this->shelf_root_has_recent = true;
                }
                if(entry.Is<EntryType::Folder>()) {
                    this->shelf_root_folders.push_back({ entry.folder_info.name, entry.GetFolderPath() });
                }
                else if(entry.Is<EntryType::Homebrew>()) {
                    this->shelf_root_has_hb = true;
                }
                else if(entry.IsSpecial()) {
                    this->shelf_root_has_special = true;
                }
            }
        }

        this->shelf_cats.clear();
        if(this->shelf_root_has_recent) {
            this->shelf_cats.push_back({ nxt::T("cat_recent"), root, ShelfFilter::Recent });
        }
        this->shelf_cats.push_back({ nxt::T("cat_games"), root, ShelfFilter::Games });
        for(const auto &[name, path] : this->shelf_root_folders) {
            this->shelf_cats.push_back({ name, path, ShelfFilter::Folder });
        }
        if(this->shelf_root_has_hb) {
            this->shelf_cats.push_back({ "Homebrew", root, ShelfFilter::Homebrew });
        }
        if(this->shelf_root_has_special) {
            this->shelf_cats.push_back({ nxt::T("cat_system"), root, ShelfFilter::System });
        }

        if(this->IsInRoot()) {
            if((this->shelf_root_filter == ShelfFilter::Homebrew) && !this->shelf_root_has_hb) {
                this->shelf_root_filter = ShelfFilter::Games;
            }
            if((this->shelf_root_filter == ShelfFilter::Recent) && !this->shelf_root_has_recent) {
                this->shelf_root_filter = ShelfFilter::Games;
            }
            if((this->shelf_root_filter == ShelfFilter::System) && !this->shelf_root_has_special) {
                this->shelf_root_filter = ShelfFilter::Games;
            }
            if(this->shelf_root_filter == ShelfFilter::Folder) {
                this->shelf_root_filter = ShelfFilter::Games;
            }
        }

        // Which category are we in right now?
        this->shelf_cat = -1;
        for(u32 i = 0; i < this->shelf_cats.size(); i++) {
            const auto &cat = this->shelf_cats.at(i);
            if(this->IsInRoot() ? ((cat.filter != ShelfFilter::Folder) && (cat.filter == this->shelf_root_filter)) : ((cat.filter == ShelfFilter::Folder) && (cat.path == this->cur_path))) {
                this->shelf_cat = i;
                break;
            }
        }
        if(this->shelf_cat < 0) {
            // A nested folder (not a top-level category): show it as its own tab
            this->shelf_cats.push_back({ fs::GetFileName(this->cur_path), this->cur_path, ShelfFilter::Folder });
            this->shelf_cat = this->shelf_cats.size() - 1;
        }

        std::vector<std::string> names;
        for(const auto &cat : this->shelf_cats) {
            names.push_back(cat.name);
        }
        if(names != this->shelf_tab_names) {
            for(auto &tex : this->shelf_tab_texts) {
                pu::ui::render::DeleteTexture(tex);
            }
            this->shelf_tab_texts.assign(names.size(), nullptr);
            this->shelf_tab_names = names;
        }
    }

    void EntryMenu::ShelfGoToCategory(const s32 idx_raw) {
        const auto n = (s32)this->shelf_cats.size();
        if(n <= 1) {
            return;
        }
        const auto idx = ((idx_raw % n) + n) % n;
        if(idx == this->shelf_cat) {
            return;
        }

        // Remember where we were on this shelf
        const auto nv = (s32)this->shelf_valid.size();
        if(nv > 0) {
            this->shelf_cat_pos[this->ShelfCatKey()] = ((this->shelf_target % nv) + nv) % nv;
        }

        const auto cat = this->shelf_cats.at(idx);
        this->shelf_intro = 0.0;
        this->shelf_intro_dir = (idx_raw > this->shelf_cat) ? 1.0 : -1.0;
        this->Haptic(0.30f, 220.0f, 0.016);
        this->Haptic(0.30f, 220.0f, 0.016, 0.085);
        this->shelf_sheen_pending = true;
        this->cur_entry_change_started_cb();

        while(!this->entry_idx_stack.empty()) {
            this->entry_idx_stack.pop();
        }

        if(cat.filter != ShelfFilter::Folder) {
            this->shelf_root_filter = cat.filter;
            if(!this->IsInRoot()) {
                if(this->shelf_folder_cb) {
                    this->shelf_folder_cb(GetActiveMenuPath(), "");
                }
            }
        }
        else if(this->shelf_folder_cb) {
            this->shelf_folder_cb(cat.path, cat.name);
        }

        this->shelf_cats_sig.clear();
        this->shelf_synced = false;
        this->shelf_restore_pending = true;
        this->ShelfClearInfo();
        this->ShelfRebuildValid();
        this->NotifyFocusedEntryChanged(-1);
    }

    void EntryMenu::ShelfSwitchCategory(const s32 dir) {
        this->ShelfGoToCategory(this->shelf_cat + dir);
    }

    void EntryMenu::ShelfClearInfo() {
        pu::ui::render::DeleteTexture(this->shelf_info_title);
        pu::ui::render::DeleteTexture(this->shelf_info_meta);
        this->shelf_info_title = nullptr;
        this->shelf_info_meta = nullptr;
        this->shelf_info_idx = -1;
    }

    void EntryMenu::ShelfMove(const s32 dir) {
        const auto nv = (s32)this->shelf_valid.size();
        if(nv == 0) {
            return;
        }

        const auto prev_idx = (s32)this->cur_entry_idx;
        if(nv < ShelfLoopMin) {
            // Few games: a plain row with ends, no infinite loop
            const auto cur = ((this->shelf_target % nv) + nv) % nv;
            const auto next = std::clamp(cur + dir, 0, nv - 1);
            if(next == cur) {
                // Rubber band: nudge past the end, the easing pulls it back
                this->shelf_s += (dir > 0) ? 0.22 : -0.22;
                this->Haptic(0.75f, 110.0f, 0.06);
                this->NxSfx(1);
                return;
            }
            this->shelf_target = next;
            this->shelf_s = std::clamp(this->shelf_s, -0.5, (double)nv - 0.5);
        }
        else {
            this->shelf_target += dir;
        }
        this->shelf_sheen_pending = true;
        const auto pos = ((this->shelf_target % nv) + nv) % nv;
        this->Haptic(0.22f, 260.0f, 0.012);
        this->cur_entry_change_started_cb();
        this->SetFocusedEntryIndex(this->shelf_valid.at(pos));
        this->NotifyFocusedEntryChanged(prev_idx);
    }

    void EntryMenu::ShelfMoveToPos(const s32 pos) {
        const auto nv = (s32)this->shelf_valid.size();
        if(nv == 0) {
            return;
        }
        const auto cur = ((this->shelf_target % nv) + nv) % nv;
        auto delta = pos - cur;
        if(nv >= ShelfLoopMin) {
            delta = ((pos - cur) % nv + nv) % nv;
            if(delta > nv / 2) {
                delta -= nv;
            }
        }
        if(delta != 0) {
            this->ShelfMove(delta);
        }
    }

    void EntryMenu::ShelfInput(const u64 keys_down, const u64 keys_held, const pu::ui::TouchPoint touch_pos) {
        if((keys_down != 0) || (keys_held != 0) || !touch_pos.IsEmpty()) {
            this->idle_t = 0.0;
            if(this->saver_on) {
                // Any input ends the idle drift and is swallowed
                this->SaverStop();
                this->shelf_touching = false;
                return;
            }
        }
        if(this->launch_t >= 0.0) {
            // Launch transition running
            return;
        }
        if(this->lock_open) {
            this->LockInput(keys_down, touch_pos);
            return;
        }
        if(this->sheet_open) {
            this->SheetInput(keys_down, touch_pos);
            return;
        }
        if(this->flip_open) {
            this->FlipInput(keys_down, touch_pos);
            return;
        }
        if(this->br_open) {
            this->BracketInput(keys_down, touch_pos);
            return;
        }
        if(this->duel_open) {
            this->DuelInput(keys_down, touch_pos);
            return;
        }
        if(this->defer_pending) {
            // Nothing to navigate until the entries are in
            return;
        }

        if(nxdash::On("eggs")) {
            // Secret tip: hold L + R + ZL + ZR
            const u64 combo = HidNpadButton_L | HidNpadButton_R | HidNpadButton_ZL | HidNpadButton_ZR;
            if((keys_held & combo) == combo) {
                if(!this->tip_combo_down) {
                    this->tip_combo_down = true;
                    const auto count = 12;
                    this->shelf_press = 1.0;
                    this->Haptic(0.35f, 200.0f, 0.03);
                    g_MenuApplication->ShowNotification(nxt::T("tip_" + std::to_string(armGetSystemTick() % count)), 3200);
                }
                return;
            }
            this->tip_combo_down = false;

            // Right stick spun in circles: the cases go down like dominoes
            u32 rs_dir = 0;
            if(keys_held & HidNpadButton_StickRUp) { rs_dir = 1; }
            else if(keys_held & HidNpadButton_StickRRight) { rs_dir = 2; }
            else if(keys_held & HidNpadButton_StickRDown) { rs_dir = 3; }
            else if(keys_held & HidNpadButton_StickRLeft) { rs_dir = 4; }
            if((rs_dir != 0) && (rs_dir != this->rs_last_dir)) {
                if(this->rs_last_dir != 0) {
                    const auto step = ((s32)rs_dir - (s32)this->rs_last_dir + 4) % 4;
                    const auto sign = (step == 1) ? 1 : ((step == 3) ? -1 : 0);
                    if((sign != 0) && (sign == this->rs_sign) && ((this->fx_time - this->rs_last_t) < 0.5)) {
                        this->rs_steps++;
                    }
                    else {
                        this->rs_steps = (sign != 0) ? 1 : 0;
                    }
                    this->rs_sign = sign;
                }
                this->rs_last_dir = rs_dir;
                this->rs_last_t = this->fx_time;
                if((this->rs_steps >= 7) && (this->domino_t < 0.0)) {
                    this->rs_steps = 0;
                    this->domino_t = 0.0;
                    for(u32 i = 0; i < 10; i++) {
                        this->Haptic(0.30f + 0.04f * i, 140.0f, 0.02, 0.05 * i);
                    }
                    this->NxSfx(1);
                }
            }
        }

        // ---- Touch: drag to scroll, tap a case to focus it, tap the focused case to open it, tap a tab to switch shelf ----
        if(!touch_pos.IsEmpty()) {
            if(!this->shelf_touching) {
                this->shelf_touching = true;
                this->shelf_touch_moved = false;
                this->shelf_touch_start_x = touch_pos.x;
                this->shelf_touch_applied = 0;
            }
            this->shelf_touch_last_x = touch_pos.x;
            this->shelf_touch_last_y = touch_pos.y;

            const auto dx = touch_pos.x - this->shelf_touch_start_x;
            if(std::abs(dx) > 24) {
                this->shelf_touch_moved = true;
            }
            if(this->shelf_touch_moved && (this->shelf_step_px > 1.0)) {
                const auto want = (s32)std::lround(-(double)dx / this->shelf_step_px);
                if(want != this->shelf_touch_applied) {
                    this->ShelfMove(want - this->shelf_touch_applied);
                    this->shelf_touch_applied = want;
                }
            }
            return;
        }
        else if(this->shelf_touching) {
            this->shelf_touching = false;
            if(!this->shelf_touch_moved) {
                const auto tx = this->shelf_touch_last_x;
                const auto ty = this->shelf_touch_last_y;
                for(const auto &hit : this->shelf_tab_hits) {
                    if((tx >= hit.x) && (tx < hit.x + hit.w) && (ty >= hit.y) && (ty < hit.y + hit.h)) {
                        this->ShelfGoToCategory(hit.pos);
                        return;
                    }
                }
                // Tap: nearest case first (hits are stored far-to-near)
                const auto nv = (s32)this->shelf_valid.size();
                for(auto it = this->shelf_hits.rbegin(); it != this->shelf_hits.rend(); it++) {
                    if((tx >= it->x) && (tx < it->x + it->w) && (ty >= it->y) && (ty < it->y + it->h)) {
                        const auto cur = (nv > 0) ? (((this->shelf_target % nv) + nv) % nv) : -1;
                        if(it->pos == cur) {
                            this->shelf_press = 1.0;
                            this->cur_entry_input_cb(HidNpadButton_A);
                        }
                        else {
                            this->ShelfMoveToPos(it->pos);
                        }
                        break;
                    }
                }
            }
            return;
        }

        const auto nv = (s32)this->shelf_valid.size();
        const auto left_down = keys_down & (HidNpadButton_Left | HidNpadButton_StickLLeft);
        const auto right_down = keys_down & (HidNpadButton_Right | HidNpadButton_StickLRight);
        this->shelf_held = this->shelf_hold_frames > 27;
        const auto left_held = keys_held & (HidNpadButton_Left | HidNpadButton_StickLLeft);
        const auto right_held = keys_held & (HidNpadButton_Right | HidNpadButton_StickLRight);
        const auto up_down = keys_down & (HidNpadButton_Up | HidNpadButton_StickLUp);
        const auto down_down = keys_down & (HidNpadButton_Down | HidNpadButton_StickLDown);

        if(up_down || down_down) {
            this->ShelfSwitchCategory(up_down ? -1 : 1);
            this->shelf_hold_frames = 0;
        }
        else if(left_down || right_down) {
            // One press = one game
            this->ShelfMove(left_down ? -1 : 1);
            this->shelf_hold_frames = 1;
        }
        else if(left_held || right_held) {
            // Hold to scroll: wait ~0.45 s, then one game every ~0.13 s
            this->shelf_hold_frames++;
            if((this->shelf_hold_frames > 27) && ((this->shelf_hold_frames % 8) == 0)) {
                this->ShelfMove(left_held ? -1 : 1);
            }
        }
        else if(keys_down & HidNpadButton_L) {
            this->ShelfMove(-5);
        }
        else if(keys_down & HidNpadButton_R) {
            this->ShelfMove(5);
        }
        else if((keys_down & HidNpadButton_Y) && nxdash::On("roulette")) {
            this->shelf_hold_frames = 0;
            this->DuelOpen();
        }
        else {
            this->shelf_hold_frames = 0;
            auto fwd = keys_down & ~(u64)(HidNpadButton_Up | HidNpadButton_Down | HidNpadButton_AnyUp | HidNpadButton_AnyDown | HidNpadButton_AnyLeft | HidNpadButton_AnyRight | HidNpadButton_Minus | HidNpadButton_Plus);
            if(nxdash::On("roulette")) {
                fwd &= ~(u64)HidNpadButton_Y;
            }
            if(nv == 0) {
                // Empty shelf: the focused entry is not visible, so don't act on it
                fwd &= ~(u64)(HidNpadButton_A | HidNpadButton_X | HidNpadButton_Y);
            }
            if(fwd) {
                if((fwd & HidNpadButton_A) && (nv > 0)) {
                    this->shelf_press = 1.0;
                }
                this->cur_entry_input_cb(fwd);
            }
        }
    }

    void EntryMenu::ShelfRender(pu::ui::render::Renderer::Ref &drawer, const s32 x, const s32 y) {
        if(this->defer_pending) {
            // Entries not read yet: placeholder cases keep the shelf from looking empty
            this->RenderSkeleton(drawer, x, y);
            this->shelf_hits.clear();
            return;
        }
        if((this->lock_open && (this->lock_anim >= 1.0) && (this->lock_out < 0.0)) || (this->launch_t > 0.45)) {
            // Fully covered by the lock screen or the launch transition: nothing here would be seen
            return;
        }

        this->ShelfRebuildValid();
        const auto nv = (s32)this->shelf_valid.size();

        // Category change slide
        this->shelf_intro += this->fx_dt;

        // Idle: after a while the shelf starts drifting on its own
        if(!this->IsModalOpen() && nxdash::On("screensaver")) {
            this->idle_t += this->fx_dt;
            const auto wait = (double)std::max(10, nxdash::Value("saver_seconds", 60));
            if(!this->saver_on && (this->idle_t > wait) && (nv > 1)) {
                this->saver_on = true;
                this->saver_step_t = 0.0;
                this->saver_dir = 1;
                this->saver_saved_pos = ((this->shelf_target % nv) + nv) % nv;
            }
        }
        else {
            this->idle_t = 0.0;
        }
        if(this->saver_on) {
            this->saver_step_t += this->fx_dt;
            if(this->saver_step_t >= 2.6) {
                this->saver_step_t = 0.0;
                this->ShelfStepSilent(this->saver_dir);
            }
        }
        this->saver_fade = this->FxApproach(this->saver_fade, this->saver_on ? 1.0 : 0.0, 0.05);
        if(this->domino_t >= 0.0) {
            this->domino_t += this->fx_dt;
            if(this->domino_t > 3.2) {
                this->domino_t = -1.0;
            }
        }

        // Which games have earned the collector's rim: one statistics query per frame, never a burst
        if(nxdash::On("eggs") && !this->cur_entries.empty()) {
            if(this->collector.size() != this->cur_entries.size()) {
                this->collector.assign(this->cur_entries.size(), -1);
                this->collector_next = 0;
            }
            if(this->collector_next < this->cur_entries.size()) {
                const auto &entry = this->cur_entries.at(this->collector_next);
                s8 earned = 0;
                if(entry.Is<EntryType::Application>()) {
                    earned = (this->QueryLaunches(entry) >= (u32)std::max(1, nxdash::Value("collector_launches", 100))) ? 1 : 0;
                }
                this->collector.at(this->collector_next) = earned;
                this->collector_next++;
            }
        }

        this->ShelfRenderInfo(drawer, x, y);
        if(nv == 0) {
            if(this->shelf_empty_text == nullptr) {
                this->shelf_empty_text = pu::ui::render::RenderText(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::MediumLarge), nxt::T("empty_shelf"), { 0xFF, 0xFF, 0xFF, 0xB0 });
            }
            const auto tw = pu::ui::render::GetTextureWidth(this->shelf_empty_text);
            const auto th = pu::ui::render::GetTextureHeight(this->shelf_empty_text);
            drawer->RenderTexture(this->shelf_empty_text, x + (this->GetWidth() - tw) / 2, y + (s32)(this->GetHeight() * 0.355) - th / 2);
            this->shelf_hits.clear();
            return;
        }

        // Ease towards the target (continuous position, so wrapping around stays smooth)
        this->shelf_s = this->FxApproach(this->shelf_s, (double)this->shelf_target, this->saver_on ? 0.022 : (this->shelf_held ? 0.42 : 0.20));
        this->shelf_press *= std::pow(0.80, this->fx_k);
        if(this->shelf_press < 0.01) {
            this->shelf_press = 0.0;
        }
        if(std::abs((double)this->shelf_target - this->shelf_s) < 0.001) {
            this->shelf_s = (double)this->shelf_target;
        }

        const double area_w = (double)this->GetWidth();
        const double area_h = (double)this->GetHeight();
        const double H = area_h * 0.62;
        const double d = H * (474.0 / 760.0);
        const double ws = H * 0.085;
        const double gap = H * 0.014;
        const double U = H * 1.3;
        const double cx = (double)x + area_w / 2.0;
        const double cy = (double)y + area_h * 0.355;

        // Pre-computed integral of the scale curve: far cases pack tighter on screen
        if(this->shelf_xs.empty() || (this->shelf_xs_u != U)) {
            this->shelf_xs.clear();
            this->shelf_xs.push_back(0.0);
            for(double u = 0.0; u < 3.0 * U; u += 2.0) {
                this->shelf_xs.push_back(this->shelf_xs.back() + 2.0 / (1.0 + std::pow(u / U, 1.6)));
            }
            this->shelf_xs_u = U;
        }
        const auto xsc = [&](const double u) {
            const auto i = std::min<size_t>(this->shelf_xs.size() - 1, (size_t)(std::abs(u) / 2.0));
            return (u < 0 ? -1.0 : 1.0) * this->shelf_xs.at(i);
        };
        const auto scl = [&](const double u) {
            return 1.0 / (1.0 + std::pow(std::abs(u) / U, 1.6));
        };
        const auto bump = [](const double r) {
            return (std::abs(r) < 1.0) ? ((r + std::sin(M_PI * r) / M_PI) / 2.0) : ((r < 0) ? -0.5 : 0.5);
        };

        struct Item { s32 pos; double r; };
        std::vector<Item> items;
        items.reserve(nv);
        for(s32 p = 0; p < nv; p++) {
            double r = (double)p - this->shelf_s;
            if(nv >= ShelfLoopMin) {
                r = std::fmod(r, (double)nv);
                if(r < 0) {
                    r += nv;
                }
                if(r >= nv / 2.0) {
                    r -= nv;
                }
            }
            items.push_back({ p, r });
        }
        // Draw far ones first so near ones overlap them
        std::sort(items.begin(), items.end(), [](const Item &a, const Item &b) {
            return std::abs(a.r) > std::abs(b.r);
        });

        this->shelf_hits.clear();
        this->shelf_step_px = ws + gap;

        EnsureShapes();
        const auto target_pos = ((this->shelf_target % nv) + nv) % nv;
        const auto settled = std::clamp(1.0 - std::abs((double)this->shelf_target - this->shelf_s) * 4.0, 0.0, 1.0);
        const auto intro_of = [&](const double r) {
            const auto p = std::clamp((this->shelf_intro - std::min(std::abs(r), 8.0) * 0.03) / 0.30, 0.0, 1.0);
            return 1.0 - (1.0 - p) * (1.0 - p) * (1.0 - p);
        };

        // Sheen: one pass of light over the cover once it has settled
        if(this->shelf_sheen_pending && (settled > 0.9) && (this->shelf_intro > 0.25)) {
            this->shelf_sheen_pending = false;
            this->shelf_sheen = 0.0;
        }
        if(this->shelf_sheen < 1.0) {
            this->shelf_sheen = std::min(1.0, this->shelf_sheen + this->fx_dt / 0.75);
        }

        // A pool of light on the floor the cases stand on
        if(g_Glow != nullptr) {
            drawer->RenderTexture(g_Glow, (s32)(cx - area_w * 0.46), (s32)(cy + H / 2.0 - H * 0.10), pu::ui::render::TextureRenderOptions((u8)(26.0 * intro_of(0.0)), (s32)(area_w * 0.92), (s32)(H * 0.24), {}, {}, {}));
        }

        // Soft floor shadow under the focused case: grows as the case lifts
        if(g_SoftShadow != nullptr) {
            const auto sh_w = d * (1.55 + 0.25 * settled);
            const auto sh_h = H * 0.085;
            drawer->RenderTexture(g_SoftShadow, (s32)(cx - sh_w / 2.0), (s32)(cy + H / 2.0 - sh_h * 0.42), pu::ui::render::TextureRenderOptions((u8)(150.0 * intro_of(0.0)), (s32)sh_w, (s32)sh_h, {}, {}, {}));
        }

        u32 loads_left = 2;
        const auto any_suspended = g_GlobalSettings.IsSuspended();

        for(const auto &it : items) {
            const auto r = it.r;
            const auto o = SmoothStep(1.0 - std::abs(r));
            const auto u = r * (ws + gap) + (d - ws + 1.4 * gap) * bump(r);
            if(std::abs(u) > U * 1.95) {
                continue;
            }

            const auto sc = scl(u);
            // Pressed: the focused case dips to 0.97
            // Focused: lifts 4.5% and floats a little; pressed: dips to 0.97
            const auto press_k = (1.0 - 0.03 * this->shelf_press * o) * (1.0 + 0.045 * o);
            const auto case_w = (ws + (d - ws) * o) * sc * press_k;
            // Domino easter egg: a wave of cases dropping left to right, then standing back up
            double dom = 0.0;
            if(this->domino_t >= 0.0) {
                const auto lt = this->domino_t - (r + 10.0) * 0.05;
                dom = SmoothStep(lt / 0.20) * (1.0 - SmoothStep((lt - 1.25) / 0.40));
            }
            const auto case_h = H * sc * press_k * (1.0 - 0.24 * dom);
            const auto sink = H * 0.5 * (1.0 - sc) * 0.42;
            const auto fade = intro_of(r);
            const auto bob = std::sin(this->fx_time * (2.0 * M_PI / 4.5)) * 3.0 * o * settled;
            const auto ex = cx + xsc(u) - case_w / 2.0;
            const auto ey = cy - case_h / 2.0 + sink - o * H * 0.02 + bob + (1.0 - fade) * 46.0 * this->shelf_intro_dir + dom * H * 0.30 * sc;
            if((fade <= 0.004) || (ex + case_w < (double)x - 24.0) || (ex > (double)x + area_w + 24.0)) {
                continue;
            }

            const auto entry_idx = this->shelf_valid.at(it.pos);
            auto &entry = this->cur_entries.at(entry_idx);
            pu::sdl2::TextureHandle::Ref icon = (entry_idx < this->entry_icons.size()) ? this->entry_icons.at(entry_idx) : nullptr;
            if((icon == nullptr) && this->pending_load_img_done && entry.Is<EntryType::Application>() && (loads_left > 0)) {
                // At most two loads per frame, so a fresh shelf never stalls a frame
                loads_left--;
                icon = LoadApplicationIconTexture(entry.app_info.app_id);
                if(icon != nullptr) {
                    entry.TryLoadNacp();
                    this->entry_icons.at(entry_idx) = icon;
                }
            }

            const auto radius = (s32)std::max(2.0, 8.0 * sc);
            this->shelf_hits.push_back({ it.pos, (s32)ex, (s32)ey, (s32)case_w, (s32)case_h });

            // Focus ring
            if(o > 0.6) {
                const auto ring = (u8)(255.0 * fade * (o - 0.6) / 0.4);
                FillRR(drawer, { 0xFF, 0xFF, 0xFF, (u8)(ring * 0.10) }, (s32)ex - 14, (s32)ey - 14, (s32)case_w + 28, (s32)case_h + 28, radius + 13);
                FillRR(drawer, { 0xFF, 0xFF, 0xFF, (u8)(ring * 0.16) }, (s32)ex - 8, (s32)ey - 8, (s32)case_w + 16, (s32)case_h + 16, radius + 7);
                FillRR(drawer, { 0xFF, 0xFF, 0xFF, (u8)(ring * 0.92) }, (s32)ex - 2, (s32)ey - 2, (s32)case_w + 4, (s32)case_h + 4, radius + 2);
            }

            // Collector's rim: a slow rainbow around games opened a hundred times
            if((entry_idx < this->collector.size()) && (this->collector.at(entry_idx) > 0)) {
                const auto hue_at = [&](const double shift) {
                    const auto h6 = std::fmod(this->fx_time * 0.22 + (double)it.pos * 0.09 + shift, 1.0) * 6.0;
                    const auto f = h6 - std::floor(h6);
                    const u8 up = (u8)(255.0 * f);
                    const u8 dn = (u8)(255.0 * (1.0 - f));
                    switch((s32)h6) {
                        case 0: return pu::ui::Color{ 0xFF, up, 0x60, 0xFF };
                        case 1: return pu::ui::Color{ dn, 0xFF, 0x60, 0xFF };
                        case 2: return pu::ui::Color{ 0x60, 0xFF, up, 0xFF };
                        case 3: return pu::ui::Color{ 0x60, dn, 0xFF, 0xFF };
                        case 4: return pu::ui::Color{ up, 0x60, 0xFF, 0xFF };
                        default: return pu::ui::Color{ 0xFF, 0x60, dn, 0xFF };
                    }
                };
                const auto outer = hue_at(0.0);
                const auto inner = hue_at(0.33);
                const auto rim = std::max(2.0, 5.0 * sc);
                FillRR(drawer, { outer.r, outer.g, outer.b, (u8)(235.0 * fade) }, (s32)(ex - rim), (s32)(ey - rim), (s32)(case_w + 2.0 * rim), (s32)(case_h + 2.0 * rim), radius + (s32)rim);
                FillRR(drawer, { inner.r, inner.g, inner.b, (u8)(235.0 * fade) }, (s32)(ex - rim / 2.0), (s32)(ey - rim / 2.0), (s32)(case_w + rim), (s32)(case_h + rim), radius + (s32)(rim / 2.0));
            }

            // Case body: red by default, violet for the last games played, shiny gold for the one running now
            const auto is_active = any_suspended && g_GlobalSettings.IsEntrySuspended(entry);
            // (The violet "recent" case was dropped once the Recientes shelf existed)
            const auto is_recent = false;
            const auto case_clr = is_active ? ShelfActiveColor : (is_recent ? ShelfRecentColor : ShelfCaseColor);
            FillRR(drawer, { case_clr.r, case_clr.g, case_clr.b, (u8)(255.0 * fade) }, (s32)ex, (s32)ey, (s32)case_w, (s32)case_h, radius);
            {
                // Moulded plastic: light from above, darker towards the floor, a bevel on top and a lip at the bottom
                const auto inset = (s32)(radius * 0.4);
                if(g_GlossDown != nullptr) {
                    drawer->RenderTexture(g_GlossDown, (s32)ex + inset, (s32)ey, pu::ui::render::TextureRenderOptions((u8)((is_active ? 96.0 : 46.0) * fade), (s32)case_w - 2 * inset, (s32)(case_h * 0.45), {}, {}, {}));
                }
                if(g_FadeUp != nullptr) {
                    drawer->RenderTexture(g_FadeUp, (s32)ex + inset, (s32)(ey + case_h * 0.5), pu::ui::render::TextureRenderOptions((u8)(84.0 * fade), (s32)case_w - 2 * inset, (s32)(case_h * 0.5), {}, {}, {}));
                }
                if(case_w > 2.0 * radius + 4.0) {
                    drawer->RenderRectangleFill({ 0xFF, 0xFF, 0xFF, (u8)(96.0 * fade) }, (s32)ex + radius, (s32)ey + 1, (s32)case_w - 2 * radius, 1);
                    drawer->RenderRectangleFill({ 0x00, 0x00, 0x00, (u8)(70.0 * fade) }, (s32)ex + radius, (s32)(ey + case_h) - 2, (s32)case_w - 2 * radius, 2);
                }
            }

            if(o > 0.35) {
                // Cover: the icon as key art inside the case frame
                const auto a = (u8)(255.0 * fade * std::min(1.0, (o - 0.35) / 0.4));
                const auto pad = case_w * 0.06;
                const auto band = case_h * 0.075;
                const auto in_x = ex + pad;
                const auto in_y = ey + band;
                const auto in_w = case_w - 2.0 * pad;
                const auto in_h = case_h - band - pad;
                {
                    // Front of the case: hinge groove down the left edge, a small brand mark on the top band
                    const auto hinge_x = ex + case_w * 0.032;
                    const auto hinge_w = std::max(2.0, case_w * 0.012);
                    drawer->RenderRectangleFill({ 0x00, 0x00, 0x00, (u8)(a * 0.30) }, (s32)hinge_x, (s32)ey + radius, (s32)hinge_w, (s32)case_h - 2 * radius);
                    drawer->RenderRectangleFill({ 0xFF, 0xFF, 0xFF, (u8)(a * 0.22) }, (s32)(hinge_x + hinge_w), (s32)ey + radius, 1, (s32)case_h - 2 * radius);
                    const auto mark = band * 0.42;
                    const auto mark_y = ey + (band - mark) / 2.0 + 1.0;
                    FillRR(drawer, { 0xFF, 0xFF, 0xFF, (u8)(a * 0.92) }, (s32)in_x, (s32)mark_y, (s32)mark, (s32)mark, (s32)(mark * 0.28));
                    drawer->RenderRectangleFill({ 0xFF, 0xFF, 0xFF, (u8)(a * 0.70) }, (s32)(in_x + mark * 1.35), (s32)(mark_y + mark * 0.36), (s32)(mark * 2.4), (s32)std::max(2.0, mark * 0.28));
                }
                if((icon != nullptr) && (in_w > 4.0) && (in_h > 4.0)) {
                    // Full-bleed sleeve: the icon blown up behind (cropped to the sleeve, dimmed), the sharp icon on top
                    const SDL_Rect clip = { (s32)in_x, (s32)in_y, (s32)in_w, (s32)in_h };
                    SDL_RenderSetClipRect(pu::ui::render::GetMainRenderer(), &clip);
                    drawer->RenderTexture(icon->Get(), (s32)(in_x + (in_w - in_h) / 2.0), (s32)in_y, pu::ui::render::TextureRenderOptions(a, (s32)in_h, (s32)in_h, {}, {}, {}));
                    SDL_RenderSetClipRect(pu::ui::render::GetMainRenderer(), nullptr);
                    drawer->RenderRectangleFill({ 0x08, 0x09, 0x10, (u8)(a * 0.50) }, (s32)in_x, (s32)in_y, (s32)in_w, (s32)in_h);
                    const auto art = in_w * 0.86;
                    drawer->RenderTexture(icon->Get(), (s32)(in_x + (in_w - art) / 2.0), (s32)(in_y + (in_h - art) / 2.0), pu::ui::render::TextureRenderOptions(a, (s32)art, (s32)art, {}, {}, {}));
                    if((it.pos == target_pos) && (this->launch_t < 0.0)) {
                        this->launch_cx = in_x + in_w / 2.0;
                        this->launch_cy = in_y + in_h / 2.0;
                        this->launch_s = art;
                        this->launch_icon = icon;
                    }
                    // The printed sleeve sits behind clear plastic: inner shadow on two sides and a fixed band of gloss
                    drawer->RenderRectangleFill({ 0x00, 0x00, 0x00, (u8)(a * 0.40) }, (s32)in_x, (s32)in_y, (s32)in_w, 2);
                    drawer->RenderRectangleFill({ 0x00, 0x00, 0x00, (u8)(a * 0.30) }, (s32)in_x, (s32)in_y, 2, (s32)in_h);
                    if(g_Sheen != nullptr) {
                        SDL_RenderSetClipRect(pu::ui::render::GetMainRenderer(), &clip);
                        drawer->RenderTexture(g_Sheen, (s32)(in_x - in_w * 0.10), (s32)(in_y - in_h * 0.25), pu::ui::render::TextureRenderOptions((u8)(a * 0.11), (s32)(in_w * 0.50), (s32)(in_h * 1.5), 18.0f, {}, {}));
                        SDL_RenderSetClipRect(pu::ui::render::GetMainRenderer(), nullptr);
                    }
                }
            }
            else {
                // Spine: white label, small icon on top, vertical title
                const auto a = (u8)(255.0 * fade * std::min(1.0, (0.35 - o) / 0.35 + 0.0001));
                const auto lw = case_w * 0.74;
                const auto lx = ex + (case_w - lw) / 2.0;
                const auto icon_s = lw;
                const auto top = ey + case_h * 0.04;
                if(icon != nullptr) {
                    drawer->RenderTexture(icon->Get(), (s32)lx, (s32)top, pu::ui::render::TextureRenderOptions(a, (s32)icon_s, (s32)icon_s, {}, {}, {}));
                }
                const auto label_y = top + icon_s + case_h * 0.02;
                const auto label_h = ey + case_h * 0.94 - label_y;
                if(label_h > 4) {
                    drawer->RenderRectangleFill({ ShelfLabelColor.r, ShelfLabelColor.g, ShelfLabelColor.b, a }, (s32)lx, (s32)label_y, (s32)lw, (s32)label_h);

                    // Title texture, created once per entry at full size and scaled down
                    auto &title_tex = this->shelf_title_texts.at(entry_idx);
                    if((title_tex == nullptr) && (loads_left > 0)) {
                        loads_left--;
                        std::string name;
                        if(entry.Is<EntryType::Folder>()) {
                            name = entry.folder_info.name;
                        }
                        else {
                            entry.TryLoadNacp();
                            name = entry.control.name;
                        }
                        if(!name.empty()) {
                            title_tex = pu::ui::render::RenderText(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Medium), name, ShelfTitleColor);
                        }
                    }
                    if(title_tex != nullptr) {
                        const double tw = (double)pu::ui::render::GetTextureWidth(title_tex);
                        const double th = (double)pu::ui::render::GetTextureHeight(title_tex);
                        // Rotated 90 degrees: the text's width runs along the label height
                        const auto k = std::min((label_h * 0.92) / tw, (lw * 0.8) / th);
                        const auto dw = tw * k;
                        const auto dh = th * k;
                        const auto tcx = lx + lw / 2.0;
                        const auto tcy = label_y + label_h / 2.0;
                        drawer->RenderTexture(title_tex, (s32)(tcx - dw / 2.0), (s32)(tcy - dh / 2.0), pu::ui::render::TextureRenderOptions(a, (s32)dw, (s32)dh, 90.0f, {}, {}));
                    }
                }
            }

            if(o <= 0.35) {
                // Spine: rounded edges read darker than the middle
                const auto edge_w = std::max(1.0, case_w * 0.10);
                const auto edge_a = (u8)(58.0 * fade);
                drawer->RenderRectangleFill({ 0x00, 0x00, 0x00, edge_a }, (s32)ex, (s32)ey + radius, (s32)edge_w, (s32)case_h - 2 * radius);
                drawer->RenderRectangleFill({ 0x00, 0x00, 0x00, edge_a }, (s32)(ex + case_w - edge_w), (s32)ey + radius, (s32)edge_w, (s32)case_h - 2 * radius);
            }

            // The running game keeps catching the light
            if(is_active && (g_Sheen != nullptr)) {
                const auto t = std::fmod(this->fx_time, 2.6);
                if(t < 1.0) {
                    const auto q = SmoothStep(t);
                    const auto band_w = std::max(case_w * 0.7, 26.0);
                    const auto band_h = case_h * 1.5;
                    const auto bx = ex - band_w + (case_w + band_w) * q;
                    const SDL_Rect clip = { (s32)ex + 1, (s32)ey + 1, (s32)case_w - 2, (s32)case_h - 2 };
                    SDL_RenderSetClipRect(pu::ui::render::GetMainRenderer(), &clip);
                    drawer->RenderTexture(g_Sheen, (s32)bx, (s32)(ey + (case_h - band_h) / 2.0), pu::ui::render::TextureRenderOptions((u8)(120.0 * fade * std::sin(M_PI * t)), (s32)band_w, (s32)band_h, 18.0f, {}, {}));
                    SDL_RenderSetClipRect(pu::ui::render::GetMainRenderer(), nullptr);
                }
            }

            // Depth shading: further cases get darker
            // Sheen over the focused cover
            if((it.pos == target_pos) && (this->shelf_sheen < 1.0) && (g_Sheen != nullptr) && (o > 0.9)) {
                const auto q = SmoothStep(this->shelf_sheen);
                const auto band_w = case_w * 0.55;
                const auto band_h = case_h * 1.5;
                const auto bx = ex - band_w + (case_w + band_w) * q;
                const SDL_Rect clip = { (s32)ex + 2, (s32)ey + 2, (s32)case_w - 4, (s32)case_h - 4 };
                SDL_RenderSetClipRect(pu::ui::render::GetMainRenderer(), &clip);
                drawer->RenderTexture(g_Sheen, (s32)bx, (s32)(ey + (case_h - band_h) / 2.0), pu::ui::render::TextureRenderOptions((u8)(70.0 * std::sin(M_PI * this->shelf_sheen)), (s32)band_w, (s32)band_h, 18.0f, {}, {}));
                SDL_RenderSetClipRect(pu::ui::render::GetMainRenderer(), nullptr);
            }

            const auto shade = fade * std::min(0.72, (1.0 - sc) * 0.62) * (1.0 - o);
            if(shade > 0.01) {
                FillRR(drawer, { 0x05, 0x06, 0x0E, (u8)(255.0 * shade) }, (s32)ex, (s32)ey, (s32)case_w, (s32)case_h, radius);
            }
        }
    }

    // ---- Focused game info + category tabs ----

    namespace {

        std::string ShelfSpecialName(const Entry &entry) {
            switch(entry.type) {
                case EntryType::SpecialEntryMiiEdit: return nxt::T("sp_mii");
                case EntryType::SpecialEntryWebBrowser: return nxt::T("sp_web");
                case EntryType::SpecialEntryUserPage: return nxt::T("sp_user");
                case EntryType::SpecialEntrySettings: return nxt::T("sp_settings");
                case EntryType::SpecialEntryThemes: return nxt::T("sp_themes");
                case EntryType::SpecialEntryControllers: return nxt::T("sp_controllers");
                case EntryType::SpecialEntryAlbum: return nxt::T("sp_album");
                case EntryType::SpecialEntryAmiibo: return "amiibo";
                default: return "";
            }
        }

        inline pu::sdl2::Texture Text(const pu::ui::DefaultFontSize size, const std::string &text) {
            return pu::ui::render::RenderText(pu::ui::GetDefaultFont(size), text, { 0xFF, 0xFF, 0xFF, 0xFF });
        }

        inline void Free(pu::sdl2::Texture &tex) {
            pu::ui::render::DeleteTexture(tex);
            tex = nullptr;
        }

        inline void Draw(pu::ui::render::Renderer::Ref &drawer, pu::sdl2::Texture tex, const s32 x, const s32 y, const double alpha) {
            if(tex != nullptr) {
                drawer->RenderTexture(tex, x, y, pu::ui::render::TextureRenderOptions((u8)std::clamp(alpha, 0.0, 255.0), {}, {}, {}, {}, {}));
            }
        }

        inline s32 TexW(pu::sdl2::Texture tex) {
            return (tex != nullptr) ? pu::ui::render::GetTextureWidth(tex) : 0;
        }

        inline s32 TexH(pu::sdl2::Texture tex) {
            return (tex != nullptr) ? pu::ui::render::GetTextureHeight(tex) : 0;
        }

        void ShelfDescribe(Entry &entry, std::string &out_title, std::string &out_meta) {
            if(entry.IsSpecial()) {
                out_title = ShelfSpecialName(entry);
                out_meta = nxt::T("meta_system");
                return;
            }
            if(entry.Is<EntryType::Folder>()) {
                out_title = entry.folder_info.name;
                out_meta = nxt::T("meta_category");
                return;
            }
            entry.TryLoadNacp();
            out_title = entry.control.name.empty() ? nxt::T("untitled") : entry.control.name;
            std::string meta;
            if(entry.Is<EntryType::Homebrew>()) {
                meta = "Homebrew";
            }
            if(!entry.control.author.empty()) {
                meta += (meta.empty() ? "" : "   ·   ") + entry.control.author;
            }
            if(!entry.control.version.empty()) {
                meta += (meta.empty() ? "" : "   ·   ") + nxt::Sub(nxt::T("version"), "v", entry.control.version);
            }
            if(entry.Is<EntryType::Application>() && entry.app_info.IsGameCardInserted()) {
                meta += "   ·   " + nxt::T("cartridge");
            }
            if(g_GlobalSettings.IsSuspended() && g_GlobalSettings.IsEntrySuspended(entry)) {
                meta += "   ·   " + nxt::T("paused");
            }
            out_meta = meta;
        }

        inline void RenderTextFit(pu::ui::render::Renderer::Ref &drawer, pu::sdl2::Texture tex, const double cx_or_x, const s32 y, const double max_w, const u8 alpha, const bool centered) {
            if(tex == nullptr) {
                return;
            }
            const double tw = (double)pu::ui::render::GetTextureWidth(tex);
            const double th = (double)pu::ui::render::GetTextureHeight(tex);
            const auto k = std::min(1.0, max_w / tw);
            const auto dw = tw * k;
            const auto dh = th * k;
            const auto dx = centered ? (cx_or_x - dw / 2.0) : cx_or_x;
            drawer->RenderTexture(tex, (s32)dx, y, pu::ui::render::TextureRenderOptions(alpha, (s32)dw, (s32)dh, {}, {}, {}));
        }

    }

    void EntryMenu::ShelfRenderInfo(pu::ui::render::Renderer::Ref &drawer, const s32 x, const s32 y) {
        const double area_w = (double)this->GetWidth();
        const double area_h = (double)this->GetHeight();

        // Tabs: one per shelf; Up/Down (or a tap) switches
        this->shelf_tab_hits.clear();
        if(this->shelf_cats.size() > 1) {
            const auto font = pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Medium);
            std::vector<s32> widths;
            s32 total = 0;
            s32 cur_left = 0;
            s32 cur_right = 0;
            for(u32 i = 0; i < this->shelf_cats.size(); i++) {
                auto &tex = this->shelf_tab_texts.at(i);
                if(tex == nullptr) {
                    tex = pu::ui::render::RenderText(font, this->shelf_cats.at(i).name, { 0xFF, 0xFF, 0xFF, 0xFF });
                }
                const auto w = pu::ui::render::GetTextureWidth(tex) + 44;
                if((s32)i == this->shelf_cat) {
                    cur_left = total;
                    cur_right = total + w;
                }
                widths.push_back(w);
                total += w + 14;
            }
            // Bottom row, no capsule: the current shelf stays centred and the row slides under it
            const auto mid = (double)x + area_w / 2.0;
            const auto want = mid - (double)(cur_left + (cur_right - cur_left) / 2);
            if(!this->tab_off_init) {
                this->tab_off_init = true;
                this->tab_off = want;
            }
            this->tab_off = this->FxApproach(this->tab_off, want, 0.24);
            s32 tx = (s32)this->tab_off;
            const auto th = 50;
            const auto ty = y + (s32)area_h - th - 24;
            {
                // A thin underline that slides between shelves
                const auto hl_x = (double)(tx + cur_left);
                const auto hl_w = (double)(cur_right - cur_left);
                if(!this->tab_hl_init) {
                    this->tab_hl_init = true;
                    this->tab_hl_x = hl_x;
                    this->tab_hl_w = hl_w;
                }
                this->tab_hl_x = this->FxApproach(this->tab_hl_x, hl_x, 0.26);
                this->tab_hl_w = this->FxApproach(this->tab_hl_w, hl_w, 0.26);
                FillRR(drawer, { 0xFF, 0xFF, 0xFF, 0xE6 }, (s32)this->tab_hl_x + 22, ty + th - 4, (s32)this->tab_hl_w - 44, 3, 1);
            }
            for(u32 i = 0; i < this->shelf_cats.size(); i++) {
                const auto w = widths.at(i);
                const auto is_cur = (s32)i == this->shelf_cat;
                if((tx + w > x) && (tx < x + (s32)area_w)) {
                    auto tex = this->shelf_tab_texts.at(i);
                    const auto tth = pu::ui::render::GetTextureHeight(tex);
                    const auto dist = std::abs((double)(tx + w / 2) - mid);
                    const auto edge = std::clamp(1.0 - (dist - 500.0) / 360.0, 0.0, 1.0);
                    drawer->RenderTexture(tex, tx + 22, ty + (th - tth) / 2, pu::ui::render::TextureRenderOptions((u8)((is_cur ? 255.0 : 120.0) * edge), {}, {}, {}, {}, {}));
                    this->shelf_tab_hits.push_back({ (s32)i, tx, ty, w, th });
                }
                tx += w + 14;
            }
        }

        // Title + details of the focused case
        const auto nv = (s32)this->shelf_valid.size();
        if(nv == 0) {
            this->ShelfClearInfo();
            return;
        }
        if((this->shelf_info_idx != (s32)this->cur_entry_idx) || (this->shelf_info_path != this->cur_path)) {
            this->ShelfClearInfo();
            this->shelf_info_idx = this->cur_entry_idx;
            this->shelf_info_path = this->cur_path;
            if(this->cur_entry_idx < this->cur_entries.size()) {
                std::string title;
                std::string meta;
                ShelfDescribe(this->cur_entries.at(this->cur_entry_idx), title, meta);
                if(!title.empty()) {
                    this->shelf_info_title = pu::ui::render::RenderText(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Large), title, { 0xFF, 0xFF, 0xFF, 0xFF });
                }
                if(!meta.empty()) {
                    this->shelf_info_meta = pu::ui::render::RenderText(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Medium), meta, { 0xFF, 0xFF, 0xFF, 0xFF });
                }
            }
        }

        // Fade the text in while the shelf settles
        const auto settle = std::clamp(1.0 - std::abs((double)this->shelf_target - this->shelf_s) * 2.5, 0.0, 1.0);
        const auto cx = (double)x + area_w / 2.0;
        const auto ty = y + (s32)(area_h * 0.75);
        const auto travel = std::clamp((double)this->shelf_target - this->shelf_s, -1.0, 1.0) * 34.0;
        const auto intro = std::clamp((this->shelf_intro - 0.08) / 0.30, 0.0, 1.0);
        {
            // "12 / 68" at the right end of the tab row
            const auto pos_now = ((this->shelf_target % nv) + nv) % nv;
            const auto key = pos_now * 10000 + nv;
            if(key != this->shelf_count_key) {
                this->shelf_count_key = key;
                pu::ui::render::DeleteTexture(this->shelf_count_tex);
                this->shelf_count_tex = pu::ui::render::RenderText(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Small), std::to_string(pos_now + 1) + " / " + std::to_string(nv), { 0xFF, 0xFF, 0xFF, 0xFF });
            }
            if(this->shelf_count_tex != nullptr) {
                const auto cw = pu::ui::render::GetTextureWidth(this->shelf_count_tex);
                drawer->RenderTexture(this->shelf_count_tex, x + (s32)area_w - 118 - cw, y + 18 + 12, pu::ui::render::TextureRenderOptions((u8)(140.0 * intro), {}, {}, {}, {}, {}));
            }
        }
        RenderTextFit(drawer, this->shelf_info_title, cx + travel, ty, area_w - 260.0, (u8)(255.0 * settle * intro), true);
        const auto settle_late = std::clamp(1.0 - std::abs((double)this->shelf_target - this->shelf_s) * 6.0, 0.0, 1.0);
        RenderTextFit(drawer, this->shelf_info_meta, cx, ty + 58 + (s32)((1.0 - settle_late) * 8.0), area_w - 260.0, (u8)(150.0 * settle_late * intro), true);
    }

    // ---- HOME game sheet ----

    void EntryMenu::SheetFree() {
        for(auto &tex : this->sheet_opt_texts) {
            pu::ui::render::DeleteTexture(tex);
        }
        this->sheet_opt_texts.clear();
        pu::ui::render::DeleteTexture(this->sheet_title);
        pu::ui::render::DeleteTexture(this->sheet_meta);
        pu::ui::render::DeleteTexture(this->sheet_badge);
        pu::ui::render::DeleteTexture(this->sheet_hint);
        this->sheet_title = nullptr;
        this->sheet_meta = nullptr;
        this->sheet_badge = nullptr;
        pu::ui::render::DeleteTexture(this->sheet_time);
        this->sheet_time = nullptr;
        Free(this->sheet_cont_tex);
        Free(this->sheet_bt_title_tex);
        Free(this->sheet_bt_pair_tex);
        Free(this->sheet_bt_none_tex);
        for(auto &tex : this->sheet_bt_st_texs) {
            Free(tex);
        }
        for(auto &tex : this->sheet_bt_dev_texs) {
            Free(tex);
        }
        this->sheet_bt_dev_texs.clear();
        this->sheet_bt_devs_sig.clear();
        Free(this->sheet_other_label);
        Free(this->sheet_icon_label);
        this->sheet_icon_label_str.clear();
        this->GuideFreeTextures();
        this->sheet_hint_str.clear();
        this->sheet_other_icons.clear();
        this->sheet_hint = nullptr;
    }


    // ---- Bluetooth: one request at a time, and the menu always says how it went ----

    void EntryMenu::BtRequest(const BtmAudioDevice &dev, const bool connect) {
        if(this->bt_pending) {
            return;
        }
        this->bt_failed_t = 0.0;
        Result rc;
        if(connect) {
            // Only one audio device at a time: let go of the current one first
            const auto cur = bt::GetConnectedAudioDevice();
            if((cur.name[0] != '\0') && !bt::AudioDevicesEqual(cur, dev)) {
                bt::DisconnectAudioDevice(cur);
            }
            rc = bt::ConnectAudioDevice(dev);
        }
        else {
            rc = bt::DisconnectAudioDevice(dev);
        }
        if(R_FAILED(rc)) {
            UL_LOG_WARN("[nxdash] Bluetooth %s failed: %s", connect ? "connect" : "disconnect", util::FormatResultDisplay(rc).c_str());
            if(connect) {
                this->bt_failed_addr = dev.addr;
                this->bt_failed_t = this->fx_time + 8.0;
            }
            this->NxSfx(1);
            this->Haptic(0.7f, 110.0f, 0.05);
            return;
        }
        this->bt_pending = true;
        this->bt_pending_connect = connect;
        this->bt_pending_addr = dev.addr;
        this->bt_pending_name = std::string(dev.name, strnlen(dev.name, sizeof(dev.name)));
        this->bt_pending_t = this->fx_time + (connect ? 15.0 : 6.0);
        this->NxSfx(0);
        this->Haptic(0.3f, 200.0f, 0.02);
    }

    void EntryMenu::BtUpdate() {
        if(!this->bt_pending) {
            return;
        }
        const auto cur = bt::GetConnectedAudioDevice();
        const auto is_connected = (cur.name[0] != '\0') && bt::AudioDeviceAddressesEqual(cur.addr, this->bt_pending_addr);
        if(this->bt_pending_connect ? is_connected : !is_connected) {
            this->bt_pending = false;
            g_MenuApplication->ShowNotification(nxt::Sub(nxt::T(this->bt_pending_connect ? "bt_ok" : "bt_off"), "name", this->bt_pending_name), 1800);
            this->NxSfx(2);
            this->Haptic(0.5f, 180.0f, 0.04);
            return;
        }
        if(this->fx_time > this->bt_pending_t) {
            // Nothing happened in time: say so instead of waiting forever
            this->bt_pending = false;
            if(this->bt_pending_connect) {
                this->bt_failed_addr = this->bt_pending_addr;
                this->bt_failed_t = this->fx_time + 8.0;
            }
            this->NxSfx(1);
            this->Haptic(0.7f, 110.0f, 0.05);
        }
    }

    EntryMenu::BtState EntryMenu::BtStateFor(const BtmAudioDevice &dev) {
        if(this->bt_pending && bt::AudioDeviceAddressesEqual(this->bt_pending_addr, dev.addr)) {
            return this->bt_pending_connect ? BtState::Connecting : BtState::Disconnecting;
        }
        const auto cur = bt::GetConnectedAudioDevice();
        if((cur.name[0] != '\0') && bt::AudioDevicesEqual(cur, dev)) {
            return BtState::Connected;
        }
        if((this->fx_time < this->bt_failed_t) && bt::AudioDeviceAddressesEqual(this->bt_failed_addr, dev.addr)) {
            return BtState::Failed;
        }
        return BtState::Idle;
    }

    void EntryMenu::SheetBtOpen() {
        this->sheet_bt_devs = bt::ListPairedAudioDevices();
        this->sheet_bt_sel = 0;
        const auto cur = bt::GetConnectedAudioDevice();
        for(u32 i = 0; i < this->sheet_bt_devs.size(); i++) {
            if((cur.name[0] != '\0') && bt::AudioDevicesEqual(cur, this->sheet_bt_devs.at(i))) {
                this->sheet_bt_sel = (s32)i;
            }
        }
        this->sheet_btl_init = false;
        this->sheet_bt_open = true;
        this->NxSfx(2);
    }

    void EntryMenu::SheetBtActivate() {
        const auto n = (s32)this->sheet_bt_devs.size();
        if(this->sheet_bt_sel >= n) {
            // Pairing happens in Settings, where the search lives
            this->SheetExecute(static_cast<u32>(SheetAction::BluetoothPair));
            return;
        }
        const auto &dev = this->sheet_bt_devs.at(this->sheet_bt_sel);
        const auto state = this->BtStateFor(dev);
        if((state == BtState::Connecting) || (state == BtState::Disconnecting)) {
            return;
        }
        this->BtRequest(dev, state != BtState::Connected);
    }

    void EntryMenu::SheetBtInput(const u64 keys_down) {
        const auto rows = (s32)this->sheet_bt_devs.size() + 1;
        if(keys_down & HidNpadButton_B) {
            this->sheet_bt_open = false;
            this->sheet_lens_init = false;
            this->NxSfx(0);
        }
        else if(keys_down & (HidNpadButton_AnyUp | HidNpadButton_AnyDown)) {
            const auto next = std::clamp(this->sheet_bt_sel + ((keys_down & HidNpadButton_AnyUp) ? -1 : 1), 0, rows - 1);
            if(next != this->sheet_bt_sel) {
                this->sheet_bt_sel = next;
                this->NxSfx(0);
                this->Haptic(0.18f, 160.0f, 0.015);
            }
            else {
                this->NxSfx(1);
                this->Haptic(0.3f, 120.0f, 0.03);
            }
        }
        else if(keys_down & HidNpadButton_A) {
            this->SheetBtActivate();
        }
    }

    void EntryMenu::SheetRenderBluetooth(pu::ui::render::Renderer::Ref &drawer, const s32 ix, const s32 iw, const s32 y0, const s32 hint_y, const double a, const std::function<u8(double)> &ca) {
        // The list follows the console: pairing in Settings shows up here on return
        {
            const auto devs = bt::ListPairedAudioDevices();
            std::string sig;
            for(const auto &dev : devs) {
                sig += std::string(dev.name, strnlen(dev.name, sizeof(dev.name))) + "\n";
            }
            if((sig != this->sheet_bt_devs_sig) || (this->sheet_bt_dev_texs.size() != devs.size())) {
                this->sheet_bt_devs_sig = sig;
                this->sheet_bt_devs = devs;
                for(auto &tex : this->sheet_bt_dev_texs) {
                    Free(tex);
                }
                this->sheet_bt_dev_texs.clear();
                for(const auto &dev : devs) {
                    const auto name = std::string(dev.name, strnlen(dev.name, sizeof(dev.name)));
                    this->sheet_bt_dev_texs.push_back(Text(pu::ui::DefaultFontSize::MediumLarge, name.empty() ? nxt::T("headphones") : name));
                }
                this->sheet_bt_sel = std::min(this->sheet_bt_sel, (s32)devs.size());
            }
        }
        if(this->sheet_bt_title_tex == nullptr) {
            this->sheet_bt_title_tex = Text(pu::ui::DefaultFontSize::MediumLarge, nxt::T("bt_title"));
            this->sheet_bt_pair_tex = Text(pu::ui::DefaultFontSize::MediumLarge, "+   " + nxt::T("bt_pair_new"));
            this->sheet_bt_none_tex = Text(pu::ui::DefaultFontSize::Small, nxt::T("bt_none"));
            this->sheet_bt_st_texs[0] = Text(pu::ui::DefaultFontSize::Small, nxt::T("bt_st_on"));
            this->sheet_bt_st_texs[1] = Text(pu::ui::DefaultFontSize::Small, nxt::T("bt_st_connecting"));
            this->sheet_bt_st_texs[2] = Text(pu::ui::DefaultFontSize::Small, nxt::T("bt_st_disconnecting"));
            this->sheet_bt_st_texs[3] = Text(pu::ui::DefaultFontSize::Small, nxt::T("bt_st_failed"));
        }

        const auto n = (s32)this->sheet_bt_devs.size();
        // Header: the glyph and what this list is
        if(this->sheet_icons[5] != nullptr) {
            drawer->RenderTexture(this->sheet_icons[5], ix + 4, y0 + 2, pu::ui::render::TextureRenderOptions(ca(230.0), 40, 40, {}, {}, {}));
        }
        Draw(drawer, this->sheet_bt_title_tex, ix + 60, y0 + 22 - TexH(this->sheet_bt_title_tex) / 2, ca(255.0));

        const s32 row_h = 76;
        s32 y = y0 + 84;
        if(n == 0) {
            Draw(drawer, this->sheet_bt_none_tex, ix + 4, y, ca(150.0));
            y += TexH(this->sheet_bt_none_tex) + 24;
        }
        const s32 rows_y = y;
        const auto row_top = [&](const s32 row) {
            return rows_y + row * (row_h + 6);
        };

        // The lens
        {
            const auto ty = (double)row_top(this->sheet_bt_sel);
            if(!this->sheet_btl_init) {
                this->sheet_btl_init = true;
                this->sheet_btl_y = ty;
            }
            this->sheet_btl_y = this->FxApproach(this->sheet_btl_y, ty, 0.30);
            RenderGlass(drawer, ix - 12, (s32)this->sheet_btl_y, iw + 24, row_h, 34, a, 100);
            FillRR(drawer, { 0xFF, 0xFF, 0xFF, ca(24.0) }, ix - 12, (s32)this->sheet_btl_y, iw + 24, row_h / 2, 34);
        }

        const auto pulse = 0.55 + 0.45 * std::sin(this->fx_time * 6.0);
        for(s32 i = 0; i <= n; i++) {
            const auto f = i == this->sheet_bt_sel;
            const auto ty = row_top(i);
            const auto mid = ty + row_h / 2;
            const auto ta = f ? 255.0 : 140.0;
            if(i < n) {
                const auto state = this->BtStateFor(this->sheet_bt_devs.at(i));
                if(this->sheet_icons[3] != nullptr) {
                    drawer->RenderTexture(this->sheet_icons[3], ix + 6, mid - 18, pu::ui::render::TextureRenderOptions(ca(ta), 36, 36, {}, {}, {}));
                }
                auto name = this->sheet_bt_dev_texs.at(i);
                RenderTextFit(drawer, name, (double)(ix + 64), mid - TexH(name) / 2, (double)(iw - 260), ca(ta), false);
                pu::sdl2::Texture st = nullptr;
                double sa = 220.0;
                switch(state) {
                    case BtState::Connected: {
                        st = this->sheet_bt_st_texs[0];
                        SDL_SetTextureColorMod(st, 0x5B, 0xE3, 0x8A);
                        break;
                    }
                    case BtState::Connecting:
                    case BtState::Disconnecting: {
                        st = this->sheet_bt_st_texs[(state == BtState::Connecting) ? 1 : 2];
                        sa = 255.0 * pulse;
                        break;
                    }
                    case BtState::Failed: {
                        st = this->sheet_bt_st_texs[3];
                        SDL_SetTextureColorMod(st, 0xFF, 0x6B, 0x5E);
                        break;
                    }
                    default: {
                        break;
                    }
                }
                if(st != nullptr) {
                    if(state == BtState::Connected) {
                        FillRR(drawer, { 0x5B, 0xE3, 0x8A, ca(255.0) }, ix + iw - TexW(st) - 26, mid - 6, 12, 12, 6);
                    }
                    Draw(drawer, st, ix + iw - TexW(st), mid - TexH(st) / 2, ca(sa));
                }
            }
            else {
                Draw(drawer, this->sheet_bt_pair_tex, ix + 10, mid - TexH(this->sheet_bt_pair_tex) / 2, ca(f ? 255.0 : 140.0));
            }
            this->sheet_hits.push_back({ 30 + i, ix - 12, ty, iw + 24, row_h });
        }

        // One line: what A does on this row
        {
            std::string hint;
            if(this->sheet_bt_sel >= n) {
                hint = nxt::T("h_bt_pair");
            }
            else {
                const auto &dev = this->sheet_bt_devs.at(this->sheet_bt_sel);
                switch(this->BtStateFor(dev)) {
                    case BtState::Connected: hint = nxt::T("h_bt_disconnect"); break;
                    case BtState::Connecting: hint = nxt::Sub(nxt::T("h_bt_wait"), "name", std::string(dev.name, strnlen(dev.name, sizeof(dev.name)))); break;
                    case BtState::Disconnecting: hint = nxt::Sub(nxt::T("h_bt_wait"), "name", std::string(dev.name, strnlen(dev.name, sizeof(dev.name)))); break;
                    case BtState::Failed: hint = nxt::T("h_bt_failed"); break;
                    default: hint = nxt::T("h_bt_connect"); break;
                }
            }
            if(hint != this->sheet_hint_str) {
                this->sheet_hint_str = hint;
                Free(this->sheet_hint);
                this->sheet_hint = Text(pu::ui::DefaultFontSize::Small, hint);
            }
            RenderTextFit(drawer, this->sheet_hint, (double)ix, hint_y, (double)iw, ca(125.0), false);
        }
    }

    // ---- Guide: per-game notes for when you get stuck, read from the SD card ----

    namespace {

        std::vector<std::string> WrapText(const pu::ui::DefaultFontSize size, const std::string &text, const s32 max_w) {
            std::vector<std::string> out;
            const auto font = pu::ui::GetDefaultFont(size);
            std::string line;
            size_t pos = 0;
            while(pos <= text.length()) {
                auto sp = text.find(' ', pos);
                if(sp == std::string::npos) {
                    sp = text.length();
                }
                const auto word = text.substr(pos, sp - pos);
                pos = sp + 1;
                if(word.empty()) {
                    continue;
                }
                const auto cand = line.empty() ? word : (line + " " + word);
                if(!line.empty() && (pu::ui::render::GetTextWidth(font, cand) > max_w)) {
                    out.push_back(line);
                    line = word;
                }
                else {
                    line = cand;
                }
            }
            if(!line.empty()) {
                out.push_back(line);
            }
            return out;
        }

        constexpr s32 GuidePanelW = 1240;
        constexpr s32 GuidePad = 56;
        constexpr s32 GuideIW = GuidePanelW - GuidePad * 2;
        constexpr s32 GuideLineH = 36;

    }

    // ---- Guide progress: read the game's own save and work out what's done ----

    namespace {

        std::string Trim(const std::string &s) {
            const auto a = s.find_first_not_of(" \t");
            if(a == std::string::npos) {
                return "";
            }
            const auto b = s.find_last_not_of(" \t");
            return s.substr(a, b - a + 1);
        }

        double JsonNum(const util::JSON &v) {
            if(v.is_boolean()) {
                return v.get<bool>() ? 1.0 : 0.0;
            }
            if(v.is_number()) {
                return v.get<double>();
            }
            return 0.0;
        }

        // A list entry inside playerData, e.g. Tools.savedData[] = { Name, Data }
        const util::JSON *FindSaved(const util::JSON &pd, const char *list, const std::string &name, const bool partial) {
            if(!pd.contains(list) || !pd[list].is_object() || !pd[list].contains("savedData") || !pd[list]["savedData"].is_array()) {
                return nullptr;
            }
            for(const auto &it : pd[list]["savedData"]) {
                if(!it.is_object() || !it.contains("Name") || !it["Name"].is_string() || !it.contains("Data") || !it["Data"].is_object()) {
                    continue;
                }
                const auto n = it["Name"].get<std::string>();
                if(partial ? (n.find(name) != std::string::npos) : (n == name)) {
                    return &it["Data"];
                }
            }
            return nullptr;
        }

        // Terms: pd.KEY · tool.NAME · tool~PART · item.NAME · quest.NAME · count.PREFIX · hours · number
        double EvalTerm(const util::JSON &pd, const std::string &raw) {
            const auto t = Trim(raw);
            if(t.empty()) {
                return 0.0;
            }
            if(t == "hours") {
                return pd.contains("playTime") ? JsonNum(pd["playTime"]) / 3600.0 : 0.0;
            }
            if(t == "relics") {
                // Collected but not handed in yet
                double n = 0.0;
                if(pd.contains("Relics") && pd["Relics"].is_object() && pd["Relics"].contains("savedData") && pd["Relics"]["savedData"].is_array()) {
                    for(const auto &it : pd["Relics"]["savedData"]) {
                        if(it.is_object() && it.contains("Data") && it["Data"].is_object()) {
                            const auto &d = it["Data"];
                            const auto col = d.contains("IsCollected") && (JsonNum(d["IsCollected"]) > 0.0);
                            const auto dep = d.contains("IsDeposited") && (JsonNum(d["IsDeposited"]) > 0.0);
                            if(col && !dep) {
                                n += 1.0;
                            }
                        }
                    }
                }
                return n;
            }
            if(t.rfind("pd.", 0) == 0) {
                const auto k = t.substr(3);
                return pd.contains(k) ? JsonNum(pd[k]) : 0.0;
            }
            if((t.rfind("tool.", 0) == 0) || (t.rfind("tool~", 0) == 0)) {
                const auto d = FindSaved(pd, "Tools", t.substr(5), t[4] == '~');
                return ((d != nullptr) && d->contains("IsUnlocked")) ? JsonNum((*d)["IsUnlocked"]) : 0.0;
            }
            if(t.rfind("item.", 0) == 0) {
                const auto d = FindSaved(pd, "Collectables", t.substr(5), false);
                return ((d != nullptr) && d->contains("Amount")) ? JsonNum((*d)["Amount"]) : 0.0;
            }
            if(t.rfind("quest.", 0) == 0) {
                const auto d = FindSaved(pd, "QuestCompletionData", t.substr(6), false);
                return ((d != nullptr) && d->contains("IsCompleted")) ? JsonNum((*d)["IsCompleted"]) : 0.0;
            }
            if(t.rfind("count.", 0) == 0) {
                const auto p = t.substr(6);
                double n = 0.0;
                for(auto it = pd.begin(); it != pd.end(); ++it) {
                    if((it.key().rfind(p, 0) == 0) && (JsonNum(it.value()) > 0.0)) {
                        n += 1.0;
                    }
                }
                return n;
            }
            char *end = nullptr;
            const auto v = strtod(t.c_str(), &end);
            return ((end != nullptr) && (*end == '\0')) ? v : 0.0;
        }

        // a + b - c * 2 (left to right, no precedence needed for our uses)
        double EvalExpr(const util::JSON &pd, const std::string &e) {
            double acc = 0.0;
            char op = '+';
            std::string cur;
            const auto apply = [&]() {
                const auto v = EvalTerm(pd, cur);
                switch(op) {
                    case '+': acc += v; break;
                    case '-': acc -= v; break;
                    case '*': acc *= v; break;
                }
                cur.clear();
            };
            for(size_t i = 0; i < e.length(); i++) {
                const auto c = e[i];
                // A '-' inside a key (pd.Some-Key) is unlikely; only treat spaced minus as an operator
                const auto is_op = (c == '+') || (c == '*') || ((c == '-') && (i > 0) && (e[i - 1] == ' '));
                if(is_op) {
                    apply();
                    op = c;
                }
                else {
                    cur += c;
                }
            }
            apply();
            return acc;
        }

        // cond && cond, each: [!]expr [>= == <= > < N]
        bool EvalAnd(const util::JSON &pd, const std::string &cond) {
            size_t start = 0;
            while(start <= cond.length()) {
                auto amp = cond.find("&&", start);
                if(amp == std::string::npos) {
                    amp = cond.length();
                }
                auto part = Trim(cond.substr(start, amp - start));
                start = amp + 2;
                if(part.empty()) {
                    continue;
                }
                auto neg = false;
                if(part[0] == '!') {
                    neg = true;
                    part = Trim(part.substr(1));
                }
                bool ok;
                static const char *Ops[] = { ">=", "<=", "==", ">", "<" };
                size_t op_pos = std::string::npos;
                std::string op;
                for(const auto o : Ops) {
                    const auto p = part.find(o);
                    if(p != std::string::npos) {
                        op_pos = p;
                        op = o;
                        break;
                    }
                }
                if(op_pos == std::string::npos) {
                    ok = EvalExpr(pd, part) > 0.0;
                }
                else {
                    const auto l = EvalExpr(pd, part.substr(0, op_pos));
                    const auto r = EvalExpr(pd, part.substr(op_pos + op.length()));
                    if(op == ">=") ok = l >= r;
                    else if(op == "<=") ok = l <= r;
                    else if(op == "==") ok = std::abs(l - r) < 0.001;
                    else if(op == ">") ok = l > r;
                    else ok = l < r;
                }
                if(neg) {
                    ok = !ok;
                }
                if(!ok) {
                    return false;
                }
            }
            return true;
        }

        // a || b, each side an && chain
        bool EvalCond(const util::JSON &pd, const std::string &cond) {
            size_t start = 0;
            while(start <= cond.length()) {
                auto bar = cond.find("||", start);
                if(bar == std::string::npos) {
                    bar = cond.length();
                }
                if(EvalAnd(pd, cond.substr(start, bar - start))) {
                    return true;
                }
                start = bar + 2;
            }
            return false;
        }

        // Every {expr} inside a string
        void CollectTemplates(const std::string &s, std::vector<std::string> &out) {
            size_t p = 0;
            while((p = s.find('{', p)) != std::string::npos) {
                const auto q = s.find('}', p);
                if(q == std::string::npos) {
                    break;
                }
                out.push_back(s.substr(p + 1, q - p - 1));
                p = q + 1;
            }
        }

        void CollectGuideExprs(const util::JSON &g, std::vector<std::string> &exprs, std::vector<std::string> &conds) {
            if(g.is_string()) {
                CollectTemplates(g.get<std::string>(), exprs);
                return;
            }
            if(g.is_array()) {
                for(const auto &x : g) {
                    CollectGuideExprs(x, exprs, conds);
                }
                return;
            }
            if(g.is_object()) {
                for(auto it = g.begin(); it != g.end(); ++it) {
                    if(((it.key() == "done") || (it.key() == "hide") || (it.key() == "show")) && it.value().is_string()) {
                        conds.push_back(it.value().get<std::string>());
                    }
                    else if(it.key() != "scenes") {
                        CollectGuideExprs(it.value(), exprs, conds);
                    }
                }
            }
        }

        std::string GuidePath(const u64 app_id, const char *suffix) {
            char path[FS_MAX_PATH] = {};
            snprintf(path, sizeof(path), "sdmc:/ulaunch/nxdash_guides/%016lX%s", app_id, suffix);
            return path;
        }

    }

    bool NxGuideSync(const u64 app_id) {
        if(app_id == 0) {
            return false;
        }
        util::JSON guide;
        if(R_FAILED(util::LoadJSONFromFile(guide, GuidePath(app_id, ".json")))) {
            return false;
        }
        if(!guide.is_object() || !guide.contains("save_slot") || !guide["save_slot"].is_string()) {
            // Guides you tick by hand: nothing to read
            return false;
        }
        const auto uid = g_GlobalSettings.system_status.selected_user;
        if(!accountUidIsValid(&uid)) {
            return false;
        }
        const FsSaveDataAttribute attr = {
            .application_id = app_id,
            .uid = uid,
            .system_save_data_id = 0,
            .save_data_type = FsSaveDataType_Account,
            .save_data_rank = FsSaveDataRank_Primary,
            .save_data_index = 0
        };
        FsFileSystem fs;
        const auto rc = fsOpenSaveDataFileSystem(&fs, FsSaveDataSpaceId_User, &attr);
        if(R_FAILED(rc)) {
            // Usually: the game is open and holds its save
            UL_LOG_WARN("NX Dash guide: could not open save of 0x%016lX: %s", app_id, util::FormatResultDisplay(rc).c_str());
            return false;
        }
        // Read every slot (user1..user4.dat) and keep the one with the most play time
        util::JSON best;
        double best_time = -1.0;
        const auto slot_fmt = guide["save_slot"].get<std::string>();
        for(s32 i = 1; i <= 4; i++) {
            char fpath[FS_MAX_PATH] = {};
            snprintf(fpath, sizeof(fpath), slot_fmt.c_str(), i);
            FsFile file;
            if(R_FAILED(fsFsOpenFile(&fs, fpath, FsOpenMode_Read, &file))) {
                continue;
            }
            s64 size = 0;
            if(R_SUCCEEDED(fsFileGetSize(&file, &size)) && (size > 0) && (size < 16 * 1024 * 1024)) {
                std::string buf((size_t)size, '\0');
                u64 read = 0;
                if(R_SUCCEEDED(fsFileRead(&file, 0, buf.data(), (u64)size, FsReadOption_None, &read)) && (read == (u64)size)) {
                    auto j = util::JSON::parse(buf, nullptr, false);
                    if(!j.is_discarded() && j.is_object() && j.contains("playerData") && j["playerData"].is_object()) {
                        const auto &pd = j["playerData"];
                        const auto t = pd.contains("playTime") ? JsonNum(pd["playTime"]) : 0.0;
                        if(t > best_time) {
                            best_time = t;
                            best = std::move(j);
                        }
                    }
                }
            }
            fsFileClose(&file);
        }
        fsFsClose(&fs);
        if(best_time < 0.0) {
            return false;
        }

        const auto &pd = best["playerData"];
        std::vector<std::string> exprs;
        std::vector<std::string> conds;
        CollectGuideExprs(guide, exprs, conds);
        util::JSON vals = util::JSON::object();
        for(const auto &e : exprs) {
            if(e != "bench") {
                vals[e] = EvalExpr(pd, e);
            }
        }
        for(const auto &c : conds) {
            vals[c] = EvalCond(pd, c) ? 1.0 : 0.0;
        }
        util::JSON progress = util::JSON::object();
        progress["time"] = (s64)::time(nullptr);
        progress["scene"] = (pd.contains("respawnScene") && pd["respawnScene"].is_string()) ? pd["respawnScene"].get<std::string>() : std::string();
        progress["v"] = vals;
        const auto ok = util::SaveJSON(GuidePath(app_id, ".progress.json"), progress);
        UL_LOG_INFO("NX Dash guide: synced 0x%016lX (%d values)", app_id, (s32)vals.size());
        return ok;
    }

    void EntryMenu::GuideLoad(const u64 app_id) {
        this->guide_avail = false;
        this->guide_status.clear();
        this->guide_stats.clear();
        this->guide_tabs.clear();
        this->guide_sync_str.clear();
        if(app_id == 0) {
            return;
        }
        util::JSON json;
        if(R_FAILED(util::LoadJSONFromFile(json, GuidePath(app_id, ".json"))) || !json.is_object()) {
            return;
        }
        this->guide_has_save = json.contains("save_slot") && json["save_slot"].is_string();
        // Steps you ticked yourself with X
        this->guide_checks.clear();
        {
            util::JSON checks;
            if(R_SUCCEEDED(util::LoadJSONFromFile(checks, GuidePath(app_id, ".checks.json"))) && checks.is_array()) {
                for(const auto &c : checks) {
                    if(c.is_string()) {
                        this->guide_checks.push_back(c.get<std::string>());
                    }
                }
            }
        }
        const auto is_checked = [&](const std::string &id) {
            return std::find(this->guide_checks.begin(), this->guide_checks.end(), id) != this->guide_checks.end();
        };
        // What we last read from the save (may be missing: then templates show "?" and nothing is ticked)
        util::JSON progress;
        const auto has_progress = R_SUCCEEDED(util::LoadJSONFromFile(progress, GuidePath(app_id, ".progress.json"))) && progress.is_object() && progress.contains("v") && progress["v"].is_object();
        const auto val = [&](const std::string &key, double &out) -> bool {
            if(!has_progress || !progress["v"].contains(key)) {
                return false;
            }
            out = JsonNum(progress["v"][key]);
            return true;
        };
        const auto cond_true = [&](const util::JSON &o, const char *key) -> int {
            // -1: no condition or unknown, 0: false, 1: true
            if(!o.is_object() || !o.contains(key) || !o[key].is_string()) {
                return -1;
            }
            double v = 0.0;
            if(!val(o[key].get<std::string>(), v)) {
                return -1;
            }
            return (v > 0.0) ? 1 : 0;
        };
        std::string bench;
        if(has_progress && progress.contains("scene") && progress["scene"].is_string() && json.contains("scenes") && json["scenes"].is_object()) {
            const auto scene = progress["scene"].get<std::string>();
            size_t best_len = 0;
            for(auto it = json["scenes"].begin(); it != json["scenes"].end(); ++it) {
                if(it.value().is_string() && (scene.rfind(it.key(), 0) == 0) && (it.key().length() > best_len)) {
                    best_len = it.key().length();
                    bench = it.value().get<std::string>();
                }
            }
        }
        const auto fill = [&](std::string s) -> std::string {
            size_t p = 0;
            while((p = s.find('{', p)) != std::string::npos) {
                const auto q = s.find('}', p);
                if(q == std::string::npos) {
                    break;
                }
                const auto e = s.substr(p + 1, q - p - 1);
                std::string rep = "?";
                double v = 0.0;
                if(e == "bench") {
                    if(!bench.empty()) {
                        rep = bench;
                    }
                }
                else if(val(e, v)) {
                    rep = std::to_string((s64)std::floor(v + 0.0001));
                }
                s.replace(p, q - p + 1, rep);
                p += rep.length();
            }
            return s;
        };
        // No exceptions in this build: check every type before reading it
        const auto str = [&](const util::JSON &o, const char *key) -> std::string {
            if(o.is_object() && o.contains(key) && o[key].is_string()) {
                return fill(o[key].get<std::string>());
            }
            return "";
        };
        auto lang = nxt::T("guide_lang");
        if(!json.contains(lang) || !json[lang].is_object()) {
            lang = "en";
        }
        if(!json.contains(lang) || !json[lang].is_object()) {
            return;
        }
        const auto &g = json[lang];
        this->guide_status = str(g, "status");
        if(g.contains("stats") && g["stats"].is_array()) {
            for(const auto &st : g["stats"]) {
                if(!st.is_array()) {
                    continue;
                }
                std::vector<std::string> v;
                for(const auto &x : st) {
                    v.push_back(x.is_string() ? fill(x.get<std::string>()) : "");
                }
                v.resize(3);
                this->guide_stats.push_back(v);
                if(this->guide_stats.size() >= 4) {
                    break;
                }
            }
        }
        if(g.contains("tabs") && g["tabs"].is_array()) {
            s32 tab_i = -1;
            for(const auto &t : g["tabs"]) {
                tab_i++;
                if(cond_true(t, "show") == 0) {
                    continue;
                }
                const auto tab_manual = t.is_object() && t.contains("manual") && t["manual"].is_boolean() && t["manual"].get<bool>();
                s32 row_i = -1;
                GuideTab tab;
                tab.title = str(t, "title");
                const auto is_path = t.is_object() && t.contains("path") && t["path"].is_boolean() && t["path"].get<bool>();
                const auto sink_done = t.is_object() && t.contains("sink_done") && t["sink_done"].is_boolean() && t["sink_done"].get<bool>();
                std::vector<GuideRow> done_rows;
                auto here_set = false;
                if(t.is_object() && t.contains("rows") && t["rows"].is_array()) {
                    for(const auto &r : t["rows"]) {
                        row_i++;
                        if(cond_true(r, "hide") == 1) {
                            continue;
                        }
                        GuideRow row;
                        row.text = str(r, "text");
                        row.sub = str(r, "sub");
                        row.zone = str(r, "zone");
                        row.mark = str(r, "mark");
                        if(r.is_object() && r.contains("hints") && r["hints"].is_array()) {
                            for(const auto &h : r["hints"]) {
                                if(h.is_string()) {
                                    row.hints.push_back(fill(h.get<std::string>()));
                                }
                            }
                        }
                        if(row.text.empty()) {
                            continue;
                        }
                        row.id = (r.is_object() && r.contains("id") && r["id"].is_string()) ? r["id"].get<std::string>() : ("t" + std::to_string(tab_i) + "r" + std::to_string(row_i));
                        row.manual = (tab_manual || (r.is_object() && r.contains("check") && r["check"].is_boolean() && r["check"].get<bool>())) && !(r.is_object() && r.contains("done"));
                        auto done = cond_true(r, "done");
                        if(row.manual) {
                            done = is_checked(row.id) ? 1 : 0;
                        }
                        if(done == 1) {
                            row.mark = "done";
                        }
                        else if(done == 0) {
                            row.mark.clear();
                            // On the path, the first step you haven't done is where you are
                            if(is_path && !here_set) {
                                row.mark = "here";
                                here_set = true;
                            }
                        }
                        if(sink_done && (row.mark == "done")) {
                            done_rows.push_back(row);
                        }
                        else {
                            tab.rows.push_back(row);
                        }
                    }
                }
                tab.rows.insert(tab.rows.end(), done_rows.begin(), done_rows.end());
                if(!tab.rows.empty()) {
                    this->guide_tabs.push_back(tab);
                }
            }
        }
        // When was the save read
        if(has_progress && progress.contains("time") && progress["time"].is_number()) {
            const auto then = (s64)JsonNum(progress["time"]);
            const auto now = (s64)::time(nullptr);
            const auto diff = now - then;
            if(then <= 0) {
                this->guide_sync_str = nxt::T("g_sync_backup");
            }
            else if(diff < 90) {
                this->guide_sync_str = nxt::T("g_sync_now");
            }
            else if(diff < 3600) {
                this->guide_sync_str = nxt::Sub(nxt::T("g_sync_min"), "n", std::to_string(diff / 60));
            }
            else if(diff < 86400) {
                this->guide_sync_str = nxt::Sub(nxt::T("g_sync_hours"), "n", std::to_string(diff / 3600));
            }
            else {
                this->guide_sync_str = nxt::Sub(nxt::T("g_sync_days"), "n", std::to_string(std::max<s64>(1, diff / 86400)));
            }
        }
        this->guide_avail = !this->guide_tabs.empty();
    }

    void EntryMenu::GuideFreeRows() {
        for(auto &rt : this->guide_row_texs) {
            Free(rt.title);
            Free(rt.zone);
            for(auto &t : rt.sub) {
                Free(t);
            }
            for(auto &h : rt.hints) {
                for(auto &t : h) {
                    Free(t);
                }
            }
        }
        this->guide_row_texs.clear();
        this->guide_built_tab = -1;
    }

    void EntryMenu::GuideFreeTextures() {
        this->GuideFreeRows();
        Free(this->guide_title_tex);
        Free(this->guide_game_tex);
        Free(this->guide_status_tex);
        Free(this->guide_lr_tex);
        Free(this->guide_more_tex);
        this->guide_more_str.clear();
        for(auto &t : this->guide_stat_texs) {
            Free(t);
        }
        this->guide_stat_texs.clear();
        for(auto &t : this->guide_tab_texs) {
            Free(t);
        }
        this->guide_tab_texs.clear();
    }

    void EntryMenu::GuideBuildRows() {
        this->GuideFreeRows();
        if((this->guide_tab < 0) || (this->guide_tab >= (s32)this->guide_tabs.size())) {
            return;
        }
        for(const auto &row : this->guide_tabs.at(this->guide_tab).rows) {
            GuideRowTex rt;
            rt.title = Text(pu::ui::DefaultFontSize::Medium, row.text);
            if(!row.zone.empty()) {
                rt.zone = Text(pu::ui::DefaultFontSize::Small, row.zone);
            }
            if(!row.sub.empty()) {
                for(const auto &l : WrapText(pu::ui::DefaultFontSize::Small, row.sub, GuideIW - 70)) {
                    rt.sub.push_back(Text(pu::ui::DefaultFontSize::Small, l));
                }
            }
            for(const auto &h : row.hints) {
                std::vector<pu::sdl2::Texture> lines;
                for(const auto &l : WrapText(pu::ui::DefaultFontSize::Small, h, GuideIW - 110)) {
                    lines.push_back(Text(pu::ui::DefaultFontSize::Small, l));
                }
                rt.hints.push_back(lines);
            }
            this->guide_row_texs.push_back(rt);
        }
        this->guide_rev.assign(this->guide_row_texs.size(), 0);
        this->guide_built_tab = this->guide_tab;
    }

    void EntryMenu::GuideToggleCheck() {
        if((this->guide_tab < 0) || (this->guide_tab >= (s32)this->guide_tabs.size())) {
            return;
        }
        const auto &rows = this->guide_tabs.at(this->guide_tab).rows;
        if((this->guide_sel < 0) || (this->guide_sel >= (s32)rows.size()) || !rows.at(this->guide_sel).manual) {
            this->NxSfx(1);
            this->Haptic(0.3f, 120.0f, 0.03);
            return;
        }
        const auto id = rows.at(this->guide_sel).id;
        const auto it = std::find(this->guide_checks.begin(), this->guide_checks.end(), id);
        const auto now_done = it == this->guide_checks.end();
        if(now_done) {
            this->guide_checks.push_back(id);
        }
        else {
            this->guide_checks.erase(it);
        }
        util::JSON arr = util::JSON::array();
        for(const auto &c : this->guide_checks) {
            arr.push_back(c);
        }
        util::SaveJSON(GuidePath(g_GlobalSettings.system_status.suspended_app_id, ".checks.json"), arr);
        this->GuideRefresh(true, false);
        this->NxSfx(now_done ? 2 : 0);
        this->Haptic(now_done ? 0.3f : 0.18f, 160.0f, 0.03);
    }

    void EntryMenu::GuideRefresh(const bool keep_place, const bool do_sync) {
        const auto app_id = g_GlobalSettings.system_status.suspended_app_id;
        const auto synced = do_sync ? NxGuideSync(app_id) : !this->guide_sync_failed;
        const auto tab = this->guide_tab;
        const auto sel = this->guide_sel;
        this->GuideLoad(app_id);
        this->guide_sync_failed = this->guide_has_save && !synced;
        if(keep_place && this->guide_avail) {
            this->GuideFreeTextures();
            this->guide_title_tex = Text(pu::ui::DefaultFontSize::MediumLarge, nxt::T("guide"));
            const auto line = !this->guide_has_save ? nxt::T("g_manual") : (this->guide_sync_failed ? nxt::T("g_sync_locked") : this->guide_sync_str);
            if(!line.empty()) {
                this->guide_game_tex = Text(pu::ui::DefaultFontSize::Small, line);
            }
            if(!this->guide_status.empty()) {
                this->guide_status_tex = Text(pu::ui::DefaultFontSize::Small, this->guide_status);
            }
            this->guide_lr_tex = Text(pu::ui::DefaultFontSize::Small, "L  R");
            for(const auto &st : this->guide_stats) {
                this->guide_stat_texs.push_back(Text(pu::ui::DefaultFontSize::Large, st.at(0)));
                this->guide_stat_texs.push_back(st.at(1).empty() ? nullptr : Text(pu::ui::DefaultFontSize::Small, st.at(1)));
                this->guide_stat_texs.push_back(st.at(2).empty() ? nullptr : Text(pu::ui::DefaultFontSize::Small, st.at(2)));
            }
            for(const auto &t : this->guide_tabs) {
                this->guide_tab_texs.push_back(Text(pu::ui::DefaultFontSize::Medium, t.title));
            }
            this->guide_tab = std::clamp(tab, 0, (s32)this->guide_tabs.size() - 1);
            this->guide_sel = sel;
            this->guide_init = false;
            this->GuideBuildRows();
            this->guide_sel = std::clamp(sel, 0, std::max(0, (s32)this->guide_row_texs.size() - 1));
            this->sheet_hint_str.clear();
        }
        else if(keep_place) {
            // The guide file went away: back to the panel
            this->sheet_guide_open = false;
        }
    }

    void EntryMenu::GuideOpen() {
        if(!this->guide_avail) {
            return;
        }
        this->GuideRefresh(false);
        if(!this->guide_avail) {
            return;
        }
        this->GuideFreeTextures();
        this->guide_title_tex = Text(pu::ui::DefaultFontSize::MediumLarge, nxt::T("guide"));
        {
            std::string line = this->guide_sync_str;
            if(!this->guide_has_save) {
                line = nxt::T("g_manual");
            }
            else if(this->guide_sync_failed) {
                line = nxt::T("g_sync_locked");
            }
            if(!line.empty()) {
                this->guide_game_tex = Text(pu::ui::DefaultFontSize::Small, line);
            }
        }
        if(!this->guide_status.empty()) {
            this->guide_status_tex = Text(pu::ui::DefaultFontSize::Small, this->guide_status);
        }
        this->guide_lr_tex = Text(pu::ui::DefaultFontSize::Small, "L  R");
        for(const auto &st : this->guide_stats) {
            this->guide_stat_texs.push_back(Text(pu::ui::DefaultFontSize::Large, st.at(0)));
            this->guide_stat_texs.push_back(st.at(1).empty() ? nullptr : Text(pu::ui::DefaultFontSize::Small, st.at(1)));
            this->guide_stat_texs.push_back(st.at(2).empty() ? nullptr : Text(pu::ui::DefaultFontSize::Small, st.at(2)));
        }
        for(const auto &t : this->guide_tabs) {
            this->guide_tab_texs.push_back(Text(pu::ui::DefaultFontSize::Medium, t.title));
        }
        if(this->guide_icon == nullptr) {
            this->guide_icon = TryFindLoadImage("ui/Main/NxDash/Book");
        }
        this->guide_tab = 0;
        this->guide_sel = 0;
        this->guide_scroll = 0.0;
        this->guide_init = false;
        this->guide_tx = 0.0;
        this->guide_tw = 0.0;
        this->GuideBuildRows();
        this->sheet_hint_str.clear();
        this->sheet_guide_open = true;
    }

    void EntryMenu::GuideSetTab(const s32 tab) {
        const auto n = (s32)this->guide_tabs.size();
        if(n == 0) {
            return;
        }
        this->guide_tab = ((tab % n) + n) % n;
        this->guide_sel = 0;
        this->guide_scroll = 0.0;
        this->guide_init = false;
        this->GuideBuildRows();
    }

    void EntryMenu::GuideActivate() {
        if((this->guide_sel < 0) || (this->guide_sel >= (s32)this->guide_row_texs.size())) {
            return;
        }
        const auto nh = (s32)this->guide_row_texs.at(this->guide_sel).hints.size();
        if(nh == 0) {
            this->NxSfx(1);
            return;
        }
        auto &rev = this->guide_rev.at(this->guide_sel);
        rev = (rev >= nh) ? 0 : (rev + 1);
        this->NxSfx(2);
        this->Haptic(0.2f, 160.0f, 0.02);
    }

    void EntryMenu::GuideInput(const u64 keys_down) {
        if(keys_down & HidNpadButton_B) {
            this->sheet_guide_open = false;
            this->sheet_hint_str.clear();
            this->NxSfx(1);
            return;
        }
        if(keys_down & HidNpadButton_X) {
            this->GuideToggleCheck();
            return;
        }
        if((keys_down & HidNpadButton_Y) && this->guide_has_save) {
            this->GuideRefresh(true);
            this->NxSfx(this->guide_sync_failed ? 1 : 2);
            this->Haptic(0.25f, 160.0f, 0.03);
            return;
        }
        const auto rows = (s32)this->guide_row_texs.size();
        if(keys_down & (HidNpadButton_L | HidNpadButton_ZL | HidNpadButton_AnyLeft)) {
            this->GuideSetTab(this->guide_tab - 1);
            this->NxSfx(0);
            this->Haptic(0.18f, 160.0f, 0.015);
        }
        else if(keys_down & (HidNpadButton_R | HidNpadButton_ZR | HidNpadButton_AnyRight)) {
            this->GuideSetTab(this->guide_tab + 1);
            this->NxSfx(0);
            this->Haptic(0.18f, 160.0f, 0.015);
        }
        else if(keys_down & (HidNpadButton_AnyUp | HidNpadButton_AnyDown)) {
            const auto next = std::clamp(this->guide_sel + ((keys_down & HidNpadButton_AnyUp) ? -1 : 1), 0, std::max(0, rows - 1));
            if(next != this->guide_sel) {
                this->guide_sel = next;
                this->NxSfx(0);
                this->Haptic(0.18f, 160.0f, 0.015);
            }
            else {
                this->NxSfx(1);
                this->Haptic(0.3f, 120.0f, 0.03);
            }
        }
        else if(keys_down & HidNpadButton_A) {
            this->GuideActivate();
        }
    }

    void EntryMenu::SheetRenderGuide(pu::ui::render::Renderer::Ref &drawer, const s32 px, const s32 py, const s32 pw, const s32 ph, const double a, const std::function<u8(double)> &ca_in) {
        if(this->guide_built_tab != this->guide_tab) {
            this->GuideBuildRows();
        }
        // The panel widens first, then the guide fades in
        const auto grow = std::clamp(((double)pw - 640.0) / (double)(GuidePanelW - 640), 0.0, 1.0);
        const auto ca = [&](const double v) {
            return (u8)std::clamp((double)ca_in(v) * grow, 0.0, 255.0);
        };
        const s32 ix = px + GuidePad;
        const s32 iw = GuideIW;
        auto sdl = pu::ui::render::GetMainRenderer();
        const SDL_Rect panel_clip = { px, py, pw, ph };
        SDL_RenderSetClipRect(sdl, &panel_clip);

        s32 y = py + 48;
        // Header: what this is, which game, where you are
        if(this->guide_icon != nullptr) {
            drawer->RenderTexture(this->guide_icon, ix, y + 2, pu::ui::render::TextureRenderOptions(ca(235.0), 44, 44, {}, {}, {}));
        }
        Draw(drawer, this->guide_title_tex, ix + 62, y + 24 - TexH(this->guide_title_tex) / 2, ca(255.0));
        if(this->guide_game_tex != nullptr) {
            const auto gw = std::min(TexW(this->guide_game_tex), iw - 320);
            RenderTextFit(drawer, this->guide_game_tex, (double)(ix + iw - gw), y + 8, (double)gw, ca(150.0), false);
        }
        y += 52;
        if(this->guide_status_tex != nullptr) {
            RenderTextFit(drawer, this->guide_status_tex, (double)(ix + 62), y, (double)(iw - 62), ca(165.0), false);
        }
        y += 58;

        // Four numbers, quiet tiles
        if(!this->guide_stats.empty()) {
            const auto n = (s32)this->guide_stats.size();
            const s32 gap = 18;
            const s32 tw = (iw - (n - 1) * gap) / n;
            for(s32 i = 0; i < n; i++) {
                const auto x = ix + i * (tw + gap);
                FillRR(drawer, { 0xFF, 0xFF, 0xFF, ca(14.0) }, x, y, tw, 96, 22);
                auto v = this->guide_stat_texs.at(i * 3);
                auto l = this->guide_stat_texs.at(i * 3 + 1);
                auto s = this->guide_stat_texs.at(i * 3 + 2);
                Draw(drawer, v, x + 22, y + 48 - TexH(v) / 2, ca(255.0));
                const auto lx = x + 34 + TexW(v);
                const auto lw = (double)(tw - (lx - x) - 16);
                RenderTextFit(drawer, l, (double)lx, y + 14, lw, ca(215.0), false);
                RenderTextFit(drawer, s, (double)lx, y + 50, lw, ca(130.0), false);
            }
            y += 128;
        }

        // Sections: plain text, one underline that slides
        {
            s32 x = ix;
            s32 cur_x = ix;
            s32 cur_w = 0;
            for(s32 i = 0; i < (s32)this->guide_tab_texs.size(); i++) {
                auto t = this->guide_tab_texs.at(i);
                const auto f = i == this->guide_tab;
                Draw(drawer, t, x, y, ca(f ? 255.0 : 120.0));
                if(f) {
                    cur_x = x;
                    cur_w = TexW(t);
                }
                this->sheet_hits.push_back({ 70 + i, x - 10, y - 10, TexW(t) + 20, 56 });
                x += TexW(t) + 44;
            }
            if(this->guide_tw <= 0.0) {
                this->guide_tx = cur_x;
                this->guide_tw = cur_w;
            }
            this->guide_tx = this->FxApproach(this->guide_tx, cur_x, 0.30);
            this->guide_tw = this->FxApproach(this->guide_tw, cur_w, 0.30);
            FillRR(drawer, { 0xFF, 0xFF, 0xFF, ca(240.0) }, (s32)this->guide_tx, y + 44, (s32)this->guide_tw, 4, 2);
            Draw(drawer, this->guide_lr_tex, ix + iw - TexW(this->guide_lr_tex), y + 4, ca(110.0));
            y += 78;
        }

        // Rows: only the focused one opens up
        const s32 top = y;
        const s32 bot = py + ph - 96;
        const auto &rows = this->guide_tabs.at(this->guide_tab).rows;
        const auto nrows = (s32)this->guide_row_texs.size();
        if(nrows == 0) {
            SDL_RenderSetClipRect(sdl, nullptr);
            return;
        }
        this->guide_sel = std::clamp(this->guide_sel, 0, nrows - 1);
        std::vector<s32> heights(nrows);
        std::vector<s32> offs(nrows);
        s32 acc = 0;
        for(s32 i = 0; i < nrows; i++) {
            const auto &rt = this->guide_row_texs.at(i);
            const auto f = i == this->guide_sel;
            const auto nsub = f ? (s32)rt.sub.size() : std::min<s32>(1, (s32)rt.sub.size());
            s32 h = 74 + nsub * GuideLineH;
            if(f) {
                const auto rev = this->guide_rev.at(i);
                if(rev > 0) {
                    h += 14;
                    for(s32 k = 0; k < rev; k++) {
                        h += (s32)rt.hints.at(k).size() * GuideLineH + 16;
                    }
                }
                if(rev < (s32)rt.hints.size()) {
                    h += 44;
                }
            }
            heights[i] = h;
            offs[i] = acc;
            acc += h + 8;
        }
        const auto view = bot - top;
        const auto fy = offs[this->guide_sel];
        const auto fh = heights[this->guide_sel];
        double want = this->guide_scroll;
        if(fy - want < 0) {
            want = fy;
        }
        if(fy + fh - want > view) {
            want = fy + fh - view;
        }
        want = std::max(0.0, want);
        if(!this->guide_init) {
            this->guide_scroll = want;
        }
        this->guide_scroll = this->FxApproach(this->guide_scroll, want, 0.30);
        const auto ty = top + fy - this->guide_scroll;
        if(!this->guide_init) {
            this->guide_init = true;
            this->guide_ly = ty;
            this->guide_lh = fh;
        }
        this->guide_ly = this->FxApproach(this->guide_ly, ty, 0.30);
        this->guide_lh = this->FxApproach(this->guide_lh, fh, 0.30);

        const SDL_Rect list_clip = { px, top - 10, pw, view + 16 };
        SDL_RenderSetClipRect(sdl, &list_clip);
        RenderGlass(drawer, ix - 16, (s32)this->guide_ly, iw + 32, (s32)this->guide_lh, 30, a * grow, 100);
        FillRR(drawer, { 0xFF, 0xFF, 0xFF, ca(22.0) }, ix - 16, (s32)this->guide_ly, iw + 32, std::min(60, (s32)this->guide_lh / 2), 30);

        const auto pulse = 0.6 + 0.4 * std::sin(this->fx_time * 5.0);
        for(s32 i = 0; i < nrows; i++) {
            const auto &rt = this->guide_row_texs.at(i);
            const auto &row = rows.at(i);
            const auto f = i == this->guide_sel;
            const auto ry = top + offs[i] - (s32)this->guide_scroll;
            if((ry > bot + 20) || (ry + heights[i] < top - 20)) {
                continue;
            }
            const auto ta = f ? 255.0 : 150.0;
            s32 cx = ix + 8;
            if(row.mark == "done") {
                FillRR(drawer, { 0x5B, 0xE3, 0x8A, ca(f ? 60.0 : 40.0) }, ix, ry + 21, 34, 34, 17);
                FillRR(drawer, { 0x5B, 0xE3, 0x8A, ca(ta) }, ix + 9, ry + 30, 16, 16, 8);
                cx = ix + 50;
            }
            else if(row.mark == "here") {
                FillRR(drawer, { 0xE8, 0x41, 0x32, ca(255.0 * pulse) }, ix + 7, ry + 29, 18, 18, 9);
                cx = ix + 50;
            }
            s32 zone_w = 0;
            if(rt.zone != nullptr) {
                zone_w = std::min(260, TexW(rt.zone) + 30);
                FillRR(drawer, { 0xFF, 0xFF, 0xFF, ca(f ? 34.0 : 16.0) }, ix + iw - zone_w, ry + 18, zone_w, 40, 20);
                RenderTextFit(drawer, rt.zone, (double)(ix + iw - zone_w + 15), ry + 38 - TexH(rt.zone) / 2, (double)(zone_w - 30), ca(f ? 235.0 : 140.0), false);
            }
            RenderTextFit(drawer, rt.title, (double)cx, ry + 38 - TexH(rt.title) / 2, (double)(ix + iw - cx - zone_w - 20), ca(((row.mark == "done") && !f) ? 110.0 : ta), false);
            s32 yy = ry + 66;
            const auto nsub = f ? (s32)rt.sub.size() : std::min<s32>(1, (s32)rt.sub.size());
            for(s32 k = 0; k < nsub; k++) {
                Draw(drawer, rt.sub.at(k), cx, yy, ca(f ? 195.0 : 115.0));
                yy += GuideLineH;
            }
            if(f) {
                const auto rev = this->guide_rev.at(i);
                if(rev > 0) {
                    yy += 14;
                    for(s32 k = 0; k < rev; k++) {
                        const auto &lines = rt.hints.at(k);
                        FillRR(drawer, { 0xFF, 0xD6, 0x66, ca(200.0) }, cx, yy + 6, 4, (s32)lines.size() * GuideLineH - 12, 2);
                        for(auto l : lines) {
                            Draw(drawer, l, cx + 26, yy, ca(235.0));
                            yy += GuideLineH;
                        }
                        yy += 16;
                    }
                }
                const auto nh = (s32)rt.hints.size();
                if(rev < nh) {
                    const auto label = nxt::Sub(nxt::Sub(nxt::T(rev == 0 ? "g_hint" : "g_more"), "a", std::to_string(rev + 1)), "b", std::to_string(nh));
                    if(label != this->guide_more_str) {
                        this->guide_more_str = label;
                        Free(this->guide_more_tex);
                        this->guide_more_tex = Text(pu::ui::DefaultFontSize::Small, label);
                        SDL_SetTextureColorMod(this->guide_more_tex, 0xFF, 0xD6, 0x66);
                    }
                    Draw(drawer, this->guide_more_tex, cx, yy + 6, ca(220.0));
                }
            }
            this->sheet_hits.push_back({ 100 + i, ix - 16, ry, iw + 32, heights[i] });
        }
        SDL_RenderSetClipRect(sdl, &panel_clip);

        // One line: what the buttons do here
        {
            const auto &rt = this->guide_row_texs.at(this->guide_sel);
            const auto &row = rows.at(this->guide_sel);
            std::vector<std::string> parts;
            if(!rt.hints.empty()) {
                parts.push_back(nxt::T((this->guide_rev.at(this->guide_sel) >= (s32)rt.hints.size()) ? "g_k_hide" : "g_k_hint"));
            }
            if(row.manual) {
                parts.push_back(nxt::T((row.mark == "done") ? "g_k_unmark" : "g_k_mark"));
            }
            parts.push_back(nxt::T("g_k_move"));
            parts.push_back(nxt::T("g_k_section"));
            if(this->guide_has_save) {
                parts.push_back(nxt::T("g_k_update"));
            }
            parts.push_back(nxt::T("g_k_back"));
            std::string hint;
            for(const auto &p : parts) {
                hint += (hint.empty() ? "" : "      ") + p;
            }
            if(hint != this->sheet_hint_str) {
                this->sheet_hint_str = hint;
                Free(this->sheet_hint);
                this->sheet_hint = Text(pu::ui::DefaultFontSize::Small, hint);
            }
            RenderTextFit(drawer, this->sheet_hint, (double)ix, py + ph - 62, (double)iw, ca(120.0), false);
        }
        SDL_RenderSetClipRect(sdl, nullptr);
    }

    void EntryMenu::SheetOpen() {
        this->SheetFree();

        std::string title;
        std::string meta;
        this->DescribeSuspended(title, meta);
        this->FocusSuspendedEntry();

        if(title.empty()) {
            title = nxt::T("paused_game");
        }
        // The "Paused" tag is already the badge here
        const std::string paused_tag = "   ·   " + nxt::T("paused");
        if((meta.length() >= paused_tag.length()) && (meta.compare(meta.length() - paused_tag.length(), paused_tag.length(), paused_tag) == 0)) {
            meta.erase(meta.length() - paused_tag.length());
        }

        this->sheet_title = Text(pu::ui::DefaultFontSize::Large, title);
        if(!meta.empty()) {
            this->sheet_meta = Text(pu::ui::DefaultFontSize::Small, meta);
        }
        this->sheet_badge = Text(pu::ui::DefaultFontSize::Small, nxt::T("paused_badge"));

        // One quiet line about this game: today, this week, streak
        {
            const auto app_id = g_GlobalSettings.system_status.suspended_app_id;
            const auto stats = session::GetAppStats(app_id);
            u64 week = 0;
            for(const auto secs : stats.week) {
                week += secs;
            }
            std::string line = nxt::Sub(nxt::T("stat_today"), "t", nxt::Played(stats.week[6]));
            if(week > stats.week[6]) {
                line += "   ·   " + nxt::Sub(nxt::T("stat_week"), "t", nxt::Played(week));
            }
            if(stats.streak >= 2) {
                line += "   ·   " + nxt::Sub(nxt::T("stat_streak"), "n", std::to_string(stats.streak));
            }
            this->sheet_time = Text(pu::ui::DefaultFontSize::Small, line);
        }

        this->sheet_cont_tex = Text(pu::ui::DefaultFontSize::MediumLarge, nxt::T("continue"));
        this->sheet_other_label = Text(pu::ui::DefaultFontSize::Small, nxt::T("other"));
        if(!this->sheet_icons_loaded) {
            this->sheet_icons_loaded = true;
            static const char *const Names[6] = { "Play", "Album", "Library", "Headphones", "Power", "Bluetooth" };
            for(u32 i = 0; i < 6; i++) {
                this->sheet_icons[i] = TryFindLoadImage(std::string("ui/Main/NxDash/") + Names[i]);
            }
        }

        // Play something else: the last games, minus the one that is paused
        this->sheet_other_ids.clear();
        this->sheet_other_icons.clear();
        this->sheet_other_names.clear();
        for(const auto app_id : session::Recents()) {
            if((app_id == 0) || (app_id == g_GlobalSettings.system_status.suspended_app_id)) {
                continue;
            }
            Entry probe = {};
            probe.type = EntryType::Application;
            probe.app_info.app_id = app_id;
            probe.app_info.record.id = app_id;
            std::string name;
            std::string unused;
            ShelfDescribe(probe, name, unused);
            this->sheet_other_ids.push_back(app_id);
            this->sheet_other_icons.push_back(LoadApplicationIconTexture(app_id));
            this->sheet_other_names.push_back(name);
            if(this->sheet_other_ids.size() >= 3) {
                break;
            }
        }

        this->sheet_bt_poll = 1.0;
        this->SheetRefreshBluetooth();
        this->sheet_bt_open = false;
        this->GuideLoad(g_GlobalSettings.system_status.suspended_app_id);
        this->sheet_guide_open = false;

        this->sheet_zone = 0;
        this->sheet_icon_sel = 0;
        this->sheet_other_sel = 0;
        this->sheet_lens_init = false;
        this->sheet_hint_str.clear();
        this->sheet_play_other = 0;
        this->sheet_intro_dim = true;
        this->sheet_anim = 0.0;
        this->sheet_open = true;
    }

    void EntryMenu::SheetRefreshBluetooth() {
        this->sheet_bt_poll += this->fx_dt;
        if(this->sheet_bt_poll < 0.25) {
            return;
        }
        this->sheet_bt_poll = 0.0;
        const auto dev = bt::GetConnectedAudioDevice();
        this->sheet_bt_connected = dev.name[0] != '\0';
        if(this->sheet_bt_connected) {
            this->sheet_bt_name = std::string(dev.name, strnlen(dev.name, sizeof(dev.name)));
            this->sheet_bt_paired = true;
        }
        else {
            const auto paired = bt::ListPairedAudioDevices();
            this->sheet_bt_paired = !paired.empty();
            this->sheet_bt_name = paired.empty() ? nxt::T("headphones") : std::string(paired.front().name, strnlen(paired.front().name, sizeof(paired.front().name)));
        }
    }

    void EntryMenu::SheetExecute(const u32 action) {
        const auto act = static_cast<SheetAction>(action);
        switch(act) {
            case SheetAction::Resume: {
                this->CloseSheetForResume();
                if(this->sheet_action_cb) {
                    this->sheet_action_cb(act);
                }
                break;
            }
            case SheetAction::Close: {
                if(this->sheet_action_cb) {
                    this->sheet_action_cb(act);
                }
                if(!g_GlobalSettings.IsSuspended()) {
                    this->sheet_open = false;
                    this->ShelfClearInfo();
                }
                break;
            }
            case SheetAction::Album: {
                if(this->sheet_action_cb) {
                    this->sheet_action_cb(act);
                }
                break;
            }
            case SheetAction::Library: {
                this->sheet_open = false;
                break;
            }
            case SheetAction::PlayOther:
            case SheetAction::BluetoothPair: {
                if(this->sheet_action_cb) {
                    this->sheet_action_cb(act);
                }
                break;
            }
        }
    }

    void EntryMenu::SheetActivate() {
        this->NxSfx(2);
        if(this->sheet_zone == 0) {
            this->SheetExecute(static_cast<u32>(SheetAction::Resume));
        }
        else if(this->sheet_zone == 1) {
            switch(this->SheetIconId(this->sheet_icon_sel)) {
                case 0: {
                    this->SheetExecute(static_cast<u32>(SheetAction::Album));
                    break;
                }
                case 1: {
                    this->SheetExecute(static_cast<u32>(SheetAction::Library));
                    break;
                }
                case 2: {
                    this->GuideOpen();
                    break;
                }
                case 3: {
                    this->SheetBtOpen();
                    break;
                }
                default: {
                    this->SheetExecute(static_cast<u32>(SheetAction::Close));
                    break;
                }
            }
        }
        else if((this->sheet_zone == 2) && (this->sheet_other_sel < (s32)this->sheet_other_ids.size())) {
            this->sheet_play_other = this->sheet_other_ids.at(this->sheet_other_sel);
            this->SheetExecute(static_cast<u32>(SheetAction::PlayOther));
        }
    }

    void EntryMenu::SheetInput(const u64 keys_down, const pu::ui::TouchPoint touch_pos) {
        if(this->sheet_guide_open && touch_pos.IsEmpty() && !this->sheet_touching) {
            this->GuideInput(keys_down);
            return;
        }
        if(this->sheet_bt_open && touch_pos.IsEmpty() && !this->sheet_touching) {
            this->SheetBtInput(keys_down);
            return;
        }
        if(!touch_pos.IsEmpty()) {
            this->sheet_touching = true;
            this->sheet_touch_x = touch_pos.x;
            this->sheet_touch_y = touch_pos.y;
            return;
        }
        if(this->sheet_touching) {
            this->sheet_touching = false;
            for(const auto &hit : this->sheet_hits) {
                if((this->sheet_touch_x >= hit.x) && (this->sheet_touch_x < hit.x + hit.w) && (this->sheet_touch_y >= hit.y) && (this->sheet_touch_y < hit.y + hit.h)) {
                    if(this->sheet_guide_open) {
                        if((hit.pos >= 70) && (hit.pos < 100)) {
                            this->GuideSetTab(hit.pos - 70);
                            this->NxSfx(0);
                        }
                        else if(hit.pos >= 100) {
                            if(hit.pos - 100 == this->guide_sel) {
                                this->GuideActivate();
                            }
                            else {
                                this->guide_sel = hit.pos - 100;
                                this->NxSfx(0);
                            }
                        }
                        else if(hit.pos == -1) {
                            // Outside the panel: back to the pause panel
                            const s32 px_final = pu::ui::render::ScreenWidth - (s32)this->sheet_pw - 28;
                            if(this->sheet_touch_x < px_final) {
                                this->sheet_guide_open = false;
                                this->sheet_hint_str.clear();
                            }
                        }
                        return;
                    }
                    if((hit.pos >= 30) && (hit.pos < 40)) {
                        const auto row = hit.pos - 30;
                        if(row == this->sheet_bt_sel) {
                            this->SheetBtActivate();
                        }
                        else {
                            this->sheet_bt_sel = row;
                            this->NxSfx(0);
                        }
                    }
                    else if(this->sheet_bt_open) {
                        // Taps on the rest of the panel do nothing while the Bluetooth list is up
                    }
                    else if(hit.pos == 0) {
                        this->sheet_zone = 0;
                        this->SheetActivate();
                    }
                    else if((hit.pos >= 10) && (hit.pos < 20)) {
                        this->sheet_zone = 1;
                        this->sheet_icon_sel = hit.pos - 10;
                        this->SheetActivate();
                    }
                    else if(hit.pos >= 20) {
                        this->sheet_zone = 2;
                        this->sheet_other_sel = hit.pos - 20;
                        this->SheetActivate();
                    }
                    return;
                }
            }
            // Tapped outside the panel: back to the library
            this->sheet_open = false;
            return;
        }

        const auto zones = this->sheet_other_ids.empty() ? 2 : 3;
        const auto prev = this->sheet_zone * 100 + this->sheet_icon_sel * 10 + this->sheet_other_sel;
        if(keys_down & HidNpadButton_AnyUp) {
            this->sheet_zone = std::max(0, this->sheet_zone - 1);
        }
        else if(keys_down & HidNpadButton_AnyDown) {
            this->sheet_zone = std::min(zones - 1, this->sheet_zone + 1);
        }
        else if(keys_down & HidNpadButton_AnyLeft) {
            if(this->sheet_zone == 1) {
                this->sheet_icon_sel = std::max(0, this->sheet_icon_sel - 1);
            }
            else if(this->sheet_zone == 2) {
                this->sheet_other_sel = std::max(0, this->sheet_other_sel - 1);
            }
        }
        else if(keys_down & HidNpadButton_AnyRight) {
            if(this->sheet_zone == 1) {
                this->sheet_icon_sel = std::min(this->SheetIconCount() - 1, this->sheet_icon_sel + 1);
            }
            else if(this->sheet_zone == 2) {
                this->sheet_other_sel = std::min((s32)this->sheet_other_ids.size() - 1, this->sheet_other_sel + 1);
            }
        }
        else if(keys_down & HidNpadButton_A) {
            this->SheetActivate();
            return;
        }
        else if(keys_down & HidNpadButton_B) {
            this->SheetExecute(static_cast<u32>(SheetAction::Library));
            return;
        }
        const auto now = this->sheet_zone * 100 + this->sheet_icon_sel * 10 + this->sheet_other_sel;
        if(now != prev) {
            this->NxSfx(0);
            this->Haptic(0.18f, 160.0f, 0.015);
        }
        else if(keys_down & (HidNpadButton_AnyUp | HidNpadButton_AnyDown | HidNpadButton_AnyLeft | HidNpadButton_AnyRight)) {
            // Edge of the panel: a soft bump
            this->NxSfx(1);
            this->Haptic(0.3f, 120.0f, 0.03);
        }
    }

    void EntryMenu::RenderOverlay(pu::ui::render::Renderer::Ref &drawer) {
        if(!ShelfMode) {
            return;
        }

        nxdash::g_Modal = this->IsModalOpen();
        this->HapticUpdate();
        this->BtUpdate();

        // Coming back from a game with HOME: show the game sheet first
        if(!this->sheet_checked) {
            this->sheet_checked = true;
            if(g_GlobalSettings.IsSuspended()) {
                // Time in the menu does not count
                session::Pause();
            }
            if(g_GlobalSettings.IsSuspended() && HasScreenCaptureBackground()) {
                this->SheetOpen();
            }
        }

        const auto target = this->sheet_open ? 1.0 : 0.0;
        this->sheet_anim = this->FxApproach(this->sheet_anim, target, this->sheet_open ? 0.18 : 0.26);
        if(std::abs(target - this->sheet_anim) < 0.003) {
            this->sheet_anim = target;
        }
        if(!this->sheet_open && (this->sheet_anim <= 0.0)) {
            if(this->sheet_title != nullptr) {
                this->SheetFree();
            }
            this->sheet_hits.clear();
            return;
        }
        EnsureShapes();
        if(this->sheet_open) {
            this->SheetRefreshBluetooth();
        }

        const auto a = SmoothStep(this->sheet_anim);
        const auto alpha = [&](const double v) {
            return (u8)std::clamp(v * a, 0.0, 255.0);
        };
        // Panel contents arrive a beat after the glass
        const auto ca = [&](const double v) {
            return (u8)std::clamp(v * a * std::clamp(this->sheet_anim * 1.7 - 0.5, 0.0, 1.0), 0.0, 255.0);
        };
        const s32 sw = pu::ui::render::ScreenWidth;
        const s32 sh = pu::ui::render::ScreenHeight;

        // The library stays behind, dimmed: you paused, you didn't leave
        drawer->RenderRectangleFill({ 0x06, 0x07, 0x0B, alpha(196.0) }, 0, 0, sw, sh);

        this->sheet_pw = this->FxApproach(this->sheet_pw, this->sheet_guide_open ? (double)GuidePanelW : 640.0, 0.22);
        if(std::abs(this->sheet_pw - (this->sheet_guide_open ? (double)GuidePanelW : 640.0)) < 0.5) {
            this->sheet_pw = this->sheet_guide_open ? (double)GuidePanelW : 640.0;
        }
        const s32 pw = (s32)this->sheet_pw;
        const s32 px_final = sw - pw - 28;
        const s32 px = px_final + (s32)((1.0 - a) * (pw + 80));
        const s32 py = 28;
        const s32 ph = sh - 56;
        const s32 pad = 48;
        const s32 ix = px + pad;
        const s32 iw = pw - pad * 2;

        // Game capture: zooms from full screen into the space the panel leaves
        {
            const double tw = std::min(1040.0, (double)px_final - 60.0);
            const double th = tw * 9.0 / 16.0;
            const double tx = ((double)px_final - tw) / 2.0;
            const double ty = ((double)sh - th) / 2.0;
            const auto rx = tx * a;
            const auto ry = ty * a;
            const auto rw = sw + (tw - sw) * a;
            const auto rh = sh + (th - sh) * a;
            if(g_Glow != nullptr) {
                drawer->RenderTexture(g_Glow, (s32)(rx - 60.0), (s32)(ry - 40.0), pu::ui::render::TextureRenderOptions(alpha(90.0), (s32)(rw + 120.0), (s32)(rh + 120.0), {}, {}, {}));
            }
            auto capture = GetScreenCaptureBackground()->GetTexture();
            if(capture != nullptr) {
                drawer->RenderTexture(capture, (s32)rx, (s32)ry, pu::ui::render::TextureRenderOptions(0xFF, (s32)rw, (s32)rh, {}, {}, {}));
            }
            else {
                drawer->RenderRectangleFill({ 0x1C, 0x1E, 0x28, 0xFF }, (s32)rx, (s32)ry, (s32)rw, (s32)rh);
            }
            if(this->sheet_intro_dim && this->sheet_open) {
                drawer->RenderRectangleFill({ 0x06, 0x07, 0x0B, (u8)(110.0 * (1.0 - a)) }, (s32)rx, (s32)ry, (s32)rw, (s32)rh);
            }
            else {
                this->sheet_intro_dim = false;
            }
        }

        // One sheet of glass, full height
        FillRR(drawer, { 0x10, 0x12, 0x1A, alpha(215.0) }, px, py, pw, ph, 40);
        RenderGlass(drawer, px, py, pw, ph, 40, a, 90);

        this->sheet_hits.clear();
        if(this->sheet_guide_open || (this->sheet_pw > 660.0)) {
            if(this->sheet_guide_open) {
                this->SheetRenderGuide(drawer, px, py, pw, ph, a, ca);
            }
            this->sheet_hits.push_back({ -1, 0, 0, px_final + pw, sh });
            return;
        }
        s32 y = py + 52;
        {
            const auto bw = TexW(this->sheet_badge);
            const auto bh = TexH(this->sheet_badge);
            FillRR(drawer, { 0xE8, 0x41, 0x32, ca(235.0) }, ix, y, bw + 28, bh + 10, (bh + 10) / 2);
            Draw(drawer, this->sheet_badge, ix + 14, y + 5, ca(255.0));
            y += bh + 26;
        }
        RenderTextFit(drawer, this->sheet_title, (double)ix, y, (double)iw, ca(255.0), false);
        y += std::max(56, TexH(this->sheet_title) + 6);
        if(this->sheet_meta != nullptr) {
            RenderTextFit(drawer, this->sheet_meta, (double)ix, y, (double)iw, ca(150.0), false);
            y += TexH(this->sheet_meta) + 18;
        }
        RenderTextFit(drawer, this->sheet_time, (double)ix, y, (double)iw, ca(185.0), false);
        y += TexH(this->sheet_time) + 44;

        if(this->sheet_bt_open) {
            this->SheetRenderBluetooth(drawer, ix, iw, y, py + ph - 62, a, ca);
        }
        else {
            // Focus targets first: the lens slides between them
            const s32 cont_y = y;
            const s32 cont_h = 78;
            y += 108;
            const s32 isz = 92;
            const s32 icount = this->SheetIconCount();
            const s32 igap = (iw - isz * icount) / (icount - 1);
            const s32 icon_cy = y + isz / 2;
            y += 160;
            const s32 other_label_y = y;
            const s32 card = 150;
            const s32 card_gap = 22;
            const s32 cards_y = y + 44;

            double tx;
            double ty;
            double tw;
            double th;
            if(this->sheet_zone == 0) {
                tx = ix - 12;
                ty = cont_y;
                tw = iw + 24;
                th = cont_h;
            }
            else if(this->sheet_zone == 1) {
                tx = ix + this->sheet_icon_sel * (isz + igap);
                ty = icon_cy - isz / 2 - 6;
                tw = isz;
                th = isz;
            }
            else {
                tx = ix + this->sheet_other_sel * (card + card_gap);
                ty = cards_y;
                tw = card;
                th = card;
            }
            if(!this->sheet_lens_init) {
                this->sheet_lens_init = true;
                this->sheet_lx = tx;
                this->sheet_ly = ty;
                this->sheet_lw = tw;
                this->sheet_lh = th;
            }
            this->sheet_lx = this->FxApproach(this->sheet_lx, tx, 0.30);
            this->sheet_ly = this->FxApproach(this->sheet_ly, ty, 0.30);
            this->sheet_lw = this->FxApproach(this->sheet_lw, tw, 0.30);
            this->sheet_lh = this->FxApproach(this->sheet_lh, th, 0.30);
            if(this->sheet_zone != 2) {
                const auto r = (s32)std::min(this->sheet_lh / 2.0, (this->sheet_zone == 1) ? 46.0 : 34.0);
                if((this->sheet_zone == 1) && (g_Glow != nullptr)) {
                    drawer->RenderTexture(g_Glow, (s32)this->sheet_lx - 40, (s32)this->sheet_ly - 40, pu::ui::render::TextureRenderOptions(ca(110.0), (s32)this->sheet_lw + 80, (s32)this->sheet_lh + 80, {}, {}, {}));
                }
                RenderGlass(drawer, (s32)this->sheet_lx, (s32)this->sheet_ly, (s32)this->sheet_lw, (s32)this->sheet_lh, r, a, (this->sheet_zone == 1) ? 160 : 100);
                FillRR(drawer, { 0xFF, 0xFF, 0xFF, ca(24.0) }, (s32)this->sheet_lx, (s32)this->sheet_ly, (s32)this->sheet_lw, (s32)(this->sheet_lh / 2.0), r);
            }

            // Continue: the only action spelled out
            {
                const auto f = this->sheet_zone == 0;
                const auto ta = f ? 255.0 : 150.0;
                if(this->sheet_icons[0] != nullptr) {
                    drawer->RenderTexture(this->sheet_icons[0], ix + 8, cont_y + cont_h / 2 - 18, pu::ui::render::TextureRenderOptions(ca(ta), 36, 36, {}, {}, {}));
                }
                Draw(drawer, this->sheet_cont_tex, ix + 64, cont_y + (cont_h - TexH(this->sheet_cont_tex)) / 2, ca(ta));
                this->sheet_hits.push_back({ 0, ix - 12, cont_y, iw + 24, cont_h });
            }

            // Icons: the name shows only under the focused one
            for(s32 i = 0; i < icount; i++) {
                const auto f = (this->sheet_zone == 1) && (this->sheet_icon_sel == i);
                const auto id = this->SheetIconId(i);
                const auto cx = ix + i * (isz + igap) + isz / 2;
                const auto cy = icon_cy - (f ? 6 : 0);
                if(!f) {
                    FillRR(drawer, { 0xFF, 0xFF, 0xFF, ca(16.0) }, cx - isz / 2, icon_cy - isz / 2, isz, isz, isz / 2);
                }
                static const s32 IconMap[5] = { 1, 2, -1, 5, 4 };
                auto icon = (id == 2) ? this->guide_icon : this->sheet_icons[IconMap[id]];
                const auto danger = (id == 4) && f;
                if(icon != nullptr) {
                    const auto isize = f ? 52 : 46;
                    if(danger) {
                        SDL_SetTextureColorMod(icon, 0xFF, 0x6B, 0x5E);
                    }
                    drawer->RenderTexture(icon, cx - isize / 2, cy - isize / 2, pu::ui::render::TextureRenderOptions(ca(f ? 255.0 : 130.0), isize, isize, {}, {}, {}));
                    if(danger) {
                        SDL_SetTextureColorMod(icon, 0xFF, 0xFF, 0xFF);
                    }
                }
                if((id == 3) && this->sheet_bt_connected) {
                    FillRR(drawer, { 0x5B, 0xE3, 0x8A, ca(255.0) }, cx + 24, cy - 32, 14, 14, 7);
                }
                if((id == 3) && this->bt_pending) {
                    // Working on it: a soft breathing ring
                    const auto p = 0.5 + 0.5 * std::sin(this->fx_time * 6.0);
                    FillRR(drawer, { 0xFF, 0xFF, 0xFF, ca(40.0 + 60.0 * p) }, cx - isz / 2 - 6, icon_cy - isz / 2 - 6, isz + 12, isz + 12, isz / 2 + 6);
                }
                if(f) {
                    std::string label;
                    switch(id) {
                        case 0: label = nxt::T("album"); break;
                        case 1: label = nxt::T("library"); break;
                        case 2: label = nxt::T("guide"); break;
                        case 3: label = this->sheet_bt_connected ? this->sheet_bt_name : nxt::T("bt"); break;
                        default: label = nxt::T("close"); break;
                    }
                    if(label != this->sheet_icon_label_str) {
                        this->sheet_icon_label_str = label;
                        Free(this->sheet_icon_label);
                        this->sheet_icon_label = Text(pu::ui::DefaultFontSize::Small, label);
                    }
                    if(this->sheet_icon_label != nullptr) {
                        const auto lw = std::min(TexW(this->sheet_icon_label), iw);
                        const auto lx = std::clamp(cx - lw / 2, ix, ix + iw - lw);
                        if(danger) {
                            SDL_SetTextureColorMod(this->sheet_icon_label, 0xFF, 0x6B, 0x5E);
                        }
                        RenderTextFit(drawer, this->sheet_icon_label, (double)lx, icon_cy + isz / 2 + 14, (double)iw, ca(240.0), false);
                        if(danger) {
                            SDL_SetTextureColorMod(this->sheet_icon_label, 0xFF, 0xFF, 0xFF);
                        }
                    }
                }
                this->sheet_hits.push_back({ 10 + i, cx - isz / 2, icon_cy - isz / 2, isz, isz });
            }

            // Play something else
            if(!this->sheet_other_ids.empty()) {
                Draw(drawer, this->sheet_other_label, ix, other_label_y, ca(150.0));
                for(s32 i = 0; i < (s32)this->sheet_other_ids.size(); i++) {
                    const auto f = (this->sheet_zone == 2) && (this->sheet_other_sel == i);
                    const auto grow = f ? 8 : 0;
                    const auto x = ix + i * (card + card_gap) - grow / 2;
                    const auto yy = cards_y - grow / 2 - (f ? 4 : 0);
                    const auto s = card + grow;
                    if(f) {
                        FillRR(drawer, { 0xFF, 0xFF, 0xFF, ca(235.0) }, x - 5, yy - 5, s + 10, s + 10, 24);
                    }
                    auto icon = this->sheet_other_icons.at(i);
                    if((icon != nullptr) && (icon->Get() != nullptr)) {
                        drawer->RenderTexture(icon->Get(), x, yy, pu::ui::render::TextureRenderOptions(ca(f ? 255.0 : 150.0), s, s, {}, {}, {}));
                    }
                    else {
                        FillRR(drawer, { 0x2A, 0x2D, 0x38, ca(255.0) }, x, yy, s, s, 18);
                    }
                    this->sheet_hits.push_back({ 20 + i, ix + i * (card + card_gap), cards_y, card, card });
                }
            }

            // One contextual hint
            {
                std::string hint;
                if(this->sheet_zone == 0) {
                    hint = nxt::T("h_continue");
                }
                else if(this->sheet_zone == 1) {
                    switch(this->SheetIconId(this->sheet_icon_sel)) {
                        case 0: hint = nxt::T("h_album"); break;
                        case 1: hint = nxt::T("h_library"); break;
                        case 2: hint = nxt::T("h_guide_open"); break;
                        case 3: hint = this->sheet_bt_connected ? nxt::Sub(nxt::T("h_bt_open_on"), "name", this->sheet_bt_name) : nxt::T("h_bt_open"); break;
                        default: hint = nxt::T("h_close"); break;
                    }
                }
                else if(this->sheet_other_sel < (s32)this->sheet_other_names.size()) {
                    hint = nxt::Sub(nxt::T("h_other"), "name", this->sheet_other_names.at(this->sheet_other_sel));
                }
                if(hint != this->sheet_hint_str) {
                    this->sheet_hint_str = hint;
                    Free(this->sheet_hint);
                    this->sheet_hint = Text(pu::ui::DefaultFontSize::Small, hint);
                }
                RenderTextFit(drawer, this->sheet_hint, (double)ix, py + ph - 62, (double)iw, ca(125.0), false);
            }
        }

        // Anywhere on the panel or the capture counts as "inside"
        this->sheet_hits.push_back({ -1, 0, 0, px_final + pw, sh });
    }

    // ---- Ambient light: the focused cover, blurred down to a wash of color behind the glass ----

    void EntryMenu::RenderBackdrop(pu::ui::render::Renderer::Ref &drawer) {
        {
            const auto now = armGetSystemTick();
            if(this->fx_last_tick != 0) {
                this->fx_dt = std::clamp((double)armTicksToNs(now - this->fx_last_tick) / 1e9, 1.0 / 240.0, 1.0 / 12.0);
            }
            this->fx_last_tick = now;
            this->fx_k = this->fx_dt * 60.0;
            this->fx_time += this->fx_dt;
        }
        if(!ShelfMode || this->amb_failed) {
            return;
        }

        auto renderer = pu::ui::render::GetMainRenderer();
        const auto has_focus = !this->shelf_valid.empty() && (this->cur_entry_idx < this->entry_icons.size());
        if(has_focus && (((s32)this->cur_entry_idx != this->amb_idx) || (this->cur_path != this->amb_path))) {
            auto icon = this->entry_icons.at(this->cur_entry_idx);
            if((icon != nullptr) && (icon->Get() != nullptr)) {
                // 256px icon -> 32x18 -> 8x5: the two downscales are the blur, the upscale smooths it into a wash of color
                auto mid = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 32, 18);
                auto tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 8, 5);
                if((mid == nullptr) || (tex == nullptr) || (SDL_SetRenderTarget(renderer, mid) != 0)) {
                    if(mid != nullptr) {
                        SDL_DestroyTexture(mid);
                    }
                    if(tex != nullptr) {
                        SDL_DestroyTexture(tex);
                    }
                    SDL_SetRenderTarget(renderer, nullptr);
                    this->amb_failed = true;
                    return;
                }
                SDL_SetTextureAlphaMod(icon->Get(), 0xFF);
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF);
                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, icon->Get(), nullptr, nullptr);
                SDL_SetRenderTarget(renderer, tex);
                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, mid, nullptr, nullptr);
                SDL_SetRenderTarget(renderer, nullptr);
                SDL_DestroyTexture(mid);
                SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

                if(this->amb_prev != nullptr) {
                    SDL_DestroyTexture(this->amb_prev);
                }
                this->amb_prev = this->amb_cur;
                this->amb_cur = tex;
                this->amb_fade = 0.0;
                this->amb_idx = this->cur_entry_idx;
                this->amb_path = this->cur_path;
            }
        }

        this->amb_fade = std::min(1.0, this->amb_fade + this->fx_dt / 0.37);
        const auto f = SmoothStep(this->amb_fade);
        const double strength = 100.0;
        const s32 sw = pu::ui::render::ScreenWidth;
        const s32 sh = pu::ui::render::ScreenHeight;
        // Oversized so the soft edges of the tiny texture stay off screen
        const s32 ox = -sw / 8;
        const s32 oy = -sh / 8;
        const s32 ow = sw + sw / 4;
        const s32 oh = sh + sh / 4;
        if((this->amb_prev != nullptr) && (f < 1.0)) {
            drawer->RenderTexture(this->amb_prev, ox, oy, pu::ui::render::TextureRenderOptions((u8)(strength * (1.0 - f)), ow, oh, {}, {}, {}));
        }
        if(this->amb_cur != nullptr) {
            drawer->RenderTexture(this->amb_cur, ox, oy, pu::ui::render::TextureRenderOptions((u8)(strength * f), ow, oh, {}, {}, {}));
        }
        // Keep the top and bottom calm so the bars and text stay readable
        EnsureShapes();
        if(g_FadeDown != nullptr) {
            drawer->RenderTexture(g_FadeDown, 0, 0, pu::ui::render::TextureRenderOptions(0x8C, sw, 300, {}, {}, {}));
        }
        if(g_FadeUp != nullptr) {
            drawer->RenderTexture(g_FadeUp, 0, sh - 420, pu::ui::render::TextureRenderOptions(0xB4, sw, 420, {}, {}, {}));
        }
    }

    // ---- Launch transition: the cover flies into the screen, light passes over it, everything ends on black ----

    void EntryMenu::RenderLaunch(pu::ui::render::Renderer::Ref &drawer) {
        if(this->launch_t < 0.0) {
            return;
        }
        if(this->launch_t < 2.0) {
            this->launch_t += this->fx_dt / 0.95;
        }
        const auto t = this->launch_t;
        const s32 sw = pu::ui::render::ScreenWidth;
        const s32 sh = pu::ui::render::ScreenHeight;

        // Rumble builds with the dive and cuts as the screen goes black
        if(t < 0.88) {
            this->Haptic(0.12f + 0.62f * (float)t, 150.0f, 0.03);
        }

        // The library falls away
        drawer->RenderRectangleFill({ 0x00, 0x00, 0x00, (u8)(255.0 * SmoothStep(t / 0.30)) }, 0, 0, sw, sh);

        if((this->launch_icon != nullptr) && (this->launch_icon->Get() != nullptr) && (this->launch_s > 1.0)) {
            const auto c = std::clamp(t / 0.62, 0.0, 1.0);
            // Slow start, hard acceleration: reads as diving into the cover
            const auto p = c * c * c * (c * (6.0 * c - 15.0) + 10.0);
            const auto size = this->launch_s + ((double)sw * 1.10 - this->launch_s) * p;
            const auto px = this->launch_cx + ((double)sw / 2.0 - this->launch_cx) * p;
            const auto py = this->launch_cy + ((double)sh / 2.0 - this->launch_cy) * p;
            drawer->RenderTexture(this->launch_icon->Get(), (s32)(px - size / 2.0), (s32)(py - size / 2.0), pu::ui::render::TextureRenderOptions(0xFF, (s32)size, (s32)size, {}, {}, {}));
            drawer->RenderRectangleFill({ 0x00, 0x00, 0x00, (u8)(70.0 * p) }, 0, 0, sw, sh);
        }

        // A band of light crosses the screen while the cover grows
        EnsureShapes();
        if((g_Sheen != nullptr) && (t > 0.16) && (t < 0.70)) {
            const auto q = (t - 0.16) / 0.54;
            const auto band_w = (double)sw * 0.55;
            const auto band_h = (double)sh * 1.7;
            const auto bx = -band_w + ((double)sw + band_w) * SmoothStep(q);
            drawer->RenderTexture(g_Sheen, (s32)bx, (s32)(((double)sh - band_h) / 2.0), pu::ui::render::TextureRenderOptions((u8)(120.0 * std::sin(M_PI * q)), (s32)band_w, (s32)band_h, 18.0f, {}, {}));
        }

        // ...and out to black, which is where the game picks up
        drawer->RenderRectangleFill({ 0x00, 0x00, 0x00, (u8)(255.0 * SmoothStep((t - 0.58) / 0.34)) }, 0, 0, sw, sh);
    }

    // =====================================================================================================
    // NX Dash: fast HOME, rumble, idle drift, lock screen, back of the case, duel, easter eggs
    // =====================================================================================================

    namespace {

        inline bool Hit(const s32 px, const s32 py, const s32 x, const s32 y, const s32 w, const s32 h) {
            return (px >= x) && (px < x + w) && (py >= y) && (py < y + h);
        }

        std::string FormatPlayed(const u64 seconds) {
            return nxt::Played(seconds);
        }

    }

    // ---- Fast HOME ----

    void EntryMenu::DescribeSuspended(std::string &out_title, std::string &out_meta) {
        for(auto &entry : this->cur_entries) {
            if((entry.Is<EntryType::Application>() || entry.Is<EntryType::Homebrew>()) && g_GlobalSettings.IsEntrySuspended(entry)) {
                ShelfDescribe(entry, out_title, out_meta);
                return;
            }
        }

        if(this->defer_pending) {
            // The entry list is not in yet: ask for the paused game's own title data
            Entry probe = {};
            probe.type = EntryType::Application;
            probe.app_info.app_id = g_GlobalSettings.system_status.suspended_app_id;
            probe.app_info.record.id = probe.app_info.app_id;
            ShelfDescribe(probe, out_title, out_meta);
            if(out_title == nxt::T("untitled")) {
                out_title.clear();
            }
            return;
        }

        // Not on this shelf: look in the root and in every category
        const auto &root = GetActiveMenuPath();
        auto search = [&](std::vector<Entry> entries) {
            for(auto &entry : entries) {
                if((entry.Is<EntryType::Application>() || entry.Is<EntryType::Homebrew>()) && g_GlobalSettings.IsEntrySuspended(entry)) {
                    ShelfDescribe(entry, out_title, out_meta);
                    return true;
                }
            }
            return false;
        };
        auto root_entries = LoadEntries(root);
        if(search(root_entries)) {
            return;
        }
        for(const auto &entry : root_entries) {
            if(entry.Is<EntryType::Folder>() && search(LoadEntries(entry.GetFolderPath()))) {
                return;
            }
        }
    }

    void EntryMenu::FocusSuspendedEntry() {
        for(u32 i = 0; i < this->cur_entries.size(); i++) {
            const auto &entry = this->cur_entries.at(i);
            if((entry.Is<EntryType::Application>() || entry.Is<EntryType::Homebrew>()) && g_GlobalSettings.IsEntrySuspended(entry)) {
                if(this->IsInRoot() && !this->ShelfEntryMatches(entry)) {
                    // Not on the shelf being shown: switch to the one that has it
                    this->shelf_root_filter = entry.Is<EntryType::Homebrew>() ? ShelfFilter::Homebrew : ShelfFilter::Games;
                    this->shelf_cats_sig.clear();
                }
                if(i != this->cur_entry_idx) {
                    const auto prev_idx = (s32)this->cur_entry_idx;
                    this->SetFocusedEntryIndex(i);
                    this->NotifyFocusedEntryChanged(prev_idx);
                }
                this->shelf_synced = false;
                return;
            }
        }
    }

    void EntryMenu::FocusAppEntry(const u64 app_id) {
        this->DeferredLoadNow();
        for(u32 i = 0; i < this->cur_entries.size(); i++) {
            const auto &entry = this->cur_entries.at(i);
            if(entry.Is<EntryType::Application>() && (entry.app_info.app_id == app_id)) {
                if(this->IsInRoot() && !this->ShelfEntryMatches(entry)) {
                    this->shelf_root_filter = ShelfFilter::Games;
                    this->shelf_cats_sig.clear();
                }
                if(i != this->cur_entry_idx) {
                    const auto prev_idx = (s32)this->cur_entry_idx;
                    this->SetFocusedEntryIndex(i);
                    this->NotifyFocusedEntryChanged(prev_idx);
                }
                this->shelf_synced = false;
                return;
            }
        }
    }

    void EntryMenu::DeferredLoadNow() {
        if(!this->defer_pending) {
            return;
        }
        this->defer_pending = false;
        const auto start = armGetSystemTick();
        this->entry_idx_stack.push(this->defer_last_idx);
        this->MoveTo(this->defer_path, true);
        this->FocusSuspendedEntry();
        this->ShelfClearInfo();
        this->shelf_intro = 0.0;
        this->shelf_intro_dir = 1.0;
        UL_LOG_INFO("EntryMenu deferred load took %d ms", (int)(armTicksToNs(armGetSystemTick() - start) / 1000000));
    }

    void EntryMenu::RenderSkeleton(pu::ui::render::Renderer::Ref &drawer, const s32 x, const s32 y) {
        EnsureShapes();
        const double area_w = (double)this->GetWidth();
        const double area_h = (double)this->GetHeight();
        const double H = area_h * 0.62;
        const double d = H * (474.0 / 760.0);
        const double ws = H * 0.085;
        const double gap = H * 0.014;
        const double cx = (double)x + area_w / 2.0;
        const double cy = (double)y + area_h * 0.355;
        // A soft pulse travelling outwards from the middle
        const auto pulse = [&](const double dist) {
            return 0.5 + 0.5 * std::sin(this->fx_time * 4.2 - dist * 0.55);
        };
        FillRR(drawer, { 0xFF, 0xFF, 0xFF, (u8)(26.0 + 22.0 * pulse(0.0)) }, (s32)(cx - d / 2.0), (s32)(cy - H / 2.0), (s32)d, (s32)H, 8);
        for(s32 side = -1; side <= 1; side += 2) {
            auto px = cx + side * (d / 2.0 + 1.4 * gap);
            for(s32 i = 1; i <= 16; i++) {
                const auto sc = 1.0 / (1.0 + std::pow((double)i * (ws + gap) / (H * 1.3), 1.6));
                const auto w = ws * sc;
                const auto h = H * sc;
                const auto left = (side > 0) ? px : (px - w);
                FillRR(drawer, { 0xFF, 0xFF, 0xFF, (u8)((18.0 + 20.0 * pulse((double)i)) * sc) }, (s32)left, (s32)(cy - h / 2.0 + H * 0.5 * (1.0 - sc) * 0.42), (s32)w, (s32)h, 3);
                px += side * (w + gap * sc);
            }
        }
        // Title and details placeholders
        const auto ty = y + (s32)(area_h * 0.75);
        FillRR(drawer, { 0xFF, 0xFF, 0xFF, (u8)(22.0 + 16.0 * pulse(1.0)) }, (s32)(cx - 260.0), ty + 8, 520, 34, 17);
        FillRR(drawer, { 0xFF, 0xFF, 0xFF, (u8)(14.0 + 12.0 * pulse(2.0)) }, (s32)(cx - 170.0), ty + 62, 340, 22, 11);
    }

    // ---- Rumble and extra sounds ----

    void EntryMenu::Haptic(const float amp, const float freq, const double dur, const double delay) {
        if(!nxdash::On("haptics")) {
            return;
        }
        if(this->hap_queue.size() < 32) {
            this->hap_queue.push_back({ this->fx_time + delay, dur, amp, freq });
        }
    }

    void EntryMenu::HapticUpdate() {
        if(this->hap_queue.empty() && (this->hap_off_at < 0.0)) {
            return;
        }
        if(!this->hap_init) {
            this->hap_init = true;
            // Handheld rails and a detached pair are set up separately: either one working is enough
            this->hap_sets[0] = R_SUCCEEDED(hidInitializeVibrationDevices(this->hap_handles, 2, HidNpadIdType_Handheld, HidNpadStyleTag_NpadHandheld));
            this->hap_sets[1] = R_SUCCEEDED(hidInitializeVibrationDevices(this->hap_handles + 2, 2, HidNpadIdType_No1, HidNpadStyleTag_NpadJoyDual));
            this->hap_ok = this->hap_sets[0] || this->hap_sets[1];
        }
        if(!this->hap_ok) {
            this->hap_queue.clear();
            this->hap_off_at = -1.0;
            return;
        }

        const auto send = [&](const float amp, const float freq) {
            HidVibrationValue values[2];
            for(auto &value : values) {
                value.amp_low = amp;
                value.freq_low = freq;
                value.amp_high = amp * 0.6f;
                value.freq_high = freq * 2.0f;
            }
            for(u32 set = 0; set < 2; set++) {
                if(this->hap_sets[set]) {
                    hidSendVibrationValues(this->hap_handles + set * 2, values, 2);
                }
            }
        };

        for(auto it = this->hap_queue.begin(); it != this->hap_queue.end(); ) {
            if(it->at <= this->fx_time) {
                send(std::clamp(it->amp, 0.0f, 1.0f), it->freq);
                this->hap_off_at = std::max(this->hap_off_at, this->fx_time + it->dur);
                it = this->hap_queue.erase(it);
            }
            else {
                it++;
            }
        }
        if((this->hap_off_at >= 0.0) && (this->fx_time >= this->hap_off_at)) {
            send(0.0f, 160.0f);
            this->hap_off_at = -1.0;
        }
    }

    void EntryMenu::NxSfx(const u32 which) {
        if(!nxdash::On("sounds") || (which >= 3)) {
            return;
        }
        if(!this->nx_sfx_loaded) {
            this->nx_sfx_loaded = true;
            static const char *const Names[3] = { "sound/NxDash/Case.wav", "sound/NxDash/End.wav", "sound/NxDash/Pick.wav" };
            for(u32 i = 0; i < 3; i++) {
                const auto path = TryGetActiveThemeResource(Names[i]);
                this->nx_sfx[i] = path.empty() ? nullptr : pu::audio::LoadSfx(path);
            }
        }
        if(this->nx_sfx[which] != nullptr) {
            pu::audio::PlaySfx(this->nx_sfx[which]);
        }
    }

    // ---- Idle drift ----

    void EntryMenu::ShelfStepSilent(const s32 dir) {
        const auto nv = (s32)this->shelf_valid.size();
        if(nv < 2) {
            return;
        }
        const auto prev_idx = (s32)this->cur_entry_idx;
        if(nv < ShelfLoopMin) {
            const auto cur = ((this->shelf_target % nv) + nv) % nv;
            auto next = cur + dir;
            if((next < 0) || (next >= nv)) {
                // Short shelf: turn around at the ends
                this->saver_dir = -this->saver_dir;
                next = cur + this->saver_dir;
            }
            this->shelf_target = std::clamp(next, 0, nv - 1);
        }
        else {
            this->shelf_target += dir;
        }
        const auto pos = ((this->shelf_target % nv) + nv) % nv;
        this->SetFocusedEntryIndex(this->shelf_valid.at(pos));
        this->NotifyFocusedEntryChanged(prev_idx);
    }

    void EntryMenu::SaverStop() {
        if(!this->saver_on) {
            return;
        }
        this->saver_on = false;
        this->idle_t = 0.0;
        // Back to where the user left it
        const auto nv = (s32)this->shelf_valid.size();
        if((nv > 0) && (this->saver_saved_pos < nv)) {
            const auto prev_idx = (s32)this->cur_entry_idx;
            const auto cur = ((this->shelf_target % nv) + nv) % nv;
            auto delta = this->saver_saved_pos - cur;
            if(nv >= ShelfLoopMin) {
                delta = ((delta % nv) + nv) % nv;
                if(delta > nv / 2) {
                    delta -= nv;
                }
            }
            this->shelf_target += delta;
            this->SetFocusedEntryIndex(this->shelf_valid.at(this->saver_saved_pos));
            this->NotifyFocusedEntryChanged(prev_idx);
        }
    }

    // ---- Shared bits ----

    u32 EntryMenu::QueryLaunches(const Entry &entry, u64 *out_seconds, s64 *out_last_posix) {
        if(out_seconds != nullptr) {
            *out_seconds = 0;
        }
        if(out_last_posix != nullptr) {
            *out_last_posix = -1;
        }
        if(!entry.Is<EntryType::Application>()) {
            return 0;
        }
        const auto own = session::GetAppStats(entry.app_info.app_id);
        u32 launches = own.launches;
        if(out_seconds != nullptr) {
            *out_seconds = own.total_seconds;
        }

        // The console keeps its own history, which reaches back before NX Dash existed
        if(!this->pdm_init) {
            this->pdm_init = true;
            this->pdm_ok = R_SUCCEEDED(pdmqryInitialize());
        }
        if(this->pdm_ok) {
            PdmPlayStatistics stats = {};
            if(R_SUCCEEDED(pdmqryQueryPlayStatisticsByApplicationIdAndUserAccountId(entry.app_info.app_id, g_GlobalSettings.system_status.selected_user, false, &stats))) {
                launches = std::max(launches, stats.total_launches);
                if(out_seconds != nullptr) {
                    *out_seconds = std::max<u64>(*out_seconds, stats.playtime / 1'000'000'000ul);
                }
                if((out_last_posix != nullptr) && (stats.total_launches > 0)) {
                    *out_last_posix = (s64)stats.last_timestamp_user;
                }
            }
        }
        return launches;
    }

    pu::sdl2::TextureHandle::Ref EntryMenu::IconForPos(const s32 pos, const bool load) {
        if((pos < 0) || (pos >= (s32)this->shelf_valid.size())) {
            return nullptr;
        }
        const auto idx = this->shelf_valid.at(pos);
        if((idx >= this->entry_icons.size()) || (idx >= this->cur_entries.size())) {
            return nullptr;
        }
        auto &icon = this->entry_icons.at(idx);
        if((icon == nullptr) && load && this->cur_entries.at(idx).Is<EntryType::Application>()) {
            icon = LoadApplicationIconTexture(this->cur_entries.at(idx).app_info.app_id);
        }
        return icon;
    }

    void EntryMenu::OnWake() {
        // Audio was torn down and rebuilt around sleep: the old sound handles are gone
        for(auto &sfx : this->nx_sfx) {
            sfx = nullptr;
        }
        this->nx_sfx_loaded = false;
        this->idle_t = 0.0;
        this->SaverStop();
        this->OpenLock();
    }

    void EntryMenu::RenderLayers(pu::ui::render::Renderer::Ref &drawer) {
        if(!ShelfMode) {
            return;
        }
        // Idle drift: quieten the chrome so only the shelf reads
        if(this->saver_fade > 0.01) {
            EnsureShapes();
            const s32 sw = pu::ui::render::ScreenWidth;
            const s32 sh = pu::ui::render::ScreenHeight;
            if(g_FadeDown != nullptr) {
                drawer->RenderTexture(g_FadeDown, 0, 0, pu::ui::render::TextureRenderOptions((u8)(250.0 * this->saver_fade), sw, 330, {}, {}, {}));
            }
            if(g_FadeUp != nullptr) {
                drawer->RenderTexture(g_FadeUp, 0, sh - 330, pu::ui::render::TextureRenderOptions((u8)(250.0 * this->saver_fade), sw, 330, {}, {}, {}));
            }
        }
        this->RenderFlip(drawer);
        this->RenderDuel(drawer);
        this->RenderBracket(drawer);
        this->RenderLock(drawer);
    }

    // ---- Lock screen ----

    namespace {

        constexpr u32 LockClockFontSize = 150;

        std::string LockDateString() {
            const auto now = ::time(nullptr);
            const auto lt = localtime(&now);
            if(lt == nullptr) {
                return "";
            }
            auto text = nxt::T("date");
            text = nxt::Sub(text, "day", nxt::T("day_" + std::to_string(lt->tm_wday % 7)));
            text = nxt::Sub(text, "month", nxt::T("mon_" + std::to_string(lt->tm_mon % 12)));
            return nxt::Sub(text, "d", std::to_string(lt->tm_mday));
        }

        std::string LockTimeString() {
            const auto now = ::time(nullptr);
            const auto lt = localtime(&now);
            if(lt == nullptr) {
                return "--:--";
            }
            char buf[16];
            snprintf(buf, sizeof(buf), "%02d:%02d", lt->tm_hour, lt->tm_min);
            return buf;
        }

    }

    void EntryMenu::LockFree() {
        Free(this->lock_time_tex);
        Free(this->lock_date_tex);
        Free(this->lock_title_tex);
        Free(this->lock_played_tex);
        Free(this->lock_badge_tex);
        Free(this->lock_hint_tex);
        Free(this->lock_bt_tex);
        Free(this->lock_bt_sub_tex);
        for(auto &tex : this->lock_val_texs) {
            Free(tex);
        }
        this->lock_bt_sub_str.clear();
        for(auto &tex : this->lock_ctl_texs) {
            Free(tex);
        }
        for(auto &tex : this->lock_opt_texs) {
            Free(tex);
        }
        for(auto &tex : this->lock_key_texs) {
            Free(tex);
        }
        this->lock_opt_texs.clear();
        this->lock_key_texs.clear();
        this->lock_opt_keys.clear();
        this->lock_time_str.clear();
        this->lock_bt_str.clear();
    }

    void EntryMenu::OpenLock() {
        if(!ShelfMode || !nxdash::On("lock") || this->lock_open || (this->launch_t >= 0.0)) {
            return;
        }
        this->LockFree();
        this->sheet_open = false;
        this->sheet_anim = 0.0;
        this->flip_open = false;
        this->duel_open = false;

        if(!this->lock_font_ready) {
            // The big clock font is only built the first time the lock screen shows
            this->lock_font_ready = true;
            auto font = std::make_shared<pu::ttf::Font>(LockClockFontSize);
            if(pu::ui::render::LoadSingleSharedFontInFont(font, PlSharedFontType_Standard)) {
                pu::ui::render::AddDefaultFont(font);
            }
        }
        if(!this->lock_svc_init) {
            this->lock_svc_init = true;
            this->lock_lbl_ok = R_SUCCEEDED(lblInitialize());
            this->lock_aud_ok = R_SUCCEEDED(audctlInitialize());
        }
        if(this->lock_lbl_ok && R_FAILED(lblGetCurrentBrightnessSetting(&this->lock_bright))) {
            this->lock_lbl_ok = false;
        }
        if(this->lock_aud_ok) {
            AudioTarget target = AudioTarget_Speaker;
            s32 vol = 0;
            s32 vol_max = 15;
            if(R_SUCCEEDED(audctlGetDefaultTarget(&target)) && R_SUCCEEDED(audctlGetTargetVolume(&vol, target))) {
                if(R_FAILED(audctlGetTargetVolumeMax(&vol_max)) || (vol_max <= 0)) {
                    vol_max = 15;
                }
                this->lock_audio_target = (u32)target;
                this->lock_vol = vol;
                this->lock_vol_max = vol_max;
            }
            else {
                this->lock_aud_ok = false;
            }
        }

        this->lock_date_tex = Text(pu::ui::DefaultFontSize::MediumLarge, LockDateString());
        this->lock_ctl_texs[0] = Text(pu::ui::DefaultFontSize::MediumLarge, nxt::T("brightness"));
        this->lock_ctl_texs[1] = Text(pu::ui::DefaultFontSize::MediumLarge, nxt::T("volume"));
        {
            static const char *const Names[7] = { "Play", "Album", "Library", "Headphones", "Sun", "Volume", "Power" };
            for(u32 i = 0; i < 7; i++) {
                if(this->sheet_icons_lock[i] == nullptr) {
                    this->sheet_icons_lock[i] = TryFindLoadImage(std::string("ui/Main/NxDash/") + Names[i]);
                }
            }
        }

        std::vector<std::string> labels;
        if(g_GlobalSettings.IsSuspended()) {
            std::string title;
            std::string meta;
            this->DescribeSuspended(title, meta);
            this->lock_title_tex = Text(pu::ui::DefaultFontSize::MediumLarge, title.empty() ? nxt::T("paused_game") : title);
            this->lock_badge_tex = Text(pu::ui::DefaultFontSize::Small, nxt::T("paused_badge"));
            if(const auto played = session::GetSeconds(); played >= 0) {
                this->lock_played_tex = Text(pu::ui::DefaultFontSize::Small, nxt::Played((u64)played));
            }
            this->lock_opt_keys = { HidNpadButton_A, HidNpadButton_X, HidNpadButton_Y };
            labels = { nxt::T("continue"), nxt::T("library"), nxt::T("album") };
        }
        else {
            this->lock_opt_keys = { HidNpadButton_A, HidNpadButton_Y };
            labels = { nxt::T("library"), nxt::T("album") };
        }
        for(u32 i = 0; i < labels.size(); i++) {
            this->lock_opt_texs.push_back(Text(pu::ui::DefaultFontSize::MediumLarge, labels.at(i)));
        }

        this->LockRefreshBluetooth(true);
        this->lock_btn = 0;
        this->lock_count = 0;
        this->lock_pop = 0.0;
        this->lock_sel = 0;
        this->lock_lens_init = false;
        this->lock_hint_str.clear();
        this->lock_val_str[0].clear();
        this->lock_val_str[1].clear();
        this->lock_drag = -1;
        this->lock_touching = false;
        this->lock_out = -1.0;
        this->lock_anim = 0.0;
        this->lock_open = true;
    }

    void EntryMenu::LockRefreshBluetooth(const bool force) {
        this->lock_bt_poll += this->fx_dt;
        if(!force && (this->lock_bt_poll < 0.25)) {
            return;
        }
        this->lock_bt_poll = 0.0;
        const auto dev = bt::GetConnectedAudioDevice();
        this->lock_bt_connected = dev.name[0] != '\0';
        std::string name;
        if(this->lock_bt_connected) {
            name = std::string(dev.name, strnlen(dev.name, sizeof(dev.name)));
            this->lock_bt_paired = true;
        }
        else {
            const auto paired = bt::ListPairedAudioDevices();
            this->lock_bt_paired = !paired.empty();
            name = paired.empty() ? nxt::T("headphones") : std::string(paired.front().name, strnlen(paired.front().name, sizeof(paired.front().name)));
        }
        std::string sub = this->lock_bt_connected ? nxt::T("connected") : nxt::T("connect");
        if(this->bt_pending) {
            sub = nxt::T(this->bt_pending_connect ? "bt_st_connecting" : "bt_st_disconnecting");
        }
        else if(this->fx_time < this->bt_failed_t) {
            sub = nxt::T("bt_st_failed");
        }
        if(force || (name != this->lock_bt_str) || (sub != this->lock_bt_sub_str) || (this->lock_bt_tex == nullptr)) {
            this->lock_bt_str = name;
            this->lock_bt_sub_str = sub;
            Free(this->lock_bt_tex);
            Free(this->lock_bt_sub_tex);
            this->lock_bt_tex = Text(pu::ui::DefaultFontSize::MediumLarge, name);
            this->lock_bt_sub_tex = Text(pu::ui::DefaultFontSize::Medium, sub);
        }
    }

    void EntryMenu::LockBluetooth() {
        if(this->bt_pending) {
            return;
        }
        const auto cur = bt::GetConnectedAudioDevice();
        if(cur.name[0] != '\0') {
            this->BtRequest(cur, false);
        }
        else {
            const auto paired = bt::ListPairedAudioDevices();
            if(paired.empty()) {
                g_MenuApplication->ShowNotification(nxt::T("bt_pair_first"), 3000);
            }
            else {
                this->BtRequest(paired.front(), true);
            }
        }
        this->lock_bt_poll = 1.0;
        this->sheet_bt_poll = 1.0;
    }

    void EntryMenu::LockSetBrightness(const float value) {
        if(!this->lock_lbl_ok) {
            return;
        }
        this->lock_bright = std::clamp(value, 0.05f, 1.0f);
        lblSetCurrentBrightnessSetting(this->lock_bright);
    }

    void EntryMenu::LockSetVolume(const s32 value) {
        if(!this->lock_aud_ok) {
            return;
        }
        const auto vol = std::clamp(value, 0, this->lock_vol_max);
        if(vol != this->lock_vol) {
            this->lock_vol = vol;
            audctlSetTargetVolume((AudioTarget)this->lock_audio_target, vol);
        }
    }

    void EntryMenu::LockPress(const u64 key) {
        if((this->lock_out >= 0.0) || (std::find(this->lock_opt_keys.begin(), this->lock_opt_keys.end(), key) == this->lock_opt_keys.end())) {
            return;
        }
        if((this->lock_btn != key) || ((this->fx_time - this->lock_last) > 1.2)) {
            this->lock_btn = key;
            this->lock_count = 0;
        }
        this->lock_count++;
        this->lock_last = this->fx_time;
        this->lock_pop = 1.0;
        this->Haptic(0.25f + 0.15f * this->lock_count, 180.0f + 40.0f * this->lock_count, 0.02);
        if(this->lock_count >= 3) {
            this->lock_choice = key;
            this->lock_out = 0.0;
            this->NxSfx(2);
        }
    }

    void EntryMenu::LockInput(const u64 keys_down, const pu::ui::TouchPoint touch_pos) {
        if(this->lock_out >= 0.0) {
            return;
        }

        if(!touch_pos.IsEmpty()) {
            if(!this->lock_touching) {
                this->lock_touching = true;
                this->lock_drag = -1;
                for(const auto &hit : this->lock_hits) {
                    if((hit.pos >= 20) && (hit.pos < 100) && Hit(touch_pos.x, touch_pos.y, hit.x - 30, hit.y - 30, hit.w + 60, hit.h + 60)) {
                        this->lock_drag = hit.pos;
                    }
                }
            }
            this->lock_tx = touch_pos.x;
            this->lock_ty = touch_pos.y;
            if(this->lock_drag >= 20) {
                for(const auto &hit : this->lock_hits) {
                    if(hit.pos == this->lock_drag) {
                        const auto v = std::clamp((double)(touch_pos.x - hit.x) / (double)std::max(1, hit.w), 0.0, 1.0);
                        if(hit.pos == 20) {
                            this->LockSetBrightness((float)v);
                        }
                        else {
                            this->LockSetVolume((s32)std::lround(v * this->lock_vol_max));
                        }
                    }
                }
            }
            return;
        }
        if(this->lock_touching) {
            this->lock_touching = false;
            if(this->lock_drag < 0) {
                for(const auto &hit : this->lock_hits) {
                    if((hit.pos >= 100) && Hit(this->lock_tx, this->lock_ty, hit.x, hit.y, hit.w, hit.h)) {
                        const auto row = hit.pos - 100;
                        if(row != this->lock_sel) {
                            this->lock_sel = row;
                            this->lock_btn = 0;
                            this->lock_count = 0;
                            this->NxSfx(0);
                        }
                        else {
                            this->LockActivate();
                        }
                        break;
                    }
                }
            }
            this->lock_drag = -1;
            return;
        }

        const auto n_act = (s32)this->lock_opt_keys.size();
        const auto rows = n_act + 3;
        if(keys_down & (HidNpadButton_AnyUp | HidNpadButton_AnyDown)) {
            const auto next = std::clamp(this->lock_sel + ((keys_down & HidNpadButton_AnyUp) ? -1 : 1), 0, rows - 1);
            if(next != this->lock_sel) {
                this->lock_sel = next;
                this->lock_btn = 0;
                this->lock_count = 0;
                this->NxSfx(0);
                this->Haptic(0.18f, 160.0f, 0.015);
            }
            else {
                this->NxSfx(1);
                this->Haptic(0.3f, 120.0f, 0.03);
            }
        }
        else if(keys_down & (HidNpadButton_AnyLeft | HidNpadButton_AnyRight)) {
            const auto dir = (keys_down & HidNpadButton_AnyRight) ? 1 : -1;
            if(this->lock_sel == n_act + 1) {
                this->LockSetBrightness(this->lock_bright + 0.05f * dir);
            }
            else if(this->lock_sel == n_act + 2) {
                this->LockSetVolume(this->lock_vol + dir);
            }
        }
        else if(keys_down & HidNpadButton_A) {
            this->LockActivate();
        }
    }

    void EntryMenu::LockActivate() {
        const auto n_act = (s32)this->lock_opt_keys.size();
        if(this->lock_sel < n_act) {
            this->LockPress(this->lock_opt_keys.at(this->lock_sel));
        }
        else if(this->lock_sel == n_act) {
            this->LockBluetooth();
        }
    }

    void EntryMenu::RenderLock(pu::ui::render::Renderer::Ref &drawer) {
        if(!this->lock_open) {
            return;
        }
        this->lock_anim = std::min(1.0, this->lock_anim + this->fx_dt / 0.45);
        if(this->lock_out >= 0.0) {
            this->lock_out = std::min(1.0, this->lock_out + this->fx_dt / 0.40);
        }
        const auto in_a = SmoothStep(this->lock_anim);
        const auto out = (this->lock_out >= 0.0) ? SmoothStep(this->lock_out) : 0.0;
        const auto vis = in_a * (1.0 - out);
        const auto al = [&](const double v) {
            return std::clamp(v * vis, 0.0, 255.0);
        };
        const s32 sw = pu::ui::render::ScreenWidth;
        const s32 sh = pu::ui::render::ScreenHeight;
        const auto lift = (s32)(-out * 90.0 + (1.0 - in_a) * 24.0);
        EnsureShapes();

        // Backdrop: the paused game if there is one, otherwise the ambient light of the shelf
        drawer->RenderRectangleFill({ 0x05, 0x06, 0x08, (u8)(255.0 * (1.0 - out)) }, 0, 0, sw, sh);
        auto capture = (g_GlobalSettings.IsSuspended() && HasScreenCaptureBackground()) ? GetScreenCaptureBackground()->GetTexture() : nullptr;
        if(capture != nullptr) {
            const auto zoom = 1.04 + 0.05 * out;
            const auto zw = (double)sw * zoom;
            const auto zh = (double)sh * zoom;
            drawer->RenderTexture(capture, (s32)(((double)sw - zw) / 2.0), (s32)(((double)sh - zh) / 2.0), pu::ui::render::TextureRenderOptions((u8)al(255.0), (s32)zw, (s32)zh, {}, {}, {}));
            drawer->RenderRectangleFill({ 0x05, 0x06, 0x0A, (u8)al(168.0) }, 0, 0, sw, sh);
        }
        else if(this->amb_cur != nullptr) {
            drawer->RenderTexture(this->amb_cur, -sw / 8, -sh / 8, pu::ui::render::TextureRenderOptions((u8)al(120.0), sw + sw / 4, sh + sh / 4, {}, {}, {}));
        }
        if(g_FadeDown != nullptr) {
            drawer->RenderTexture(g_FadeDown, 0, 0, pu::ui::render::TextureRenderOptions((u8)al(150.0), sw, 340, {}, {}, {}));
        }
        if(g_FadeUp != nullptr) {
            drawer->RenderTexture(g_FadeUp, 0, sh - 480, pu::ui::render::TextureRenderOptions((u8)al(200.0), sw, 480, {}, {}, {}));
        }

        // Left column: time and what is paused. No boxes.
        const s32 lx = 150;
        Draw(drawer, this->lock_date_tex, lx + 6, 236 + lift, al(190.0));
        const auto time_str = LockTimeString();
        if((time_str != this->lock_time_str) || (this->lock_time_tex == nullptr)) {
            this->lock_time_str = time_str;
            Free(this->lock_time_tex);
            this->lock_time_tex = pu::ui::render::RenderText(pu::ui::MakeDefaultFontName(LockClockFontSize), time_str, { 0xFF, 0xFF, 0xFF, 0xFF });
            if(this->lock_time_tex == nullptr) {
                this->lock_time_tex = Text(pu::ui::DefaultFontSize::Large, time_str);
            }
        }
        if(this->lock_time_tex != nullptr) {
            // If the big font was not available, the regular one is scaled up instead
            const auto th = TexH(this->lock_time_tex);
            const auto k = (th < 100) ? (170.0 / (double)std::max(1, th)) : 1.0;
            drawer->RenderTexture(this->lock_time_tex, lx, 290 + lift + (s32)((1.0 - in_a) * 24.0), pu::ui::render::TextureRenderOptions((u8)al(255.0), (s32)(TexW(this->lock_time_tex) * k), (s32)(th * k), {}, {}, {}));
        }
        if(this->lock_title_tex != nullptr) {
            const s32 cy = 640 + lift;
            s32 tx = lx + 6;
            if(capture != nullptr) {
                drawer->RenderTexture(capture, tx, cy, pu::ui::render::TextureRenderOptions((u8)al(255.0), 192, 108, {}, {}, {}));
                tx += 218;
            }
            Draw(drawer, this->lock_badge_tex, tx, cy + 8, al(170.0));
            Draw(drawer, this->lock_played_tex, tx + TexW(this->lock_badge_tex) + 18, cy + 8, al(170.0));
            RenderTextFit(drawer, this->lock_title_tex, (double)tx, cy + 46, 760.0, (u8)al(255.0), false);
        }

        // Right column: one quiet list. Actions, a hairline, then the controls.
        this->lock_hits.clear();
        this->LockRefreshBluetooth(false);
        this->lock_pop *= std::pow(0.86, this->fx_k);
        if((this->lock_btn != 0) && (this->lock_out < 0.0) && ((this->fx_time - this->lock_last) > 1.2)) {
            this->lock_btn = 0;
            this->lock_count = 0;
        }
        const auto n_act = (s32)this->lock_opt_texs.size();
        const auto rows = n_act + 3;
        this->lock_sel = std::clamp(this->lock_sel, 0, rows - 1);
        const s32 rx = 1150;
        const s32 rw = 620;
        const s32 row_h = 68;
        const s32 row_big = 100;
        const s32 sep_h = 34;
        // Row geometry: sliders grow when focused to show their track
        s32 row_y[16] = {};
        s32 row_hh[16] = {};
        s32 bottom = 0;
        {
            s32 oy = 200 + lift;
            for(s32 r = 0; r < rows; r++) {
                if(r == n_act) {
                    oy += sep_h;
                }
                const auto is_slider = r >= n_act + 1;
                row_y[r] = oy;
                row_hh[r] = (is_slider && (r == this->lock_sel)) ? row_big : row_h;
                oy += row_hh[r] + 6;
            }
            bottom = oy;
            // The hairline between actions and controls
            FillRR(drawer, { 0xFF, 0xFF, 0xFF, (u8)al(34.0) }, rx + 24, row_y[n_act] - sep_h / 2 - 3, rw - 48, 1, 0);
        }
        {
            const auto ty = (double)row_y[this->lock_sel];
            const auto th = (double)row_hh[this->lock_sel];
            if(!this->lock_lens_init) {
                this->lock_lens_init = true;
                this->lock_lens_y = ty;
                this->lock_lens_h = th;
            }
            this->lock_lens_y = this->FxApproach(this->lock_lens_y, ty, 0.32);
            this->lock_lens_h = this->FxApproach(this->lock_lens_h, th, 0.32);
            const auto r = (s32)std::min(34.0, this->lock_lens_h / 2.0);
            RenderGlass(drawer, rx, (s32)this->lock_lens_y, rw, (s32)this->lock_lens_h, r, vis, 92);
            FillRR(drawer, { 0xFF, 0xFF, 0xFF, (u8)al(22.0) }, rx, (s32)this->lock_lens_y, rw, (s32)(this->lock_lens_h / 2.0), r);
        }
        const auto glyph = [&](const s32 idx, const s32 cx, const s32 cy, const double a) {
            auto icon = (idx >= 0) && (idx < 7) ? this->sheet_icons_lock[idx] : nullptr;
            if(icon != nullptr) {
                drawer->RenderTexture(icon, cx - 20, cy - 20, pu::ui::render::TextureRenderOptions((u8)al(a), 40, 40, {}, {}, {}));
            }
        };
        for(s32 r = 0; r < rows; r++) {
            const auto f = r == this->lock_sel;
            const auto dx = f ? 10 : 0;
            const auto ta = f ? 255.0 : 118.0;
            const auto y = row_y[r];
            const auto mid = y + row_h / 2;
            const s32 lab_x = rx + 82 + dx;
            if(r < n_act) {
                // Actions: pressed three times to go
                const auto key = this->lock_opt_keys.at(r);
                glyph((key == HidNpadButton_A) && g_GlobalSettings.IsSuspended() ? 0 : ((key == HidNpadButton_Y) ? 1 : 2), rx + 44 + dx, mid, ta);
                auto tex = this->lock_opt_texs.at(r);
                Draw(drawer, tex, lab_x, mid - TexH(tex) / 2, al(ta));
                if(f && (this->lock_btn == key)) {
                    for(s32 dot = 0; dot < 3; dot++) {
                        const auto filled = dot < (s32)this->lock_count;
                        const auto rr = filled ? 8 + (s32)(3.0 * this->lock_pop) : 6;
                        FillRR(drawer, { 0xFF, 0xFF, 0xFF, (u8)al(filled ? 255.0 : 60.0) }, rx + rw - 92 + dot * 26 - rr, mid - rr, rr * 2, rr * 2, rr);
                    }
                }
            }
            else if(r == n_act) {
                // Headphones: one press to connect the paired pair
                glyph(3, rx + 44 + dx, mid, ta);
                RenderTextFit(drawer, this->lock_bt_tex, (double)lab_x, mid - TexH(this->lock_bt_tex) / 2, (double)(rw - 260), (u8)al(ta), false);
                if(this->lock_bt_sub_tex != nullptr) {
                    const auto connected = this->lock_bt_connected;
                    if(connected) {
                        SDL_SetTextureColorMod(this->lock_bt_sub_tex, 0x5B, 0xE3, 0x8A);
                    }
                    Draw(drawer, this->lock_bt_sub_tex, rx + rw - 32 - TexW(this->lock_bt_sub_tex), mid - TexH(this->lock_bt_sub_tex) / 2, al(connected ? 235.0 : (f ? 220.0 : 96.0)));
                    if(connected) {
                        SDL_SetTextureColorMod(this->lock_bt_sub_tex, 0xFF, 0xFF, 0xFF);
                    }
                }
            }
            else {
                // Brightness and volume: the value until focused, then the track
                const auto i = r - n_act - 1;
                const auto ok = (i == 0) ? this->lock_lbl_ok : this->lock_aud_ok;
                const auto v = (i == 0) ? (double)this->lock_bright : ((double)this->lock_vol / (double)std::max(1, this->lock_vol_max));
                glyph(4 + i, rx + 44 + dx, mid, ok ? ta : 70.0);
                Draw(drawer, this->lock_ctl_texs[i], lab_x, mid - TexH(this->lock_ctl_texs[i]) / 2, al(ok ? ta : 70.0));
                const auto val = std::to_string((s32)std::lround(v * 100.0));
                if(val != this->lock_val_str[i]) {
                    this->lock_val_str[i] = val;
                    Free(this->lock_val_texs[i]);
                    this->lock_val_texs[i] = Text(pu::ui::DefaultFontSize::Medium, val);
                }
                Draw(drawer, this->lock_val_texs[i], rx + rw - 32 - TexW(this->lock_val_texs[i]), mid - TexH(this->lock_val_texs[i]) / 2, al(f ? 220.0 : 96.0));
                if(f && ok) {
                    const s32 tx = lab_x;
                    const s32 tw = rx + rw - 40 - tx;
                    const s32 ty = y + row_h + 8;
                    FillRR(drawer, { 0xFF, 0xFF, 0xFF, (u8)al(46.0) }, tx, ty - 3, tw, 6, 3);
                    FillRR(drawer, { 0xFF, 0xFF, 0xFF, (u8)al(240.0) }, tx, ty - 3, std::max(6, (s32)(tw * v)), 6, 3);
                    const auto knob = (this->lock_drag == 20 + i) ? 15 : 12;
                    FillRR(drawer, { 0xFF, 0xFF, 0xFF, (u8)al(255.0) }, tx + (s32)(tw * v) - knob, ty - knob, knob * 2, knob * 2, knob);
                    this->lock_hits.push_back({ 20 + i, tx, ty - 22, tw, 44 });
                }
            }
            this->lock_hits.push_back({ 100 + r, rx, y, rw, row_hh[r] });
        }

        // One line that says what A does here
        {
            std::string hint;
            if(this->lock_sel < n_act) {
                const auto key = this->lock_opt_keys.at(this->lock_sel);
                const auto action = ((key == HidNpadButton_A) && g_GlobalSettings.IsSuspended()) ? nxt::T("lock_resume") : ((key == HidNpadButton_Y) ? nxt::T("lock_album") : nxt::T("lock_library"));
                hint = nxt::Sub(nxt::T("lock_triple"), "action", action);
            }
            else if(this->lock_sel == n_act) {
                hint = !this->lock_bt_paired ? nxt::T("h_bt_none") : nxt::Sub(nxt::T(this->lock_bt_connected ? "h_bt_on" : "h_bt_off"), "name", this->lock_bt_str);
            }
            else {
                hint = nxt::T("lock_adjust");
            }
            if(hint != this->lock_hint_str) {
                this->lock_hint_str = hint;
                Free(this->lock_hint_tex);
                this->lock_hint_tex = Text(pu::ui::DefaultFontSize::Small, hint);
            }
            RenderTextFit(drawer, this->lock_hint_tex, (double)(rx + 32), bottom + 18, (double)(rw - 40), (u8)al(115.0), false);
        }

        if(this->lock_out >= 1.0) {
            // Unlocked: go where the chosen button points
            const auto choice = this->lock_choice;
            const auto was_suspended = g_GlobalSettings.IsSuspended();
            this->lock_open = false;
            this->lock_out = -1.0;
            this->LockFree();
            if((choice == HidNpadButton_A) && was_suspended) {
                this->CloseSheetForResume();
                if(this->sheet_action_cb) {
                    this->sheet_action_cb(SheetAction::Resume);
                }
            }
            else if(choice == HidNpadButton_Y) {
                if(this->sheet_action_cb) {
                    this->sheet_action_cb(SheetAction::Album);
                }
            }
        }
    }

    // ---- The back of the case ----

    void EntryMenu::FlipFree() {
        Free(this->flip_title);
        Free(this->flip_hint);
        Free(this->flip_chart_title);
        for(auto &tex : this->flip_lines) {
            Free(tex);
        }
        this->flip_lines.clear();
        for(auto &tex : this->flip_days) {
            Free(tex);
        }
        this->flip_icon = nullptr;
    }

    void EntryMenu::ToggleFlip() {
        if(this->flip_open) {
            this->flip_open = false;
            this->NxSfx(0);
            return;
        }
        const auto nv = (s32)this->shelf_valid.size();
        if(!ShelfMode || this->IsModalOpen() || this->defer_pending || this->saver_on || (nv == 0) || !this->IsFocusedNonemptyEntry()) {
            return;
        }
        auto &entry = this->GetFocusedEntry();
        if(!entry.Is<EntryType::Application>() && !entry.Is<EntryType::Homebrew>()) {
            return;
        }

        this->FlipFree();
        entry.TryLoadNacp();
        this->flip_title = Text(pu::ui::DefaultFontSize::Large, entry.control.name.empty() ? nxt::T("untitled") : entry.control.name);
        for(auto &secs : this->flip_week) {
            secs = 0;
        }

        std::vector<std::string> lines;
        if(entry.Is<EntryType::Application>()) {
            u64 total_secs = 0;
            s64 last_posix = -1;
            const auto launches = this->QueryLaunches(entry, &total_secs, &last_posix);
            const auto own = session::GetAppStats(entry.app_info.app_id);
            for(u32 i = 0; i < 7; i++) {
                this->flip_week[i] = own.week[i];
            }

            lines.push_back((total_secs > 0) ? nxt::Sub(nxt::T("flip_played"), "t", FormatPlayed(total_secs)) : nxt::T("flip_none"));
            s64 days_ago = own.days_since_last;
            if(last_posix > 0) {
                const auto diff = (s64)::time(nullptr) - last_posix;
                const auto console_days = (diff > 0) ? (diff / 86400) : 0;
                if((days_ago < 0) || (console_days < days_ago)) {
                    days_ago = console_days;
                }
            }
            if(days_ago == 0) {
                lines.push_back(nxt::T("flip_today"));
            }
            else if(days_ago == 1) {
                lines.push_back(nxt::T("flip_yesterday"));
            }
            else if(days_ago > 1) {
                lines.push_back(nxt::Sub(nxt::T("flip_days"), "n", std::to_string(days_ago)));
            }
            if(own.streak >= 2) {
                lines.push_back(nxt::Sub(nxt::T("flip_streak"), "n", std::to_string(own.streak)));
            }
            else if(own.streak == 1) {
                lines.push_back(nxt::T("flip_streak1"));
            }
            else {
                lines.push_back(nxt::T("flip_nostreak"));
            }
            if(launches > 0) {
                lines.push_back((launches == 1) ? nxt::T("flip_opened1") : nxt::Sub(nxt::T("flip_opened"), "n", std::to_string(launches)));
            }
            if(!entry.control.author.empty()) {
                lines.push_back(entry.control.author + (entry.control.version.empty() ? "" : ("  ·  " + nxt::Sub(nxt::T("version"), "v", entry.control.version))));
            }
        }
        else {
            lines.push_back("Homebrew");
            if(!entry.control.author.empty()) {
                lines.push_back(entry.control.author);
            }
            if(!entry.control.version.empty()) {
                lines.push_back(nxt::Sub(nxt::T("version"), "v", entry.control.version));
            }
        }
        for(const auto &line : lines) {
            this->flip_lines.push_back(Text(pu::ui::DefaultFontSize::Medium, line));
        }

        const auto day_letters = nxt::T("daylet");
        const auto now = ::time(nullptr);
        const auto lt = localtime(&now);
        const auto today_wd = (lt != nullptr) ? lt->tm_wday : 0;
        for(s32 i = 0; i < 7; i++) {
            this->flip_days[i] = Text(pu::ui::DefaultFontSize::Small, (day_letters.size() >= 7) ? day_letters.substr((((today_wd - (6 - i)) % 7) + 7) % 7, 1) : std::string(""));
        }
        this->flip_chart_title = Text(pu::ui::DefaultFontSize::Small, nxt::T("flip_week"));
        this->flip_hint = Text(pu::ui::DefaultFontSize::Small, nxt::T("flip_hint"));

        const auto pos = ((this->shelf_target % nv) + nv) % nv;
        this->flip_icon = this->IconForPos(pos, true);
        this->flip_clr = (g_GlobalSettings.IsSuspended() && g_GlobalSettings.IsEntrySuspended(entry)) ? ShelfActiveColor : ShelfCaseColor;
        this->flip_anim = 0.0;
        this->flip_touching = false;
        this->flip_open = true;
        this->NxSfx(0);
        this->Haptic(0.35f, 150.0f, 0.03);
        this->Haptic(0.2f, 240.0f, 0.02, 0.12);
    }

    void EntryMenu::FlipInput(const u64 keys_down, const pu::ui::TouchPoint touch_pos) {
        if(!touch_pos.IsEmpty()) {
            this->flip_touching = true;
            this->flip_tx = touch_pos.x;
            this->flip_ty = touch_pos.y;
            return;
        }
        if(this->flip_touching) {
            this->flip_touching = false;
            auto inside = false;
            for(const auto &hit : this->flip_hits) {
                inside = inside || Hit(this->flip_tx, this->flip_ty, hit.x, hit.y, hit.w, hit.h);
            }
            if(!inside) {
                this->ToggleFlip();
            }
            return;
        }
        if(keys_down & HidNpadButton_B) {
            this->ToggleFlip();
        }
        else if(keys_down & (HidNpadButton_A | HidNpadButton_X)) {
            // Close first, then let the usual handler launch it or open its options
            this->flip_open = false;
            this->flip_anim = 0.0;
            nxdash::g_Modal = false;
            if(keys_down & HidNpadButton_A) {
                this->shelf_press = 1.0;
            }
            this->cur_entry_input_cb(keys_down & (HidNpadButton_A | HidNpadButton_X));
        }
    }

    void EntryMenu::RenderFlip(pu::ui::render::Renderer::Ref &drawer) {
        this->flip_anim = this->FxApproach(this->flip_anim, this->flip_open ? 1.0 : 0.0, this->flip_open ? 0.17 : 0.26);
        if(!this->flip_open && (this->flip_anim < 0.004)) {
            if(this->flip_title != nullptr) {
                this->FlipFree();
            }
            this->flip_anim = 0.0;
            this->flip_hits.clear();
            return;
        }
        const auto a = SmoothStep(this->flip_anim);
        const auto al = [&](const double v) {
            return (u8)std::clamp(v * a, 0.0, 255.0);
        };
        const s32 sw = pu::ui::render::ScreenWidth;
        const s32 sh = pu::ui::render::ScreenHeight;
        drawer->RenderRectangleFill({ 0x06, 0x07, 0x0B, al(205.0) }, 0, 0, sw, sh);

        const auto pop = 0.95 + 0.05 * a;
        const s32 pw = (s32)(1580.0 * pop);
        const s32 ph = (s32)(760.0 * pop);
        const s32 px = (sw - pw) / 2;
        const s32 py = (sh - ph) / 2;
        RenderGlass(drawer, px, py, pw, ph, 36, a, 205);
        this->flip_hits.clear();
        this->flip_hits.push_back({ -1, px, py, pw, ph });

        // The case, opening: the cover swings out to the left and the cartridge shows on the right
        const auto clr = this->flip_clr;
        const s32 half_w = 300;
        const s32 half_h = 560;
        const s32 tray_x = px + 80 + half_w + 8;
        const s32 case_y = py + (ph - half_h) / 2 - 20;
        const s32 cover_x = tray_x - (s32)((half_w + 8) * a);
        // Tray (right half)
        FillRR(drawer, { clr.r, clr.g, clr.b, al(255.0) }, tray_x, case_y, half_w, half_h, 12);
        FillRR(drawer, { 0x00, 0x00, 0x00, al(70.0) }, tray_x + 14, case_y + 14, half_w - 28, half_h - 28, 8);
        {
            // Cartridge in its slot
            const s32 cw = 150;
            const s32 ch = 176;
            const s32 cx = tray_x + (half_w - cw) / 2;
            const s32 cy = case_y + 70 - (s32)((1.0 - a) * 30.0);
            FillRR(drawer, { 0x00, 0x00, 0x00, al(90.0) }, cx - 8, cy - 8, cw + 16, ch + 16, 12);
            FillRR(drawer, { 0x2B, 0x2D, 0x36, al(255.0) }, cx, cy, cw, ch, 10);
            if((this->flip_icon != nullptr) && (this->flip_icon->Get() != nullptr)) {
                drawer->RenderTexture(this->flip_icon->Get(), cx + 15, cy + 14, pu::ui::render::TextureRenderOptions(al(255.0), 120, 120, {}, {}, {}));
            }
            FillRR(drawer, { 0xFF, 0xFF, 0xFF, al(60.0) }, cx + 45, cy + ch - 26, 60, 8, 4);
            // Manual clips
            FillRR(drawer, { 0xFF, 0xFF, 0xFF, al(28.0) }, tray_x + 40, case_y + half_h - 150, half_w - 80, 10, 5);
            FillRR(drawer, { 0xFF, 0xFF, 0xFF, al(28.0) }, tray_x + 40, case_y + half_h - 110, half_w - 80, 10, 5);
        }
        // Cover (left half, inside face) with the sleeve showing through
        FillRR(drawer, { clr.r, clr.g, clr.b, al(255.0) }, cover_x, case_y, half_w, half_h, 12);
        FillRR(drawer, { 0x00, 0x00, 0x00, al(46.0) }, cover_x + 14, case_y + 14, half_w - 28, half_h - 28, 8);
        if((this->flip_icon != nullptr) && (this->flip_icon->Get() != nullptr)) {
            const s32 art = half_w - 56;
            drawer->RenderTexture(this->flip_icon->Get(), cover_x + 28, case_y + (half_h - art) / 2, pu::ui::render::TextureRenderOptions(al(150.0), art, art, {}, {}, {}));
        }
        // Hinge between the halves
        drawer->RenderRectangleFill({ 0x00, 0x00, 0x00, al(110.0) }, tray_x - 6, case_y + 12, 4, half_h - 24);
        if(g_GlossDown != nullptr) {
            drawer->RenderTexture(g_GlossDown, cover_x + 6, case_y, pu::ui::render::TextureRenderOptions(al(40.0), tray_x + half_w - cover_x - 12, half_h / 2, {}, {}, {}));
        }

        // Numbers
        const s32 x1 = px + 80 + 2 * half_w + 90;
        const double colw = (double)(px + pw - 70 - x1);
        RenderTextFit(drawer, this->flip_title, (double)x1, py + 84, colw, al(255.0), false);
        for(u32 i = 0; i < this->flip_lines.size(); i++) {
            // Lines arrive one after another
            const auto p = SmoothStep(std::clamp(this->flip_anim * 1.8 - 0.3 - (double)i * 0.10, 0.0, 1.0));
            RenderTextFit(drawer, this->flip_lines.at(i), (double)x1 + (1.0 - p) * 20.0, py + 164 + (s32)i * 50, colw, (u8)((i == 0 ? 255.0 : 205.0) * p * (this->flip_open ? 1.0 : a)), false);
        }

        // Last seven days
        const s32 chart_y = py + ph - 290;
        Draw(drawer, this->flip_chart_title, x1, chart_y, al(150.0));
        u32 week_max = 60;
        for(const auto secs : this->flip_week) {
            week_max = std::max(week_max, secs);
        }
        for(s32 i = 0; i < 7; i++) {
            const s32 bar_w = 64;
            const s32 bar_gap = 26;
            const s32 bar_max = 120;
            const s32 bx = x1 + i * (bar_w + bar_gap);
            const s32 base_y = chart_y + 48 + bar_max;
            const auto grow = SmoothStep(std::clamp(this->flip_anim * 1.6 - 0.35 - (double)i * 0.05, 0.0, 1.0));
            const auto h = (s32)std::max(6.0, (double)bar_max * ((double)this->flip_week[i] / (double)week_max) * grow);
            const auto today = i == 6;
            const auto played = this->flip_week[i] > 0;
            FillRR(drawer, today ? pu::ui::Color{ ShelfActiveColor.r, ShelfActiveColor.g, ShelfActiveColor.b, al(played ? 255.0 : 110.0) } : pu::ui::Color{ 0xFF, 0xFF, 0xFF, al(played ? 215.0 : 50.0) }, bx, base_y - h, bar_w, h, std::min(10, h / 2));
            Draw(drawer, this->flip_days[i], bx + (bar_w - TexW(this->flip_days[i])) / 2, base_y + 12, a * (today ? 255.0 : 150.0));
        }

        Draw(drawer, this->flip_hint, x1, py + ph - 70, a * 150.0);
    }

    // ---- "Which game?" duel ----

    void EntryMenu::DuelFree() {
        Free(this->duel_title);
        Free(this->duel_hint);
        Free(this->duel_msg);
        Free(this->duel_vs);
        for(auto &tex : this->duel_names) {
            Free(tex);
        }
    }

    void EntryMenu::DuelOpen() {
        if(this->IsModalOpen() || this->defer_pending) {
            return;
        }
        std::vector<s32> candidates;
        for(s32 pos = 0; pos < (s32)this->shelf_valid.size(); pos++) {
            const auto &entry = this->cur_entries.at(this->shelf_valid.at(pos));
            if(entry.Is<EntryType::Application>() || entry.Is<EntryType::Homebrew>()) {
                candidates.push_back(pos);
            }
        }
        if(candidates.size() < 2) {
            g_MenuApplication->ShowNotification(nxt::T("duel_need2"), 2200);
            return;
        }
        this->DuelFree();
        this->duel_title = Text(pu::ui::DefaultFontSize::Large, nxt::T("duel_title"));
        this->duel_vs = Text(pu::ui::DefaultFontSize::Large, nxt::T("duel_or"));
        this->duel_rolls = 0;
        this->duel_tired = false;
        this->duel_pos[0] = -1;
        this->duel_pos[1] = -1;
        this->duel_sel = 0;
        this->duel_anim = 0.0;
        this->duel_touching = false;
        this->duel_open = true;
        this->DuelRoll();
    }

    void EntryMenu::DuelRoll() {
        if(this->duel_rolls >= 3) {
            // Three throws is the deal
            if(!this->duel_tired) {
                this->duel_tired = true;

                Free(this->duel_msg);
                this->duel_msg = pu::ui::render::RenderText(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Medium), nxt::T("duel_tired_" + std::to_string(armGetSystemTick() % 4)), { ShelfActiveColor.r, ShelfActiveColor.g, ShelfActiveColor.b, 0xFF });
            }
            this->Haptic(0.7f, 110.0f, 0.05);
            this->Haptic(0.7f, 110.0f, 0.05, 0.12);
            this->NxSfx(1);
            return;
        }

        std::vector<s32> candidates;
        for(s32 pos = 0; pos < (s32)this->shelf_valid.size(); pos++) {
            const auto &entry = this->cur_entries.at(this->shelf_valid.at(pos));
            if(entry.Is<EntryType::Application>() || entry.Is<EntryType::Homebrew>()) {
                candidates.push_back(pos);
            }
        }
        if(candidates.size() < 2) {
            this->duel_open = false;
            return;
        }
        // xorshift on the system tick: plenty for picking two games
        u64 seed = armGetSystemTick() | 1;
        const auto next = [&]() {
            seed ^= seed << 13;
            seed ^= seed >> 7;
            seed ^= seed << 17;
            return seed;
        };
        const auto prev_a = this->duel_pos[0];
        const auto prev_b = this->duel_pos[1];
        s32 pick_a = 0;
        s32 pick_b = 0;
        for(u32 attempt = 0; attempt < 12; attempt++) {
            pick_a = candidates.at(next() % candidates.size());
            pick_b = candidates.at(next() % candidates.size());
            const auto same_pair = ((pick_a == prev_a) && (pick_b == prev_b)) || ((pick_a == prev_b) && (pick_b == prev_a));
            if((pick_a != pick_b) && (!same_pair || (candidates.size() == 2))) {
                break;
            }
        }
        if(pick_a == pick_b) {
            pick_b = candidates.at((std::find(candidates.begin(), candidates.end(), pick_a) - candidates.begin() + 1) % candidates.size());
        }
        this->duel_pos[0] = pick_a;
        this->duel_pos[1] = pick_b;
        this->duel_rolls++;
        this->duel_spin = 0.0;
        this->duel_tick_t = 0.0;
        this->duel_sel = 0;

        for(u32 k = 0; k < 2; k++) {
            auto &entry = this->cur_entries.at(this->shelf_valid.at(this->duel_pos[k]));
            entry.TryLoadNacp();
            this->IconForPos(this->duel_pos[k], true);
            Free(this->duel_names[k]);
            this->duel_names[k] = Text(pu::ui::DefaultFontSize::MediumLarge, entry.control.name.empty() ? nxt::T("untitled") : entry.control.name);
            this->duel_show[k] = this->duel_pos[k];
        }
        Free(this->duel_hint);
        this->duel_hint = Text(pu::ui::DefaultFontSize::Small, nxt::Sub(nxt::T("duel_hint"), "n", std::to_string(this->duel_rolls)));
    }

    void EntryMenu::DuelClose(const bool pick) {
        this->duel_open = false;
        if(pick && (this->duel_pos[this->duel_sel] >= 0)) {
            this->ShelfMoveToPos(this->duel_pos[this->duel_sel]);
            this->shelf_press = 1.0;
            this->NxSfx(2);
            this->Haptic(0.5f, 180.0f, 0.04);
        }
    }

    void EntryMenu::DuelInput(const u64 keys_down, const pu::ui::TouchPoint touch_pos) {
        const auto spinning = this->duel_spin < 1.0;
        if(!touch_pos.IsEmpty()) {
            this->duel_touching = true;
            this->duel_tx = touch_pos.x;
            this->duel_ty = touch_pos.y;
            return;
        }
        if(this->duel_touching) {
            this->duel_touching = false;
            if(spinning) {
                return;
            }
            for(const auto &hit : this->duel_hits) {
                if(Hit(this->duel_tx, this->duel_ty, hit.x, hit.y, hit.w, hit.h)) {
                    if(hit.pos == this->duel_sel) {
                        this->DuelClose(true);
                    }
                    else {
                        this->duel_sel = hit.pos;
                        this->Haptic(0.22f, 260.0f, 0.012);
                    }
                    return;
                }
            }
            this->DuelClose(false);
            return;
        }

        if(keys_down & HidNpadButton_B) {
            this->DuelClose(false);
        }
        else if(spinning) {
            return;
        }
        else if(keys_down & (HidNpadButton_AnyLeft | HidNpadButton_AnyRight)) {
            this->duel_sel = (keys_down & HidNpadButton_AnyLeft) ? 0 : 1;
            this->Haptic(0.22f, 260.0f, 0.012);
        }
        else if(keys_down & HidNpadButton_A) {
            this->DuelClose(true);
        }
        else if(keys_down & HidNpadButton_Y) {
            this->DuelRoll();
        }
        else if(keys_down & HidNpadButton_X) {
            // From the dice to the bracket: you choose who wins
            this->DuelClose(false);
            this->BracketOpen();
        }
    }

    void EntryMenu::RenderDuel(pu::ui::render::Renderer::Ref &drawer) {
        this->duel_anim = this->FxApproach(this->duel_anim, this->duel_open ? 1.0 : 0.0, this->duel_open ? 0.18 : 0.28);
        if(!this->duel_open && (this->duel_anim < 0.004)) {
            if(this->duel_title != nullptr) {
                this->DuelFree();
            }
            this->duel_anim = 0.0;
            this->duel_hits.clear();
            return;
        }
        const auto a = SmoothStep(this->duel_anim);
        const auto al = [&](const double v) {
            return (u8)std::clamp(v * a, 0.0, 255.0);
        };
        const s32 sw = pu::ui::render::ScreenWidth;
        const s32 sh = pu::ui::render::ScreenHeight;
        drawer->RenderRectangleFill({ 0x06, 0x07, 0x0B, al(218.0) }, 0, 0, sw, sh);

        // The reels: covers flick by, slower and slower, until the two picks land
        const auto nv = (s32)this->shelf_valid.size();
        if(this->duel_open && (this->duel_spin < 1.0) && (nv > 0)) {
            this->duel_spin = std::min(1.0, this->duel_spin + this->fx_dt / 1.05);
            this->duel_tick_t -= this->fx_dt;
            if(this->duel_tick_t <= 0.0) {
                this->duel_tick_t = 0.045 + 0.26 * this->duel_spin * this->duel_spin;
                this->duel_tick++;
                for(u32 k = 0; k < 2; k++) {
                    // Only covers that are already loaded take part in the blur
                    for(u32 attempt = 0; attempt < 6; attempt++) {
                        const auto pos = (s32)((this->duel_tick * 7 + k * 13 + attempt * 5 + (u32)this->duel_pos[k]) % (u32)nv);
                        if(this->IconForPos(pos, false) != nullptr) {
                            this->duel_show[k] = pos;
                            break;
                        }
                    }
                }
                this->Haptic(0.18f, 280.0f, 0.010);
            }
            if(this->duel_spin >= 1.0) {
                this->duel_show[0] = this->duel_pos[0];
                this->duel_show[1] = this->duel_pos[1];
                this->Haptic(0.6f, 150.0f, 0.05);
                this->NxSfx(2);
            }
        }
        const auto landed = this->duel_spin >= 1.0;

        Draw(drawer, this->duel_title, (sw - TexW(this->duel_title)) / 2, 78 - (s32)((1.0 - a) * 20.0), a * 255.0);
        if(this->duel_tired) {
            Draw(drawer, this->duel_msg, (sw - TexW(this->duel_msg)) / 2, 150, a * 255.0);
        }

        this->duel_hits.clear();
        const s32 cw = 560;
        const s32 ch = 660;
        const s32 gap = 150;
        const s32 cy = 215;
        for(s32 k = 0; k < 2; k++) {
            const auto selected = landed && (k == this->duel_sel);
            // Cards come in from the sides
            const auto slide = (s32)((1.0 - a) * 140.0) * ((k == 0) ? -1 : 1);
            const auto grow = selected ? 14 : 0;
            const s32 x = sw / 2 + ((k == 0) ? (-gap / 2 - cw) : (gap / 2)) + slide - grow;
            const s32 y = cy - grow;
            const s32 w = cw + 2 * grow;
            const s32 h = ch + 2 * grow;
            if(selected) {
                FillRR(drawer, { 0xFF, 0xFF, 0xFF, al(46.0) }, x - 12, y - 12, w + 24, h + 24, 46);
                FillRR(drawer, { 0xFF, 0xFF, 0xFF, al(235.0) }, x - 3, y - 3, w + 6, h + 6, 39);
            }
            RenderGlass(drawer, x, y, w, h, 36, a, selected ? 235 : 190);
            // The game as a case front
            const s32 art = w - 110;
            const s32 ax = x + 55;
            const s32 ay = y + 48;
            FillRR(drawer, { ShelfCaseColor.r, ShelfCaseColor.g, ShelfCaseColor.b, al(255.0) }, ax - 14, ay - 34, art + 28, art + 48, 12);
            drawer->RenderRectangleFill({ 0xFF, 0xFF, 0xFF, al(90.0) }, ax - 2, ay - 33, art + 4, 1);
            auto icon = this->IconForPos(this->duel_show[k], false);
            if((icon != nullptr) && (icon->Get() != nullptr)) {
                drawer->RenderTexture(icon->Get(), ax, ay, pu::ui::render::TextureRenderOptions(al(landed ? 255.0 : 170.0), art, art, {}, {}, {}));
            }
            else {
                drawer->RenderRectangleFill({ 0x1C, 0x1E, 0x28, al(255.0) }, ax, ay, art, art);
            }
            if(landed) {
                RenderTextFit(drawer, this->duel_names[k], (double)x + (double)w / 2.0, ay + art + 40, (double)(w - 70), al(selected ? 255.0 : 170.0), true);
            }
            this->duel_hits.push_back({ k, x, y, w, h });
        }
        Draw(drawer, this->duel_vs, (sw - TexW(this->duel_vs)) / 2, cy + ch / 2 - 40, a * 170.0);
        Draw(drawer, this->duel_hint, (sw - TexW(this->duel_hint)) / 2, sh - 150, a * 150.0);
    }


    // ---- Bracket: you decide who beats whom ----

    void EntryMenu::BracketFree() {
        Free(this->br_title);
        Free(this->br_sub);
        Free(this->br_hint);
        Free(this->br_vs);
        for(auto &tex : this->br_names) {
            Free(tex);
        }
        this->br_hint_str.clear();
        this->br_title_str.clear();
    }

    void EntryMenu::BracketOpen() {
        std::vector<s32> pool;
        for(s32 pos = 0; pos < (s32)this->shelf_valid.size(); pos++) {
            const auto &entry = this->cur_entries.at(this->shelf_valid.at(pos));
            if(entry.Is<EntryType::Application>() || entry.Is<EntryType::Homebrew>()) {
                pool.push_back(pos);
            }
        }
        if(pool.size() < 4) {
            g_MenuApplication->ShowNotification(nxt::T("br_need"), 2200);
            return;
        }
        u64 seed = armGetSystemTick() | 1;
        for(s32 i = (s32)pool.size() - 1; i > 0; i--) {
            seed ^= seed << 13;
            seed ^= seed >> 7;
            seed ^= seed << 17;
            std::swap(pool.at(i), pool.at(seed % (u64)(i + 1)));
        }
        pool.resize((pool.size() >= 8) ? 8 : 4);
        for(const auto pos : pool) {
            this->cur_entries.at(this->shelf_valid.at(pos)).TryLoadNacp();
            this->IconForPos(pos, true);
        }
        this->BracketFree();
        this->br_vs = Text(pu::ui::DefaultFontSize::Large, nxt::T("br_vs"));
        this->br_rounds.clear();
        this->br_rounds.push_back(pool);
        this->br_rounds.push_back({});
        this->br_round = 0;
        this->br_match = 0;
        this->br_sel = 0;
        this->br_picked = 0;
        this->br_champ = -1;
        this->br_pop = 0.0;
        this->br_anim = std::max(this->br_anim, 0.0);
        this->br_touching = false;
        this->br_open = true;
        this->BracketRefreshMatch();
    }

    void EntryMenu::BracketRefreshMatch() {
        for(s32 k = 0; k < 2; k++) {
            Free(this->br_names[k]);
        }
        if(this->br_champ >= 0) {
            auto &entry = this->cur_entries.at(this->shelf_valid.at(this->br_champ));
            this->br_names[0] = Text(pu::ui::DefaultFontSize::MediumLarge, entry.control.name.empty() ? nxt::T("untitled") : entry.control.name);
        }
        else {
            const auto &round = this->br_rounds.at(this->br_round);
            for(s32 k = 0; k < 2; k++) {
                auto &entry = this->cur_entries.at(this->shelf_valid.at(round.at(this->br_match * 2 + k)));
                this->br_names[k] = Text(pu::ui::DefaultFontSize::MediumLarge, entry.control.name.empty() ? nxt::T("untitled") : entry.control.name);
            }
        }
        Free(this->br_sub);
        if(this->br_champ < 0) {
            const auto matches = this->br_rounds.at(this->br_round).size() / 2;
            this->br_sub = Text(pu::ui::DefaultFontSize::Small, nxt::Sub(nxt::Sub(nxt::T("br_match"), "a", std::to_string(this->br_match + 1)), "b", std::to_string(matches)));
        }
    }

    void EntryMenu::BracketPick() {
        auto &round = this->br_rounds.at(this->br_round);
        const auto winner = round.at(this->br_match * 2 + this->br_sel);
        this->br_rounds.at(this->br_round + 1).push_back(winner);
        this->br_picked++;
        this->br_pop = 1.0;
        this->NxSfx(2);
        this->Haptic(0.5f, 180.0f, 0.04);
        const auto matches = (s32)round.size() / 2;
        if(this->br_match + 1 < matches) {
            this->br_match++;
        }
        else if(this->br_rounds.at(this->br_round + 1).size() == 1) {
            // The final: a champion
            this->br_round++;
            this->br_champ = winner;
            this->Haptic(0.8f, 140.0f, 0.06);
            this->Haptic(0.8f, 200.0f, 0.06, 0.12);
        }
        else {
            this->br_round++;
            this->br_match = 0;
            this->br_rounds.push_back({});
        }
        this->br_sel = 0;
        this->BracketRefreshMatch();
    }

    void EntryMenu::BracketUndo() {
        this->br_sel = 0;
        if(this->br_champ >= 0) {
            this->br_champ = -1;
            this->br_round--;
            this->br_rounds.resize(this->br_round + 2);
            this->br_rounds.at(this->br_round + 1).pop_back();
            this->br_match = (s32)this->br_rounds.at(this->br_round).size() / 2 - 1;
        }
        else if(this->br_match > 0) {
            this->br_match--;
            this->br_rounds.at(this->br_round + 1).pop_back();
        }
        else if(this->br_round > 0) {
            this->br_rounds.resize(this->br_round + 1);
            this->br_round--;
            this->br_match = (s32)this->br_rounds.at(this->br_round).size() / 2 - 1;
            this->br_rounds.at(this->br_round + 1).pop_back();
        }
        else {
            this->br_open = false;
            return;
        }
        this->br_picked = std::max(0, this->br_picked - 1);
        this->NxSfx(0);
        this->BracketRefreshMatch();
    }

    void EntryMenu::BracketPlay() {
        if(this->br_champ < 0) {
            return;
        }
        this->br_open = false;
        this->br_launch_pos = this->br_champ;
        this->br_launch_t = 0.38;
        this->ShelfMoveToPos(this->br_champ);
    }

    void EntryMenu::BracketInput(const u64 keys_down, const pu::ui::TouchPoint touch_pos) {
        if(!touch_pos.IsEmpty()) {
            this->br_touching = true;
            this->br_tx = touch_pos.x;
            this->br_ty = touch_pos.y;
            return;
        }
        if(this->br_touching) {
            this->br_touching = false;
            for(const auto &hit : this->br_hits) {
                if(Hit(this->br_tx, this->br_ty, hit.x, hit.y, hit.w, hit.h)) {
                    if(this->br_champ >= 0) {
                        this->BracketPlay();
                    }
                    else if(hit.pos == this->br_sel) {
                        this->BracketPick();
                    }
                    else {
                        this->br_sel = hit.pos;
                        this->Haptic(0.22f, 260.0f, 0.012);
                    }
                    return;
                }
            }
            return;
        }
        if(keys_down & HidNpadButton_B) {
            this->BracketUndo();
        }
        else if(this->br_champ >= 0) {
            if(keys_down & HidNpadButton_A) {
                this->BracketPlay();
            }
            else if(keys_down & HidNpadButton_Y) {
                this->BracketOpen();
            }
        }
        else if(keys_down & (HidNpadButton_AnyLeft | HidNpadButton_AnyRight)) {
            const auto sel = (keys_down & HidNpadButton_AnyLeft) ? 0 : 1;
            if(sel != this->br_sel) {
                this->br_sel = sel;
                this->NxSfx(0);
                this->Haptic(0.22f, 260.0f, 0.012);
            }
        }
        else if(keys_down & HidNpadButton_A) {
            this->BracketPick();
        }
    }

    void EntryMenu::RenderBracket(pu::ui::render::Renderer::Ref &drawer) {
        // After "Play": let the shelf glide to the champion, then press A for you
        if(this->br_launch_t > 0.0) {
            this->br_launch_t -= this->fx_dt;
            if(this->br_launch_t <= 0.0) {
                const auto nv = (s32)this->shelf_valid.size();
                const auto cur = (nv > 0) ? (((this->shelf_target % nv) + nv) % nv) : -1;
                if((cur == this->br_launch_pos) && !this->IsModalOpen()) {
                    this->shelf_press = 1.0;
                    this->cur_entry_input_cb(HidNpadButton_A);
                }
            }
        }

        this->br_anim = this->FxApproach(this->br_anim, this->br_open ? 1.0 : 0.0, this->br_open ? 0.18 : 0.28);
        if(!this->br_open && (this->br_anim < 0.004)) {
            if(this->br_vs != nullptr) {
                this->BracketFree();
            }
            this->br_anim = 0.0;
            this->br_hits.clear();
            return;
        }
        if(this->br_rounds.empty()) {
            return;
        }
        const auto a = SmoothStep(this->br_anim);
        const auto al = [&](const double v) {
            return (u8)std::clamp(v * a, 0.0, 255.0);
        };
        const s32 sw = pu::ui::render::ScreenWidth;
        const s32 sh = pu::ui::render::ScreenHeight;
        drawer->RenderRectangleFill({ 0x06, 0x07, 0x0B, al(244.0) }, 0, 0, sw, sh);
        this->br_pop *= std::pow(0.86, this->fx_k);
        this->br_hits.clear();

        const auto size = (s32)this->br_rounds.at(0).size();
        const auto levels = (size == 8) ? 4 : 3;

        // Title: the round, or the verdict
        {
            std::string title;
            if(this->br_champ >= 0) {
                title = nxt::T("br_champ");
            }
            else {
                const auto from_end = levels - 1 - this->br_round;
                title = nxt::T((from_end == 1) ? "br_f" : ((from_end == 2) ? "br_s" : "br_q"));
            }
            if(title != this->br_title_str) {
                this->br_title_str = title;
                Free(this->br_title);
                if(this->br_champ >= 0) {
                    this->br_title = pu::ui::render::RenderText(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Medium), title, { ShelfActiveColor.r, ShelfActiveColor.g, ShelfActiveColor.b, 0xFF });
                }
                else {
                    this->br_title = Text(pu::ui::DefaultFontSize::Large, title);
                }
            }
        }
        Draw(drawer, this->br_title, (sw - TexW(this->br_title)) / 2, (this->br_champ >= 0) ? 86 : 64, a * 255.0);
        if((this->br_champ < 0) && (this->br_sub != nullptr)) {
            Draw(drawer, this->br_sub, (sw - TexW(this->br_sub)) / 2, 132, a * 150.0);
        }

        // A case on glass, like the duel
        const auto card = [&](const s32 pos, s32 x, s32 y, const s32 w, const bool sel, const bool gold, pu::sdl2::Texture name) {
            const s32 h = (s32)(w * 1.18);
            const auto grow = sel ? 12 : 0;
            x -= grow;
            y -= grow;
            const auto ww = w + 2 * grow;
            const auto hh = h + 2 * grow;
            if(sel) {
                FillRR(drawer, { 0xFF, 0xFF, 0xFF, al(40.0) }, x - 12, y - 12, ww + 24, hh + 24, 40);
                if(gold) {
                    FillRR(drawer, { ShelfActiveColor.r, ShelfActiveColor.g, ShelfActiveColor.b, al(255.0) }, x - 3, y - 3, ww + 6, hh + 6, 34);
                }
                else {
                    FillRR(drawer, { 0xFF, 0xFF, 0xFF, al(235.0) }, x - 3, y - 3, ww + 6, hh + 6, 34);
                }
            }
            RenderGlass(drawer, x, y, ww, hh, 30, a, sel ? 230 : 170);
            const s32 art = ww - 90;
            const s32 ax = x + 45;
            const s32 ay = y + 40;
            const auto color = gold ? ShelfActiveColor : ShelfCaseColor;
            FillRR(drawer, { color.r, color.g, color.b, al(255.0) }, ax - 12, ay - 26, art + 24, art + 38, 10);
            auto icon = this->IconForPos(pos, false);
            if((icon != nullptr) && (icon->Get() != nullptr)) {
                drawer->RenderTexture(icon->Get(), ax, ay, pu::ui::render::TextureRenderOptions(al(255.0), art, art, {}, {}, {}));
            }
            else {
                drawer->RenderRectangleFill({ 0x1C, 0x1E, 0x28, al(255.0) }, ax, ay, art, art);
            }
            RenderTextFit(drawer, name, (double)x + (double)ww / 2.0, ay + art + 24, (double)(ww - 50), al(sel ? 255.0 : 150.0), true);
            return ShelfHit { 0, x, y, ww, hh };
        };

        if(this->br_champ >= 0) {
            const auto w = 320 + (s32)(30.0 * SmoothStep(std::clamp(1.0 - this->br_pop, 0.0, 1.0)));
            auto hit = card(this->br_champ, (sw - w) / 2, 140, w, true, true, this->br_names[0]);
            this->br_hits.push_back(hit);
            // Gold dust
            for(s32 i = 0; i < 26; i++) {
                const auto an = i * 2.4;
                const auto d = (1.0 - this->br_pop) * 360.0 + 40.0;
                const auto px = sw / 2 + (s32)(std::cos(an) * d);
                const auto py = 340 + (s32)(std::sin(an) * d * 0.6);
                FillRR(drawer, { ShelfActiveColor.r, ShelfActiveColor.g, ShelfActiveColor.b, al(160.0 * this->br_pop) }, px, py, 7, 7, 3);
            }
        }
        else {
            const auto &round = this->br_rounds.at(this->br_round);
            const s32 w = 380;
            const s32 gap = 140;
            const s32 y = 176;
            for(s32 k = 0; k < 2; k++) {
                const auto slide = (s32)((1.0 - a) * 120.0) * ((k == 0) ? -1 : 1);
                const s32 x = sw / 2 + ((k == 0) ? (-gap / 2 - w) : (gap / 2)) + slide;
                auto hit = card(round.at(this->br_match * 2 + k), x, y, w, k == this->br_sel, false, this->br_names[k]);
                hit.pos = k;
                this->br_hits.push_back(hit);
            }
            Draw(drawer, this->br_vs, (sw - TexW(this->br_vs)) / 2, y + (s32)(w * 0.55), a * 150.0);
        }

        // The whole bracket, small, along the bottom
        {
            const s32 bottom = sh - 96;
            const s32 top = 730;
            const s32 row_h = (bottom - top) / (levels - 1);
            const s32 ih = 46;
            const s32 span = sw - 520;
            const auto slot_x = [&](const s32 level, const s32 i) {
                const auto n = size >> level;
                return 260 + (s32)((double)span * ((double)i + 0.5) / (double)n);
            };
            for(s32 level = 0; level < levels; level++) {
                const auto n = size >> level;
                const auto y = bottom - level * row_h;
                const auto have = (level < (s32)this->br_rounds.size()) ? (s32)this->br_rounds.at(level).size() : 0;
                for(s32 i = 0; i < n; i++) {
                    const auto x = slot_x(level, i);
                    const auto filled = i < have;
                    if(level > 0) {
                        // Axis-aligned connector: two stems, a bar, and a stem up
                        const auto xa = slot_x(level - 1, i * 2);
                        const auto xb = slot_x(level - 1, i * 2 + 1);
                        const auto ymid = y + row_h / 2;
                        const pu::ui::Color lc = { 0xFF, 0xFF, 0xFF, al(filled ? 110.0 : 40.0) };
                        drawer->RenderRectangleFill(lc, xa - 1, ymid, 2, (y + row_h - ih / 2) - ymid);
                        drawer->RenderRectangleFill(lc, xb - 1, ymid, 2, (y + row_h - ih / 2) - ymid);
                        drawer->RenderRectangleFill(lc, xa, ymid - 1, xb - xa, 2);
                        drawer->RenderRectangleFill(lc, x - 1, y + ih / 2, 2, ymid - (y + ih / 2));
                    }
                    const auto s = (level == levels - 1) ? ih + 14 : ih;
                    if(filled) {
                        const auto pos = this->br_rounds.at(level).at(i);
                        const auto live = (this->br_champ < 0) && (level == this->br_round) && ((i / 2) == this->br_match);
                        bool out = false;
                        if((level + 1 < (s32)this->br_rounds.size()) && ((i / 2) < (s32)this->br_rounds.at(level + 1).size())) {
                            out = this->br_rounds.at(level + 1).at(i / 2) != pos;
                        }
                        if(live) {
                            FillRR(drawer, { 0xFF, 0xFF, 0xFF, al(((i % 2) == this->br_sel) ? 240.0 : 90.0) }, x - s / 2 - 4, y - s / 2 - 4, s + 8, s + 8, 12);
                        }
                        if(level == levels - 1) {
                            FillRR(drawer, { ShelfActiveColor.r, ShelfActiveColor.g, ShelfActiveColor.b, al(255.0) }, x - s / 2 - 4, y - s / 2 - 4, s + 8, s + 8, 12);
                        }
                        auto icon = this->IconForPos(pos, false);
                        if((icon != nullptr) && (icon->Get() != nullptr)) {
                            drawer->RenderTexture(icon->Get(), x - s / 2, y - s / 2, pu::ui::render::TextureRenderOptions(al(out ? 60.0 : 255.0), s, s, {}, {}, {}));
                        }
                    }
                    else {
                        FillRR(drawer, { 0xFF, 0xFF, 0xFF, al(18.0) }, x - s / 2, y - s / 2, s, s, 9);
                    }
                }
            }
        }

        // One line that says what the buttons do right now
        {
            const auto hint = (this->br_champ >= 0) ? nxt::T("br_hintC") : (this->br_picked > 0 ? nxt::T("br_hint") : nxt::T("br_hint0"));
            if(hint != this->br_hint_str) {
                this->br_hint_str = hint;
                Free(this->br_hint);
                this->br_hint = Text(pu::ui::DefaultFontSize::Small, hint);
            }
            Draw(drawer, this->br_hint, (sw - TexW(this->br_hint)) / 2, sh - 48, a * 140.0);
        }
    }

}
