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

        // Pre-rendered shapes: gfx rounded boxes are drawn line by line and were the main per-frame cost
        SDL_Texture *g_Corners[4] = {};
        SDL_Texture *g_SoftShadow = nullptr;
        SDL_Texture *g_Sheen = nullptr;
        SDL_Texture *g_FadeDown = nullptr;
        SDL_Texture *g_FadeUp = nullptr;
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
            for(const auto &entry : *root_entries) {
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
        if(this->sheet_open) {
            this->SheetInput(keys_down, touch_pos);
            return;
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
        const auto left_down = keys_down & HidNpadButton_Left;
        const auto right_down = keys_down & HidNpadButton_Right;
        this->shelf_held = this->shelf_hold_frames > 27;
        const auto left_held = keys_held & (HidNpadButton_Left | HidNpadButton_StickLLeft | HidNpadButton_StickRLeft);
        const auto right_held = keys_held & (HidNpadButton_Right | HidNpadButton_StickLRight | HidNpadButton_StickRRight);
        const auto up_down = keys_down & (HidNpadButton_Up | HidNpadButton_StickLUp | HidNpadButton_StickRUp);
        const auto down_down = keys_down & (HidNpadButton_Down | HidNpadButton_StickLDown | HidNpadButton_StickRDown);

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
        else {
            this->shelf_hold_frames = 0;
            auto fwd = keys_down & ~(u64)(HidNpadButton_Up | HidNpadButton_Down | HidNpadButton_AnyUp | HidNpadButton_AnyDown);
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
        this->ShelfRebuildValid();
        const auto nv = (s32)this->shelf_valid.size();

        // Category change slide
        this->shelf_intro += this->fx_dt;

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
        this->shelf_s = this->FxApproach(this->shelf_s, (double)this->shelf_target, this->shelf_held ? 0.42 : 0.20);
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

        // Soft floor shadow under the focused case: grows as the case lifts
        if(g_SoftShadow != nullptr) {
            const auto sh_w = d * (1.55 + 0.25 * settled);
            const auto sh_h = H * 0.085;
            drawer->RenderTexture(g_SoftShadow, (s32)(cx - sh_w / 2.0), (s32)(cy + H / 2.0 - sh_h * 0.42), pu::ui::render::TextureRenderOptions((u8)(150.0 * intro_of(0.0)), (s32)sh_w, (s32)sh_h, {}, {}, {}));
        }

        u32 loads_left = 2;

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
            const auto case_h = H * sc * press_k;
            const auto sink = H * 0.5 * (1.0 - sc) * 0.42;
            const auto fade = intro_of(r);
            const auto bob = std::sin(this->fx_time * (2.0 * M_PI / 4.5)) * 3.0 * o * settled;
            const auto ex = cx + xsc(u) - case_w / 2.0;
            const auto ey = cy - case_h / 2.0 + sink - o * H * 0.02 + bob + (1.0 - fade) * 46.0 * this->shelf_intro_dir;
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

            // Case body
            FillRR(drawer, { ShelfCaseColor.r, ShelfCaseColor.g, ShelfCaseColor.b, (u8)(255.0 * fade) }, (s32)ex, (s32)ey, (s32)case_w, (s32)case_h, radius);
            if(case_w > 2.0 * radius + 4.0) {
                drawer->RenderRectangleFill({ 0xFF, 0xFF, 0xFF, (u8)(90.0 * fade) }, (s32)ex + radius, (s32)ey + 1, (s32)case_w - 2 * radius, 1);
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
                if((icon != nullptr) && (in_w > 4.0) && (in_h > 4.0)) {
                    // Full-bleed sleeve: the icon blown up behind (cropped to the sleeve, dimmed), the sharp icon on top
                    const SDL_Rect clip = { (s32)in_x, (s32)in_y, (s32)in_w, (s32)in_h };
                    SDL_RenderSetClipRect(pu::ui::render::GetMainRenderer(), &clip);
                    drawer->RenderTexture(icon->Get(), (s32)(in_x + (in_w - in_h) / 2.0), (s32)in_y, pu::ui::render::TextureRenderOptions(a, (s32)in_h, (s32)in_h, {}, {}, {}));
                    SDL_RenderSetClipRect(pu::ui::render::GetMainRenderer(), nullptr);
                    drawer->RenderRectangleFill({ 0x08, 0x09, 0x10, (u8)(a * 0.50) }, (s32)in_x, (s32)in_y, (s32)in_w, (s32)in_h);
                    const auto art = in_w * 0.86;
                    drawer->RenderTexture(icon->Get(), (s32)(in_x + (in_w - art) / 2.0), (s32)(in_y + (in_h - art) / 2.0), pu::ui::render::TextureRenderOptions(a, (s32)art, (s32)art, {}, {}, {}));
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
        this->sheet_hint = nullptr;
    }

    void EntryMenu::SheetOpen() {
        this->SheetFree();

        std::string title;
        std::string meta;
        bool found = false;

        // Is the paused game on the current shelf? Then focus it, too.
        for(u32 i = 0; i < this->cur_entries.size(); i++) {
            auto &entry = this->cur_entries.at(i);
            if((entry.Is<EntryType::Application>() || entry.Is<EntryType::Homebrew>()) && g_GlobalSettings.IsEntrySuspended(entry)) {
                ShelfDescribe(entry, title, meta);
                found = true;
                if(this->IsInRoot() && entry.Is<EntryType::Homebrew>()) {
                    this->shelf_root_filter = ShelfFilter::Homebrew;
                    this->shelf_cats_sig.clear();
                }
                if(i != this->cur_entry_idx) {
                    const auto prev_idx = (s32)this->cur_entry_idx;
                    this->SetFocusedEntryIndex(i);
                    this->NotifyFocusedEntryChanged(prev_idx);
                }
                this->shelf_synced = false;
                break;
            }
        }

        // Otherwise look for it in the root and in every category
        if(!found) {
            const auto &root = GetActiveMenuPath();
            auto search = [&](std::vector<Entry> entries) {
                for(auto &entry : entries) {
                    if((entry.Is<EntryType::Application>() || entry.Is<EntryType::Homebrew>()) && g_GlobalSettings.IsEntrySuspended(entry)) {
                        ShelfDescribe(entry, title, meta);
                        return true;
                    }
                }
                return false;
            };
            auto root_entries = LoadEntries(root);
            found = search(root_entries);
            for(const auto &entry : root_entries) {
                if(found) {
                    break;
                }
                if(entry.Is<EntryType::Folder>()) {
                    found = search(LoadEntries(entry.GetFolderPath()));
                }
            }
        }

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
        this->sheet_hint = pu::ui::render::RenderText(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Small), "A  Elegir        B  Biblioteca        HOME  Volver al juego", white);

        this->sheet_actions = { static_cast<u32>(SheetAction::Resume), static_cast<u32>(SheetAction::Close), static_cast<u32>(SheetAction::Album), static_cast<u32>(SheetAction::Library) };
        const std::vector<std::string> labels = { "Continuar jugando", "Cerrar juego", "Álbum", "Ir a la biblioteca" };
        for(const auto &label : labels) {
            this->sheet_opt_texts.push_back(pu::ui::render::RenderText(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::MediumLarge), label, white));
        }

        this->sheet_sel = 0;
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

        // Coming back from a game with HOME: show the game sheet first
        if(!this->sheet_checked) {
            this->sheet_checked = true;
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
        RenderTextFit(drawer, this->sheet_hint, tx, (s32)(ty + th + 44.0), tw, alpha(150.0), false);

        // Right column: badge, title, details, options
        const double cx0 = tx + tw + 64.0;
        const double colw = (px + pw - 60.0) - cx0;
        if(this->sheet_badge != nullptr) {
            const auto bw = pu::ui::render::GetTextureWidth(this->sheet_badge);
            const auto bh = pu::ui::render::GetTextureHeight(this->sheet_badge);
            FillRR(drawer, { ShelfCaseColor.r, ShelfCaseColor.g, ShelfCaseColor.b, alpha(255.0) }, (s32)cx0, (s32)ty, bw + 28, bh + 12, (bh + 12) / 2);
            drawer->RenderTexture(this->sheet_badge, (s32)cx0 + 14, (s32)ty + 6, pu::ui::render::TextureRenderOptions(alpha(255.0), {}, {}, {}, {}, {}));
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

}
