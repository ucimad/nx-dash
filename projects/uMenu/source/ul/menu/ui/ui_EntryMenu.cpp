#include <ul/menu/ui/ui_NxDashSession.hpp>
#include <ul/menu/bt/bt_Manager.hpp>
#include <ctime>
#include <cstring>
#include <ul/menu/ui/ui_EntryMenu.hpp>
#include <algorithm>
#include <functional>
#include <cmath>
#include <ul/menu/ui/ui_MainMenuLayout.hpp>
#include <ul/menu/ui/ui_MenuApplication.hpp>
#include <ul/menu/smi/smi_Commands.hpp>
#include <ul/util/util_String.hpp>
#include <ul/acc/acc_Accounts.hpp>

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
            this->shelf_cats.push_back({ "Recientes", root, ShelfFilter::Recent });
        }
        this->shelf_cats.push_back({ "Juegos", root, ShelfFilter::Games });
        for(const auto &[name, path] : this->shelf_root_folders) {
            this->shelf_cats.push_back({ name, path, ShelfFilter::Folder });
        }
        if(this->shelf_root_has_hb) {
            this->shelf_cats.push_back({ "Homebrew", root, ShelfFilter::Homebrew });
        }
        if(this->shelf_root_has_special) {
            this->shelf_cats.push_back({ "Sistema", root, ShelfFilter::System });
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
                    static const char *const Tips[] = {
                        "Mantené izquierda o derecha para recorrer el estante de corrido.",
                        "L y R saltan de a cinco juegos.",
                        "El botón menos da vuelta la caja y muestra cuánto jugaste.",
                        "Y arma un duelo entre dos juegos al azar.",
                        "La caja dorada es el juego que quedó en pausa.",
                        "Arriba y abajo cambian de estante.",
                        "X sobre un juego: mover a una categoría o crear una nueva.",
                        "Girá la palanca derecha en círculos y mirá el estante.",
                        "Un juego con 100 aperturas gana borde de coleccionista.",
                        "Después de un minuto quieto, el estante se pasea solo.",
                        "En la pantalla de bloqueo, arriba y abajo cambian el brillo.",
                        "HOME dentro de un juego muestra cuánto llevás jugando.",
                    };
                    const auto count = sizeof(Tips) / sizeof(Tips[0]);
                    this->shelf_press = 1.0;
                    this->Haptic(0.35f, 200.0f, 0.03);
                    g_MenuApplication->ShowNotification(Tips[armGetSystemTick() % count], 3200);
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
                this->shelf_empty_text = pu::ui::render::RenderText(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::MediumLarge), "No hay nada en esta categoría todavía", { 0xFF, 0xFF, 0xFF, 0xB0 });
            }
            const auto tw = pu::ui::render::GetTextureWidth(this->shelf_empty_text);
            const auto th = pu::ui::render::GetTextureHeight(this->shelf_empty_text);
            drawer->RenderTexture(this->shelf_empty_text, x + (this->GetWidth() - tw) / 2, y + (s32)(this->GetHeight() * 0.45) - th / 2);
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
        const double cy = (double)y + area_h * 0.45;

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
                case EntryType::SpecialEntryMiiEdit: return "Editor de Mii";
                case EntryType::SpecialEntryWebBrowser: return "Navegador";
                case EntryType::SpecialEntryUserPage: return "Perfil";
                case EntryType::SpecialEntrySettings: return "Configuración";
                case EntryType::SpecialEntryThemes: return "Temas";
                case EntryType::SpecialEntryControllers: return "Controles";
                case EntryType::SpecialEntryAlbum: return "Álbum";
                case EntryType::SpecialEntryAmiibo: return "amiibo";
                default: return "";
            }
        }

        void ShelfDescribe(Entry &entry, std::string &out_title, std::string &out_meta) {
            if(entry.IsSpecial()) {
                out_title = ShelfSpecialName(entry);
                out_meta = "Sistema";
                return;
            }
            if(entry.Is<EntryType::Folder>()) {
                out_title = entry.folder_info.name;
                out_meta = "Categoría";
                return;
            }
            entry.TryLoadNacp();
            out_title = entry.control.name.empty() ? "Sin título" : entry.control.name;
            std::string meta;
            if(entry.Is<EntryType::Homebrew>()) {
                meta = "Homebrew";
            }
            if(!entry.control.author.empty()) {
                meta += (meta.empty() ? "" : "   ·   ") + entry.control.author;
            }
            if(!entry.control.version.empty()) {
                meta += (meta.empty() ? "" : "   ·   ") + std::string("Versión ") + entry.control.version;
            }
            if(entry.Is<EntryType::Application>() && entry.app_info.IsGameCardInserted()) {
                meta += "   ·   Cartucho";
            }
            if(g_GlobalSettings.IsSuspended() && g_GlobalSettings.IsEntrySuspended(entry)) {
                meta += "   ·   En pausa";
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
            const auto left = x + 110;
            const auto max_right = x + (s32)area_w - 110;
            s32 shift = 0;
            if(left + cur_right > max_right) {
                shift = (left + cur_right) - max_right;
            }
            if(left + cur_left - shift < left) {
                shift = cur_left;
            }
            s32 tx = left - shift;
            const auto ty = y + 18;
            const auto th = 56;
            {
                const auto cap_x = std::max(tx - 8, x + 60);
                const auto cap_r = std::min(tx + total - 14 + 8, x + (s32)area_w - 60);
                RenderGlass(drawer, cap_x, ty - 8, cap_r - cap_x, th + 16, (th + 16) / 2, 1.0, 120);
            }
            {
                // The highlight slides between tabs instead of jumping
                const auto hl_x = (double)(tx + cur_left);
                const auto hl_w = (double)(cur_right - cur_left);
                if(!this->tab_hl_init) {
                    this->tab_hl_init = true;
                    this->tab_hl_x = hl_x;
                    this->tab_hl_w = hl_w;
                }
                this->tab_hl_x = this->FxApproach(this->tab_hl_x, hl_x, 0.26);
                this->tab_hl_w = this->FxApproach(this->tab_hl_w, hl_w, 0.26);
                FillRR(drawer, { 0xFF, 0xFF, 0xFF, 0x3C }, (s32)this->tab_hl_x, ty, (s32)this->tab_hl_w, th, th / 2);
                drawer->RenderRectangleFill({ 0xFF, 0xFF, 0xFF, 0x50 }, (s32)this->tab_hl_x + th / 2, ty + 1, (s32)this->tab_hl_w - th, 1);
            }
            for(u32 i = 0; i < this->shelf_cats.size(); i++) {
                const auto w = widths.at(i);
                const auto is_cur = (s32)i == this->shelf_cat;
                if((tx + w > x) && (tx < x + (s32)area_w)) {
                    auto tex = this->shelf_tab_texts.at(i);
                    const auto tth = pu::ui::render::GetTextureHeight(tex);
                    drawer->RenderTexture(tex, tx + 22, ty + (th - tth) / 2, pu::ui::render::TextureRenderOptions(is_cur ? 0xFF : 0x8C, {}, {}, {}, {}, {}));
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
        const auto ty = y + (s32)(area_h * 0.815);
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
        RenderTextFit(drawer, this->shelf_info_meta, cx, ty + 70 + (s32)((1.0 - settle_late) * 8.0), area_w - 260.0, (u8)(150.0 * settle_late * intro), true);
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
        this->sheet_hint = nullptr;
    }

    void EntryMenu::SheetOpen() {
        this->SheetFree();

        std::string title;
        std::string meta;
        this->DescribeSuspended(title, meta);
        this->FocusSuspendedEntry();

        if(title.empty()) {
            title = "Juego en pausa";
        }
        // The "En pausa" tag is already the badge here
        const std::string paused_tag = "   ·   En pausa";
        if((meta.length() >= paused_tag.length()) && (meta.compare(meta.length() - paused_tag.length(), paused_tag.length(), paused_tag) == 0)) {
            meta.erase(meta.length() - paused_tag.length());
        }

        const auto white = pu::ui::Color{ 0xFF, 0xFF, 0xFF, 0xFF };
        this->sheet_title = pu::ui::render::RenderText(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Large), title, white);
        if(!meta.empty()) {
            this->sheet_meta = pu::ui::render::RenderText(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Medium), meta, white);
        }
        this->sheet_badge = pu::ui::render::RenderText(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Small), "EN PAUSA", white);
        if(const auto played = session::GetSeconds(); played >= 0) {
            this->sheet_time = pu::ui::render::RenderText(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Small), session::Format(played), white);
        }
        this->sheet_hint = pu::ui::render::RenderText(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Small), "A  Elegir        B  Biblioteca        HOME  Volver al juego", white);

        this->sheet_actions = { static_cast<u32>(SheetAction::Resume), static_cast<u32>(SheetAction::Close), static_cast<u32>(SheetAction::Album), static_cast<u32>(SheetAction::Library) };
        const std::vector<std::string> labels = { "Continuar jugando", "Cerrar juego", "Álbum", "Ir a la biblioteca" };
        for(const auto &label : labels) {
            this->sheet_opt_texts.push_back(pu::ui::render::RenderText(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::MediumLarge), label, white));
        }

        this->sheet_sel = 0;
        this->sheet_intro_dim = true;
        this->sheet_sel_y = -1.0;
        this->sheet_anim = 0.0;
        this->sheet_open = true;
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
        }
    }

    void EntryMenu::SheetInput(const u64 keys_down, const pu::ui::TouchPoint touch_pos) {
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
                    if(hit.pos >= 0) {
                        this->sheet_sel = hit.pos;
                        this->SheetExecute(this->sheet_actions.at(hit.pos));
                    }
                    return;
                }
            }
            // Tapped outside the panel: back to the library
            this->sheet_open = false;
            return;
        }

        const auto n = (s32)this->sheet_actions.size();
        if(keys_down & HidNpadButton_AnyUp) {
            this->sheet_sel = (this->sheet_sel + n - 1) % n;
            this->cur_entry_change_started_cb();
        }
        else if(keys_down & HidNpadButton_AnyDown) {
            this->sheet_sel = (this->sheet_sel + 1) % n;
            this->cur_entry_change_started_cb();
        }
        else if(keys_down & HidNpadButton_A) {
            this->SheetExecute(this->sheet_actions.at(this->sheet_sel));
        }
        else if(keys_down & HidNpadButton_B) {
            this->SheetExecute(static_cast<u32>(SheetAction::Library));
        }
    }

    void EntryMenu::RenderOverlay(pu::ui::render::Renderer::Ref &drawer) {
        if(!ShelfMode) {
            return;
        }

        nxdash::g_Modal = this->IsModalOpen();
        this->HapticUpdate();

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
        this->sheet_anim = this->FxApproach(this->sheet_anim, target, this->sheet_open ? 0.20 : 0.26);
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

        const auto a = SmoothStep(this->sheet_anim);
        const auto alpha = [&](const double v) {
            return (u8)std::clamp(v * a, 0.0, 255.0);
        };
        const s32 sw = pu::ui::render::ScreenWidth;
        const s32 sh = pu::ui::render::ScreenHeight;

        drawer->RenderRectangleFill({ 0x06, 0x07, 0x0B, alpha(200.0) }, 0, 0, sw, sh);

        // Glass panel
        const auto pop = 0.95 + 0.05 * a;
        const s32 pw = (s32)(1640.0 * pop);
        const s32 ph = (s32)(780.0 * pop);
        const s32 px = (sw - pw) / 2;
        const s32 py = (sh - ph) / 2;
        RenderGlass(drawer, px, py, pw, ph, 36, a, 205);

        // Game capture: zooms from full screen into the thumbnail
        const double tw = 960.0;
        const double th = 540.0;
        const double tx = px + 60.0;
        const double ty = py + 60.0;
        const auto rx = tx * a;
        const auto ry = ty * a;
        const auto rw = sw + (tw - sw) * a;
        const auto rh = sh + (th - sh) * a;
        FillRR(drawer, { 0xFF, 0xFF, 0xFF, alpha(90.0) }, (s32)rx - 2, (s32)ry - 2, (s32)rw + 4, (s32)rh + 4, 10);
        auto capture = GetScreenCaptureBackground()->GetTexture();
        if(capture != nullptr) {
            drawer->RenderTexture(capture, (s32)rx, (s32)ry, pu::ui::render::TextureRenderOptions(0xFF, (s32)rw, (s32)rh, {}, {}, {}));
        }
        else {
            drawer->RenderRectangleFill({ 0x1C, 0x1E, 0x28, 0xFF }, (s32)rx, (s32)ry, (s32)rw, (s32)rh);
        }
        if(this->sheet_intro_dim && this->sheet_open) {
            // The first thing shown after HOME is the game image dimmed; it brightens as it settles into the panel
            drawer->RenderRectangleFill({ 0x06, 0x07, 0x0B, (u8)(110.0 * (1.0 - a)) }, (s32)rx, (s32)ry, (s32)rw, (s32)rh);
        }
        else {
            this->sheet_intro_dim = false;
        }
        RenderTextFit(drawer, this->sheet_hint, tx, (s32)(ty + th + 44.0), tw, alpha(150.0), false);

        // Right column: badge, title, details, options
        const double cx0 = tx + tw + 64.0;
        const double colw = (px + pw - 60.0) - cx0;
        if(this->sheet_badge != nullptr) {
            const auto bw = pu::ui::render::GetTextureWidth(this->sheet_badge);
            const auto bh = pu::ui::render::GetTextureHeight(this->sheet_badge);
            FillRR(drawer, { ShelfCaseColor.r, ShelfCaseColor.g, ShelfCaseColor.b, alpha(255.0) }, (s32)cx0, (s32)ty, bw + 28, bh + 12, (bh + 12) / 2);
            drawer->RenderTexture(this->sheet_badge, (s32)cx0 + 14, (s32)ty + 6, pu::ui::render::TextureRenderOptions(alpha(255.0), {}, {}, {}, {}, {}));
            RenderTextFit(drawer, this->sheet_time, cx0 + bw + 28.0 + 18.0, (s32)ty + 6, colw - (bw + 46.0), alpha(190.0), false);
        }
        RenderTextFit(drawer, this->sheet_title, cx0, (s32)(ty + 62.0), colw, alpha(255.0), false);
        RenderTextFit(drawer, this->sheet_meta, cx0, (s32)(ty + 128.0), colw, alpha(165.0), false);

        this->sheet_hits.clear();
        this->sheet_hits.push_back({ -1, px, py, pw, ph });
        const s32 row_h = 76;
        const s32 row_gap = 12;
        s32 oy = (s32)(ty + 210.0);
        {
            // The selection glides between rows
            const auto sel_y = (double)(oy + this->sheet_sel * (row_h + row_gap));
            if(this->sheet_sel_y < 0.0) {
                this->sheet_sel_y = sel_y;
            }
            this->sheet_sel_y = this->FxApproach(this->sheet_sel_y, sel_y, 0.30);
            FillRR(drawer, { 0xFF, 0xFF, 0xFF, alpha(46.0) }, (s32)cx0, (s32)this->sheet_sel_y, (s32)colw, row_h, 18);
        }
        for(u32 i = 0; i < this->sheet_opt_texts.size(); i++) {
            const auto sel = (s32)i == this->sheet_sel;
            // Rows arrive one after another
            const auto row_p = SmoothStep(std::clamp(this->sheet_anim * 1.7 - 0.25 - (double)i * 0.11, 0.0, 1.0));
            auto tex = this->sheet_opt_texts.at(i);
            const auto tth = pu::ui::render::GetTextureHeight(tex);
            drawer->RenderTexture(tex, (s32)cx0 + 28 + (s32)((1.0 - row_p) * 22.0), oy + (row_h - tth) / 2, pu::ui::render::TextureRenderOptions((u8)((sel ? 255.0 : 150.0) * row_p * (this->sheet_open ? 1.0 : a)), {}, {}, {}, {}, {}));
            this->sheet_hits.insert(this->sheet_hits.begin(), { (s32)i, (s32)cx0, oy, (s32)colw, row_h });
            oy += row_h + row_gap;
        }
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

        inline bool Hit(const s32 px, const s32 py, const s32 x, const s32 y, const s32 w, const s32 h) {
            return (px >= x) && (px < x + w) && (py >= y) && (py < y + h);
        }

        std::string FormatPlayed(const u64 seconds) {
            const auto mins = seconds / 60;
            if(mins < 1) {
                return "menos de 1 min";
            }
            if(mins < 60) {
                return std::to_string(mins) + " min";
            }
            return std::to_string(mins / 60) + " h " + std::to_string(mins % 60) + " min";
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
            if(out_title == "Sin título") {
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
        const double cy = (double)y + area_h * 0.45;
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
        const auto ty = y + (s32)(area_h * 0.815);
        FillRR(drawer, { 0xFF, 0xFF, 0xFF, (u8)(22.0 + 16.0 * pulse(1.0)) }, (s32)(cx - 260.0), ty + 8, 520, 34, 17);
        FillRR(drawer, { 0xFF, 0xFF, 0xFF, (u8)(14.0 + 12.0 * pulse(2.0)) }, (s32)(cx - 170.0), ty + 72, 340, 22, 11);
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
            const auto rc1 = hidInitializeVibrationDevices(this->hap_handles, 2, HidNpadIdType_Handheld, HidNpadStyleTag_NpadHandheld);
            const auto rc2 = hidInitializeVibrationDevices(this->hap_handles + 2, 2, HidNpadIdType_No1, HidNpadStyleTag_NpadJoyDual);
            this->hap_ok = R_SUCCEEDED(rc1) && R_SUCCEEDED(rc2);
        }
        if(!this->hap_ok) {
            this->hap_queue.clear();
            this->hap_off_at = -1.0;
            return;
        }

        const auto send = [&](const float amp, const float freq) {
            HidVibrationValue values[4];
            for(auto &value : values) {
                value.amp_low = amp;
                value.freq_low = freq;
                value.amp_high = amp * 0.6f;
                value.freq_high = freq * 2.0f;
            }
            hidSendVibrationValues(this->hap_handles, values, 4);
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
        this->RenderLock(drawer);
    }

    // ---- Lock screen ----

    namespace {

        constexpr u32 LockClockFontSize = 150;

        std::string LockDateString() {
            static const char *const Days[7] = { "domingo", "lunes", "martes", "miércoles", "jueves", "viernes", "sábado" };
            static const char *const Months[12] = { "enero", "febrero", "marzo", "abril", "mayo", "junio", "julio", "agosto", "septiembre", "octubre", "noviembre", "diciembre" };
            const auto now = ::time(nullptr);
            const auto lt = localtime(&now);
            if(lt == nullptr) {
                return "";
            }
            return std::string(Days[lt->tm_wday % 7]) + " " + std::to_string(lt->tm_mday) + " de " + Months[lt->tm_mon % 12];
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
        this->lock_hint_tex = Text(pu::ui::DefaultFontSize::Small, "Apretá 3 veces el mismo botón");
        this->lock_ctl_texs[0] = Text(pu::ui::DefaultFontSize::Small, "Brillo");
        this->lock_ctl_texs[1] = Text(pu::ui::DefaultFontSize::Small, "Volumen");

        std::vector<std::string> labels;
        if(g_GlobalSettings.IsSuspended()) {
            std::string title;
            std::string meta;
            this->DescribeSuspended(title, meta);
            this->lock_title_tex = Text(pu::ui::DefaultFontSize::MediumLarge, title.empty() ? "Juego en pausa" : title);
            this->lock_badge_tex = Text(pu::ui::DefaultFontSize::Small, "EN PAUSA");
            if(const auto played = session::GetSeconds(); played >= 0) {
                this->lock_played_tex = Text(pu::ui::DefaultFontSize::Small, session::Format(played));
            }
            this->lock_opt_keys = { HidNpadButton_A, HidNpadButton_X, HidNpadButton_Y };
            labels = { "Seguir jugando", "Biblioteca", "Álbum" };
        }
        else {
            this->lock_opt_keys = { HidNpadButton_A, HidNpadButton_Y };
            labels = { "Biblioteca", "Álbum" };
        }
        for(u32 i = 0; i < labels.size(); i++) {
            this->lock_opt_texs.push_back(Text(pu::ui::DefaultFontSize::Medium, labels.at(i)));
            const auto key = this->lock_opt_keys.at(i);
            this->lock_key_texs.push_back(pu::ui::render::RenderText(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::MediumLarge), (key == HidNpadButton_A) ? "A" : ((key == HidNpadButton_X) ? "X" : "Y"), { 0x12, 0x12, 0x16, 0xFF }));
        }

        this->LockRefreshBluetooth(true);
        this->lock_btn = 0;
        this->lock_count = 0;
        this->lock_pop = 0.0;
        this->lock_drag = -1;
        this->lock_touching = false;
        this->lock_out = -1.0;
        this->lock_anim = 0.0;
        this->lock_open = true;
    }

    void EntryMenu::LockRefreshBluetooth(const bool force) {
        this->lock_bt_poll += this->fx_dt;
        if(!force && (this->lock_bt_poll < 1.0)) {
            return;
        }
        this->lock_bt_poll = 0.0;
        const auto dev = bt::GetConnectedAudioDevice();
        const std::string name = (dev.name[0] != '\0') ? std::string(dev.name, strnlen(dev.name, sizeof(dev.name))) : "";
        const auto label = name.empty() ? std::string("Bluetooth") : name;
        if(force || (label != this->lock_bt_str) || (this->lock_bt_tex == nullptr)) {
            this->lock_bt_str = label;
            Free(this->lock_bt_tex);
            Free(this->lock_bt_sub_tex);
            this->lock_bt_tex = Text(pu::ui::DefaultFontSize::Medium, label);
            this->lock_bt_sub_tex = Text(pu::ui::DefaultFontSize::Small, name.empty() ? "L para conectar" : "Conectado · L para desconectar");
        }
    }

    void EntryMenu::LockBluetooth() {
        const auto dev = bt::GetConnectedAudioDevice();
        if(dev.name[0] != '\0') {
            bt::DisconnectAudioDevice(dev);
            g_MenuApplication->ShowNotification("Desconectando el audio Bluetooth…", 1800);
        }
        else {
            const auto paired = bt::ListPairedAudioDevices();
            if(paired.empty()) {
                g_MenuApplication->ShowNotification("Primero emparejá un dispositivo en Configuración → Bluetooth", 3000);
            }
            else {
                const auto &target = paired.front();
                bt::ConnectAudioDevice(target);
                g_MenuApplication->ShowNotification("Conectando a " + std::string(target.name, strnlen(target.name, sizeof(target.name))) + "…", 2200);
            }
        }
        this->Haptic(0.3f, 200.0f, 0.02);
        this->lock_bt_poll = 0.4;
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
                    if((hit.pos >= 20) && Hit(touch_pos.x, touch_pos.y, hit.x - 30, hit.y - 30, hit.w + 60, hit.h + 60)) {
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
                    if(Hit(this->lock_tx, this->lock_ty, hit.x, hit.y, hit.w, hit.h)) {
                        if(hit.pos < (s32)this->lock_opt_keys.size()) {
                            this->LockPress(this->lock_opt_keys.at(hit.pos));
                        }
                        else if(hit.pos == 10) {
                            this->LockBluetooth();
                        }
                        break;
                    }
                }
            }
            this->lock_drag = -1;
            return;
        }

        if(keys_down & HidNpadButton_A) {
            this->LockPress(HidNpadButton_A);
        }
        else if(keys_down & HidNpadButton_X) {
            this->LockPress(HidNpadButton_X);
        }
        else if(keys_down & HidNpadButton_Y) {
            this->LockPress(HidNpadButton_Y);
        }
        else if(keys_down & HidNpadButton_AnyUp) {
            this->LockSetBrightness(this->lock_bright + 0.1f);
        }
        else if(keys_down & HidNpadButton_AnyDown) {
            this->LockSetBrightness(this->lock_bright - 0.1f);
        }
        else if(keys_down & HidNpadButton_AnyRight) {
            this->LockSetVolume(this->lock_vol + 1);
        }
        else if(keys_down & HidNpadButton_AnyLeft) {
            this->LockSetVolume(this->lock_vol - 1);
        }
        else if(keys_down & HidNpadButton_L) {
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

        // Clock (re-rendered when the minute changes)
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
            const auto dw = TexW(this->lock_time_tex) * k;
            const auto dh = th * k;
            drawer->RenderTexture(this->lock_time_tex, (s32)(((double)sw - dw) / 2.0), 96 + lift, pu::ui::render::TextureRenderOptions((u8)al(255.0), (s32)dw, (s32)dh, {}, {}, {}));
        }
        Draw(drawer, this->lock_date_tex, (sw - TexW(this->lock_date_tex)) / 2, 300 + lift, al(205.0));

        // Paused game card
        if(this->lock_title_tex != nullptr) {
            const s32 cw = 820;
            const s32 ch = 150;
            const s32 cx = (sw - cw) / 2;
            const s32 cy = 392 + lift;
            RenderGlass(drawer, cx, cy, cw, ch, 30, vis, 150);
            s32 tx = cx + 36;
            if(capture != nullptr) {
                FillRR(drawer, { 0xFF, 0xFF, 0xFF, (u8)al(90.0) }, cx + 22, cy + 22, 192, 108, 8);
                drawer->RenderTexture(capture, cx + 24, cy + 24, pu::ui::render::TextureRenderOptions((u8)al(255.0), 188, 104, {}, {}, {}));
                tx = cx + 240;
            }
            const auto bw = TexW(this->lock_badge_tex);
            FillRR(drawer, { ShelfActiveColor.r, ShelfActiveColor.g, ShelfActiveColor.b, (u8)al(255.0) }, tx, cy + 26, bw + 28, 42, 21);
            Draw(drawer, this->lock_badge_tex, tx + 14, cy + 32, al(255.0));
            Draw(drawer, this->lock_played_tex, tx + bw + 46, cy + 32, al(190.0));
            RenderTextFit(drawer, this->lock_title_tex, (double)tx, cy + 82, (double)(cx + cw - 30 - tx), (u8)al(255.0), false);
        }

        // Controls: Bluetooth, brightness, volume
        this->lock_hits.clear();
        this->LockRefreshBluetooth(false);
        {
            const s32 bw = 420;
            const s32 slw = 400;
            const s32 h = 92;
            const s32 gap = 24;
            s32 x = (sw - (bw + 2 * slw + 2 * gap)) / 2;
            const s32 y = 590 + lift;
            const auto connected = this->lock_bt_str != "Bluetooth";
            RenderGlass(drawer, x, y, bw, h, h / 2, vis, connected ? 200 : 130);
            if(connected) {
                FillRR(drawer, { 0x3B, 0x82, 0xF6, (u8)al(150.0) }, x, y, bw, h, h / 2);
            }
            RenderTextFit(drawer, this->lock_bt_tex, (double)(x + 40), y + 10, (double)(bw - 70), (u8)al(255.0), false);
            RenderTextFit(drawer, this->lock_bt_sub_tex, (double)(x + 40), y + 50, (double)(bw - 70), (u8)al(160.0), false);
            this->lock_hits.push_back({ 10, x, y, bw, h });
            x += bw + gap;

            for(s32 i = 0; i < 2; i++) {
                const auto ok = (i == 0) ? this->lock_lbl_ok : this->lock_aud_ok;
                const auto v = (i == 0) ? (double)this->lock_bright : ((double)this->lock_vol / (double)std::max(1, this->lock_vol_max));
                RenderGlass(drawer, x, y, slw, h, h / 2, vis, 130);
                Draw(drawer, this->lock_ctl_texs[i], x + 36, y + 10, al(ok ? 200.0 : 90.0));
                const s32 tx = x + 40;
                const s32 tw = slw - 80;
                const s32 ty = y + 62;
                FillRR(drawer, { 0xFF, 0xFF, 0xFF, (u8)al(50.0) }, tx, ty - 5, tw, 10, 5);
                if(ok) {
                    FillRR(drawer, { 0xFF, 0xFF, 0xFF, (u8)al(235.0) }, tx, ty - 5, std::max(10, (s32)(tw * v)), 10, 5);
                    const auto knob = (this->lock_drag == 20 + i) ? 17 : 13;
                    FillRR(drawer, { 0xFF, 0xFF, 0xFF, (u8)al(255.0) }, tx + (s32)(tw * v) - knob, ty - knob, knob * 2, knob * 2, knob);
                    this->lock_hits.push_back({ 20 + i, tx, ty - 20, tw, 40 });
                }
                x += slw + gap;
            }
        }

        // Options: the same button three times
        this->lock_pop *= std::pow(0.86, this->fx_k);
        if((this->lock_btn != 0) && (this->lock_out < 0.0) && ((this->fx_time - this->lock_last) > 1.2)) {
            this->lock_btn = 0;
            this->lock_count = 0;
        }
        {
            const s32 ow = 430;
            const s32 oh = 112;
            const s32 gap = 28;
            const auto n = (s32)this->lock_opt_texs.size();
            s32 ox = (sw - (n * ow + (n - 1) * gap)) / 2;
            const s32 oy = 800 + lift;
            Draw(drawer, this->lock_hint_tex, (sw - TexW(this->lock_hint_tex)) / 2, oy - 58, al(150.0));
            for(s32 i = 0; i < n; i++) {
                const auto active = this->lock_btn == this->lock_opt_keys.at(i);
                const auto count = active ? this->lock_count : 0;
                const auto grow = active ? (0.04 * this->lock_pop) : 0.0;
                const auto w = (s32)(ow * (1.0 + grow));
                const auto h = (s32)(oh * (1.0 + grow));
                const auto x = ox - (w - ow) / 2;
                const auto y = oy - (h - oh) / 2;
                RenderGlass(drawer, x, y, w, h, h / 2, vis, active ? 195 : 130);
                if(active) {
                    FillRR(drawer, { 0xFF, 0xFF, 0xFF, (u8)al(30.0 + 40.0 * this->lock_pop) }, x, y, w, h, h / 2);
                }
                FillRR(drawer, { 0xFF, 0xFF, 0xFF, (u8)al(235.0) }, x + 28, y + h / 2 - 30, 60, 60, 30);
                auto key_tex = this->lock_key_texs.at(i);
                Draw(drawer, key_tex, x + 58 - TexW(key_tex) / 2, y + h / 2 - TexH(key_tex) / 2, al(255.0));
                Draw(drawer, this->lock_opt_texs.at(i), x + 110, y + h / 2 - 34, al(255.0));
                for(s32 dot = 0; dot < 3; dot++) {
                    const auto filled = dot < (s32)count;
                    const auto r = filled ? 9 : 7;
                    FillRR(drawer, { 0xFF, 0xFF, 0xFF, (u8)al(filled ? 255.0 : 70.0) }, x + 116 + dot * 30 - r, y + h / 2 + 24 - r, r * 2, r * 2, r);
                }
                this->lock_hits.push_back({ i, x, y, w, h });
                ox += ow + gap;
            }
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
        this->flip_title = Text(pu::ui::DefaultFontSize::Large, entry.control.name.empty() ? "Sin título" : entry.control.name);
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

            lines.push_back((total_secs > 0) ? ("Jugaste " + FormatPlayed(total_secs)) : "Todavía sin tiempo de juego registrado");
            s64 days_ago = own.days_since_last;
            if(last_posix > 0) {
                const auto diff = (s64)::time(nullptr) - last_posix;
                const auto console_days = (diff > 0) ? (diff / 86400) : 0;
                if((days_ago < 0) || (console_days < days_ago)) {
                    days_ago = console_days;
                }
            }
            if(days_ago == 0) {
                lines.push_back("Última vez: hoy");
            }
            else if(days_ago == 1) {
                lines.push_back("Última vez: ayer");
            }
            else if(days_ago > 1) {
                lines.push_back("Última vez: hace " + std::to_string(days_ago) + " días");
            }
            if(own.streak >= 2) {
                lines.push_back("Racha: " + std::to_string(own.streak) + " días seguidos");
            }
            else if(own.streak == 1) {
                lines.push_back("Racha: 1 día. Volvé mañana para sumar.");
            }
            else {
                lines.push_back("Sin racha activa");
            }
            if(launches > 0) {
                lines.push_back("Lo abriste " + std::to_string(launches) + ((launches == 1) ? " vez" : " veces"));
            }
            if(!entry.control.author.empty()) {
                lines.push_back(entry.control.author + (entry.control.version.empty() ? "" : ("  ·  Versión " + entry.control.version)));
            }
        }
        else {
            lines.push_back("Homebrew");
            if(!entry.control.author.empty()) {
                lines.push_back(entry.control.author);
            }
            if(!entry.control.version.empty()) {
                lines.push_back("Versión " + entry.control.version);
            }
        }
        for(const auto &line : lines) {
            this->flip_lines.push_back(Text(pu::ui::DefaultFontSize::Medium, line));
        }

        static const char *const DayLetters[7] = { "D", "L", "M", "M", "J", "V", "S" };
        const auto now = ::time(nullptr);
        const auto lt = localtime(&now);
        const auto today_wd = (lt != nullptr) ? lt->tm_wday : 0;
        for(s32 i = 0; i < 7; i++) {
            this->flip_days[i] = Text(pu::ui::DefaultFontSize::Small, DayLetters[(((today_wd - (6 - i)) % 7) + 7) % 7]);
        }
        this->flip_chart_title = Text(pu::ui::DefaultFontSize::Small, "Últimos 7 días");
        this->flip_hint = Text(pu::ui::DefaultFontSize::Small, "A  Jugar          X  Opciones          −  Volver");

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
            g_MenuApplication->ShowNotification("Hacen falta al menos dos juegos en este estante", 2200);
            return;
        }
        this->DuelFree();
        this->duel_title = Text(pu::ui::DefaultFontSize::Large, "¿A qué juego?");
        this->duel_vs = Text(pu::ui::DefaultFontSize::Large, "o");
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
                static const char *const Lines[] = {
                    "El dado se cansó. Es uno de estos dos, sin vueltas.",
                    "Tres tiradas y ni una te convenció. Elegí con el corazón.",
                    "No hay cuarta. El destino ya habló dos veces.",
                    "El dado pidió vacaciones. Te quedás con estos.",
                };
                Free(this->duel_msg);
                this->duel_msg = pu::ui::render::RenderText(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Medium), Lines[armGetSystemTick() % (sizeof(Lines) / sizeof(Lines[0]))], { ShelfActiveColor.r, ShelfActiveColor.g, ShelfActiveColor.b, 0xFF });
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
            this->duel_names[k] = Text(pu::ui::DefaultFontSize::MediumLarge, entry.control.name.empty() ? "Sin título" : entry.control.name);
            this->duel_show[k] = this->duel_pos[k];
        }
        Free(this->duel_hint);
        this->duel_hint = Text(pu::ui::DefaultFontSize::Small, "Izquierda / derecha  Elegir          A  Confirmar          Y  Tirar de nuevo (" + std::to_string(this->duel_rolls) + " de 3)          B  Salir");
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

}
