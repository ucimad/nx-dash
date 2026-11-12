#include <ul/menu/ui/ui_EntryMenu.hpp>
#include <algorithm>
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
            this->ShelfInput(keys_down, keys_held);
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
        this->shelf_valid.clear();
        for(u32 i = 0; i < this->cur_entries.size(); i++) {
            if(!this->cur_entries.at(i).Is<EntryType::Invalid>()) {
                this->shelf_valid.push_back(i);
            }
        }

        if(this->shelf_title_texts.size() != this->cur_entries.size()) {
            this->ShelfClearTitles();
            this->shelf_title_texts.resize(this->cur_entries.size(), nullptr);
        }

        // Keep the shelf position in sync with the real focused entry (e.g. after MoveTo / folder changes)
        const auto nv = (s32)this->shelf_valid.size();
        if(nv > 0) {
            const auto cur_pos = ((this->shelf_target % nv) + nv) % nv;
            if(!this->shelf_synced || (this->shelf_valid.at(cur_pos) != this->cur_entry_idx)) {
                s32 found = 0;
                for(s32 i = 0; i < nv; i++) {
                    if(this->shelf_valid.at(i) == this->cur_entry_idx) {
                        found = i;
                        break;
                    }
                }
                this->shelf_target = found;
                this->shelf_s = (double)found;
                this->shelf_synced = true;
                if(this->shelf_valid.at(found) != this->cur_entry_idx) {
                    this->SetFocusedEntryIndex(this->shelf_valid.at(found));
                }
            }
        }
    }

    void EntryMenu::ShelfMove(const s32 dir) {
        const auto nv = (s32)this->shelf_valid.size();
        if(nv == 0) {
            return;
        }

        const auto prev_idx = (s32)this->cur_entry_idx;
        this->shelf_target += dir;
        const auto pos = ((this->shelf_target % nv) + nv) % nv;
        this->cur_entry_change_started_cb();
        this->SetFocusedEntryIndex(this->shelf_valid.at(pos));
        this->NotifyFocusedEntryChanged(prev_idx);
    }

    void EntryMenu::ShelfInput(const u64 keys_down, const u64 keys_held) {
        const auto left_down = keys_down & HidNpadButton_Left;
        const auto right_down = keys_down & HidNpadButton_Right;
        const auto left_held = keys_held & (HidNpadButton_Left | HidNpadButton_StickLLeft | HidNpadButton_StickRLeft);
        const auto right_held = keys_held & (HidNpadButton_Right | HidNpadButton_StickLRight | HidNpadButton_StickRRight);

        if(left_down || right_down) {
            this->ShelfMove(left_down ? -1 : 1);
            this->shelf_hold_frames = 0;
        }
        else if(left_held || right_held) {
            // Hold to scroll: short delay, then repeat
            this->shelf_hold_frames++;
            if((this->shelf_hold_frames == 1) || ((this->shelf_hold_frames > 18) && ((this->shelf_hold_frames % 4) == 0))) {
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
            if(!(keys_down & (HidNpadButton_Up | HidNpadButton_Down))) {
                this->cur_entry_input_cb(keys_down);
            }
        }
    }

    void EntryMenu::ShelfRender(pu::ui::render::Renderer::Ref &drawer, const s32 x, const s32 y) {
        this->ShelfRebuildValid();
        const auto nv = (s32)this->shelf_valid.size();
        if(nv == 0) {
            return;
        }

        // Ease towards the target (continuous position, so wrapping around stays smooth)
        this->shelf_s += ((double)this->shelf_target - this->shelf_s) * 0.18;
        if(std::abs((double)this->shelf_target - this->shelf_s) < 0.001) {
            this->shelf_s = (double)this->shelf_target;
        }

        const double area_w = (double)this->GetWidth();
        const double area_h = (double)this->GetHeight();
        const double H = area_h * 0.80;
        const double d = H * (474.0 / 760.0);
        const double ws = H * 0.085;
        const double gap = H * 0.014;
        const double U = H * 1.3;
        const double cx = (double)x + area_w / 2.0;
        const double cy = (double)y + area_h * 0.47;

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
            double r = std::fmod((double)p - this->shelf_s, (double)nv);
            if(r < 0) {
                r += nv;
            }
            if(r >= nv / 2.0) {
                r -= nv;
            }
            items.push_back({ p, r });
        }
        // Draw far ones first so near ones overlap them
        std::sort(items.begin(), items.end(), [](const Item &a, const Item &b) {
            return std::abs(a.r) > std::abs(b.r);
        });

        // Soft floor shadow under the focused case
        drawer->RenderShadowSimple((s32)(cx - d * 0.9), (s32)(cy + H / 2.0 - 6.0), (s32)(d * 1.8), 18, 120, 0xFF);

        for(const auto &it : items) {
            const auto r = it.r;
            const auto o = SmoothStep(1.0 - std::abs(r));
            const auto u = r * (ws + gap) + (d - ws + 1.4 * gap) * bump(r);
            if(std::abs(u) > U * 1.95) {
                continue;
            }

            const auto sc = scl(u);
            const auto case_w = (ws + (d - ws) * o) * sc;
            const auto case_h = H * sc;
            const auto sink = H * 0.5 * (1.0 - sc) * 0.42;
            const auto ex = cx + xsc(u) - case_w / 2.0;
            const auto ey = cy - case_h / 2.0 + sink - o * H * 0.02;

            const auto entry_idx = this->shelf_valid.at(it.pos);
            auto &entry = this->cur_entries.at(entry_idx);
            pu::sdl2::TextureHandle::Ref icon = (entry_idx < this->entry_icons.size()) ? this->entry_icons.at(entry_idx) : nullptr;
            if((icon == nullptr) && this->pending_load_img_done && entry.Is<EntryType::Application>()) {
                icon = LoadApplicationIconTexture(entry.app_info.app_id);
                if(icon != nullptr) {
                    entry.TryLoadNacp();
                    this->entry_icons.at(entry_idx) = icon;
                }
            }

            const auto radius = (s32)std::max(2.0, 8.0 * sc);

            // Focus ring
            if(o > 0.6) {
                const auto ring = (u8)(255.0 * (o - 0.6) / 0.4);
                drawer->RenderRoundedRectangleFill({ 0xFF, 0xFF, 0xFF, ring }, (s32)ex - 5, (s32)ey - 5, (s32)case_w + 10, (s32)case_h + 10, radius + 4);
            }

            // Case body
            drawer->RenderRoundedRectangleFill(ShelfCaseColor, (s32)ex, (s32)ey, (s32)case_w, (s32)case_h, radius);

            if(o > 0.35) {
                // Cover: the icon as key art inside the case frame
                const auto a = (u8)(255.0 * std::min(1.0, (o - 0.35) / 0.4));
                const auto pad = case_w * 0.06;
                const auto band = case_h * 0.075;
                const auto art = std::min(case_w - 2.0 * pad, case_h - band - 2.0 * pad);
                if(icon != nullptr) {
                    drawer->RenderTexture(icon->Get(), (s32)(ex + (case_w - art) / 2.0), (s32)(ey + band + pad), pu::ui::render::TextureRenderOptions(a, (s32)art, (s32)art, {}, {}, {}));
                }
            }
            else {
                // Spine: white label, small icon on top, vertical title
                const auto a = (u8)(255.0 * std::min(1.0, (0.35 - o) / 0.35 + 0.0001));
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
                    if(title_tex == nullptr) {
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
            const auto shade = std::min(0.72, o * 0.0 + (1.0 - sc) * 0.62) * (1.0 - o);
            if(shade > 0.01) {
                drawer->RenderRoundedRectangleFill({ 0x05, 0x06, 0x0E, (u8)(255.0 * shade) }, (s32)ex, (s32)ey, (s32)case_w, (s32)case_h, radius);
            }
        }
    }

}
