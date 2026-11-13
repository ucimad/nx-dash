
#pragma once
#include <ul/menu/menu_Entries.hpp>
#include <ul/ul_Result.hpp>
#include <ul/util/util_Enum.hpp>
#include <pu/Plutonium>
#include <map>
#include <cmath>

namespace ul::menu::ui {

    class EntryMenu : public pu::ui::elm::Element {
        public:
            enum class SwipeMode {
                None,
                LeftSingle,
                RightSingle,
                LeftPage,
                RightPage,
                Rewind
            };

            static constexpr u32 HorizontalSideMargin = 60;
            static constexpr u32 VerticalSideMargin = 35;
            static constexpr u32 EntryVerticalMargin = 32;
            static constexpr u32 CursorTransitionIncrementSteps = 6;

            static constexpr u32 DefaultEntrySize = 256;
            static constexpr u32 MinScalableEntrySize = DefaultEntrySize * 1.5; // Equivalent to keeping 256x256 in 1280x720 resolution (otherwise they would be too small in 1080p)
            static constexpr u32 V2MinScalableEntrySize = DefaultEntrySize; // Old scale size used in theme v2

            static constexpr u32 DefaultOverTextSideMargin = 15;
            static constexpr u32 DefaultProgressBarHeight = 25;
            static constexpr pu::ui::Color ProgressBackgroundColor = { 0, 0, 0, 215 };
            static constexpr pu::ui::Color ProgressForegroundColor = { 0, 210, 210, 215 };
            static constexpr u8 MoveOverIconAlpha = 220;
            static constexpr u32 DefaultMoveOverIconOffset = 8;
            static constexpr u32 EntryShowIncrementSteps = 4;
            static constexpr u32 EntriesSwipePageIncrementSteps = 20;
            static constexpr u32 EntriesSwipeSingleIncrementSteps = 6;
            static constexpr u32 EntriesSwipeRewindIncrementSteps = 36;

            using FocusedEntryInputPressedCallback = std::function<void(const u64)>;
            using FocusedEntryChangedCallback = std::function<void(const bool, const bool, const bool)>;
            using FocusedEntryChangeStartedCallback = std::function<void()>;

            // NX Dash callbacks into the main layout
            enum class SheetAction : u32 { Resume = 0, Close = 1, Album = 2, Library = 3 };
            using SheetActionCallback = std::function<void(const SheetAction)>;
            using ShelfFolderCallback = std::function<void(const std::string&, const std::string&)>;

        private:
            s32 x;
            s32 y;
            u32 entry_width_count;
            u32 entry_size;
            u32 base_entry_idx_x;
            u32 cur_entry_idx;
            s32 cursor_transition_x;
            pu::ui::SigmoidIncrementer<s32> cursor_transition_x_incr;
            s32 cursor_transition_y;
            pu::ui::SigmoidIncrementer<s32> cursor_transition_y_incr;
            s32 pre_transition_base_entry_idx_x;
            s32 pre_transition_entry_idx;
            s32 after_transition_base_entry_idx_x;
            s32 after_transition_entry_idx;
            pu::sdl2::Texture cursor_over_icon;
            u32 cursor_size;
            pu::sdl2::Texture border_over_icon;
            u32 border_size;
            pu::sdl2::Texture suspended_over_icon;
            u32 suspended_size;
            pu::sdl2::Texture selected_over_icon;
            u32 selected_size;
            pu::sdl2::Texture hb_takeover_app_over_icon;
            u32 hb_takeover_app_size;
            pu::sdl2::Texture gamecard_over_icon;
            u32 gamecard_size;
            pu::sdl2::Texture corrupted_over_icon;
            u32 corrupted_size;
            pu::sdl2::Texture not_launchable_over_icon;
            u32 not_launchable_size;
            pu::sdl2::Texture needs_update_over_icon;
            u32 needs_update_size;
            std::vector<pu::sdl2::TextureHandle::Ref> entry_icons;
            std::vector<pu::sdl2::Texture> entry_over_texts;
            std::vector<float> entry_progresses;
            std::vector<u8> load_img_entry_alphas;
            std::vector<pu::ui::SigmoidIncrementer<u8>> load_img_entry_incrs;
            std::string cur_path;
            std::vector<Entry> cur_entries;
            pu::sdl2::Texture entry_info_bg;
            std::stack<u32> entry_idx_stack;
            FocusedEntryInputPressedCallback cur_entry_input_cb;
            FocusedEntryChangedCallback cur_entry_changed_cb;
            FocusedEntryChangeStartedCallback cur_entry_change_started_cb;
            SheetActionCallback sheet_action_cb;
            ShelfFolderCallback shelf_folder_cb;
            bool enabled;
            s32 selected_entry_idx;
            pu::sdl2::TextureHandle::Ref empty_entry_icon;
            pu::sdl2::TextureHandle::Ref folder_entry_icon;
            pu::sdl2::TextureHandle::Ref selected_entry_icon;
            u32 entry_h_margin;
            u32 v_side_margin;
            std::vector<Entry> entries_to_add;
            std::vector<Entry> entries_to_remove;
            u32 pending_load_img_entry_start_idx;
            u32 pending_load_img_entry_ext_idx;
            bool pending_load_img_done;
            u32 entry_page_count;
            u32 entry_total_count;
            SwipeMode swipe_mode;
            u32 entries_base_swipe_neg_offset;
            pu::ui::SigmoidIncrementer<u32> entries_base_swipe_neg_offset_incr;
            u32 extra_entry_swipe_show_h_count;

            // ---- NX Dash: shelf mode (curved row of game cases) ----
            static constexpr bool ShelfMode = true;
            double shelf_s = 0.0;
            s32 shelf_target = 0;
            bool shelf_synced = false;
            u32 shelf_hold_frames = 0;
            std::vector<u32> shelf_valid;
            std::vector<pu::sdl2::Texture> shelf_title_texts;
            std::vector<double> shelf_xs;
            double shelf_xs_u = 0.0;
            void ShelfRebuildValid();
            void ShelfClearTitles();
            void ShelfMove(const s32 dir);
            void ShelfRender(pu::ui::render::Renderer::Ref &drawer, const s32 x, const s32 y);
            void ShelfInput(const u64 keys_down, const u64 keys_held, const pu::ui::TouchPoint touch_pos);
            struct ShelfHit {
                s32 pos;
                s32 x;
                s32 y;
                s32 w;
                s32 h;
            };
            std::vector<ShelfHit> shelf_hits;
            double shelf_step_px = 40.0;
            bool shelf_touching = false;
            bool shelf_touch_moved = false;
            s32 shelf_touch_start_x = 0;
            s32 shelf_touch_last_x = 0;
            s32 shelf_touch_last_y = 0;
            s32 shelf_touch_applied = 0;
            void ShelfMoveToPos(const s32 pos);

            // ---- NX Dash: category shelves (Up/Down) ----
            enum class ShelfFilter : u32 { Games, Folder, Homebrew, System };
            struct ShelfCategory {
                std::string name;
                std::string path;
                ShelfFilter filter;
            };
            std::vector<ShelfCategory> shelf_cats;
            std::string shelf_cats_sig;
            bool shelf_root_loaded = false;
            bool shelf_restore_pending = false;
            s32 shelf_cat = 0;
            ShelfFilter shelf_root_filter = ShelfFilter::Games;
            std::vector<std::pair<std::string, std::string>> shelf_root_folders;
            bool shelf_root_has_hb = false;
            bool shelf_root_has_special = false;
            std::map<std::string, s32> shelf_cat_pos;
            double shelf_vy = 0.0;
            double shelf_press = 0.0;

            // ---- NX Dash: motion (time based, so it stays even if a frame is dropped) ----
            u64 fx_last_tick = 0;
            double fx_dt = 1.0 / 60.0;
            double fx_k = 1.0;
            double fx_time = 0.0;
            double shelf_intro = 0.0;
            double shelf_intro_dir = 1.0;
            double shelf_sheen = 1.0;
            bool shelf_sheen_pending = true;
            bool tab_hl_init = false;
            double tab_hl_x = 0.0;
            double tab_hl_w = 0.0;
            double sheet_sel_y = -1.0;
            inline double FxApproach(const double cur, const double target, const double rate) {
                return cur + (target - cur) * (1.0 - std::pow(1.0 - rate, this->fx_k));
            }

            // ---- NX Dash: ambient light from the focused cover ----
            SDL_Texture *amb_cur = nullptr;
            SDL_Texture *amb_prev = nullptr;
            double amb_fade = 1.0;
            s32 amb_idx = -2;
            std::string amb_path;
            bool amb_failed = false;
            bool shelf_held = false;
            std::vector<pu::sdl2::Texture> shelf_tab_texts;
            std::vector<std::string> shelf_tab_names;
            std::vector<ShelfHit> shelf_tab_hits;
            pu::sdl2::Texture shelf_empty_text = nullptr;
            void ShelfRebuildCategories();
            bool ShelfEntryMatches(const Entry &entry);
            void ShelfSwitchCategory(const s32 dir);
            void ShelfGoToCategory(const s32 idx);
            std::string ShelfCatKey();

            // ---- NX Dash: focused game info under the shelf ----
            s32 shelf_info_idx = -1;
            std::string shelf_info_path;
            pu::sdl2::Texture shelf_info_title = nullptr;
            pu::sdl2::Texture shelf_info_meta = nullptr;
            void ShelfClearInfo();
            void ShelfRenderInfo(pu::ui::render::Renderer::Ref &drawer, const s32 x, const s32 y);

            // ---- NX Dash: HOME game sheet ----
            bool sheet_checked = false;
            bool sheet_open = false;
            double sheet_anim = 0.0;
            s32 sheet_sel = 0;
            std::vector<u32> sheet_actions;
            std::vector<pu::sdl2::Texture> sheet_opt_texts;
            pu::sdl2::Texture sheet_title = nullptr;
            pu::sdl2::Texture sheet_meta = nullptr;
            pu::sdl2::Texture sheet_badge = nullptr;
            pu::sdl2::Texture sheet_time = nullptr;
            pu::sdl2::Texture sheet_hint = nullptr;
            std::vector<ShelfHit> sheet_hits;
            bool sheet_touching = false;
            s32 sheet_touch_x = 0;
            s32 sheet_touch_y = 0;
            void SheetOpen();
            void SheetFree();
            void SheetInput(const u64 keys_down, const pu::ui::TouchPoint touch_pos);
            void SheetExecute(const u32 action);

            inline bool IsNotInSwipe() {
                return this->swipe_mode == SwipeMode::None;
            }

            inline bool IsNotInLargeSwipe() {
                return (this->swipe_mode != SwipeMode::LeftPage) && (this->swipe_mode != SwipeMode::RightPage) && (this->swipe_mode != SwipeMode::Rewind);
            }

            void SwipeSingleLeft();
            void SwipeSingleRight();
            void SwipeToPreviousPage();
            void SwipeToNextPage();
            void SwipeRewind();

            pu::sdl2::TextureHandle::Ref LoadEntryIconTexture(const Entry &entry);
            void CleanOverTexts();
            void LoadOverText(const u32 idx);
            bool LoadEntry(const u32 idx);

            void NotifyFocusedEntryChanged(const s32 prev_entry_idx, const s32 is_prev_entry_suspended_override = -1);

            inline bool IsCursorInTransition() {
                return !this->cursor_transition_x_incr.IsDone() || !this->cursor_transition_y_incr.IsDone();
            }

            inline void ComputeSizes(const u32 entry_height_count) {
                this->entry_size = (u32)((double)(this->GetHeight() - 2 * VerticalSideMargin - (entry_height_count - 1) * EntryVerticalMargin) / (double)entry_height_count);
                // Let's not overscale icons, avoid them looking too ugly
                if(this->entry_size > MinScalableEntrySize) {
                    this->entry_size = MinScalableEntrySize;
                }
                this->v_side_margin = (u32)((double)(this->GetHeight() - entry_height_count * this->entry_size - (entry_height_count - 1) * EntryVerticalMargin) / 2.0f);

                this->entry_width_count = (u32)((double)(this->GetWidth() - 2 * HorizontalSideMargin + EntryVerticalMargin) / (double)(this->entry_size + EntryVerticalMargin));
                this->entry_h_margin = (u32)((double)(this->GetWidth() - 2 * HorizontalSideMargin - this->entry_width_count * this->entry_size) / (double)(this->entry_width_count - 1));

                this->entry_page_count = this->entry_width_count * entry_height_count;
                this->entry_total_count = ((this->cur_entries.size() / this->entry_page_count) + 1) * this->entry_page_count;

                #define _COMPUTE_OVER_SIZE(name) { \
                    const auto over_def_size = pu::ui::render::GetTextureWidth(this->name##_over_icon); \
                    auto factor = ((double)over_def_size / (double)MinScalableEntrySize); \
                    if(factor < 1.0f) { \
                        factor = ((double)over_def_size / (double)V2MinScalableEntrySize); \
                    } \
                    this->name##_size = (u32)((double)this->entry_size * factor); \
                }

                _COMPUTE_OVER_SIZE(cursor)
                _COMPUTE_OVER_SIZE(border)
                _COMPUTE_OVER_SIZE(suspended)
                _COMPUTE_OVER_SIZE(selected)
                _COMPUTE_OVER_SIZE(hb_takeover_app)
                _COMPUTE_OVER_SIZE(gamecard)
                _COMPUTE_OVER_SIZE(corrupted)
                _COMPUTE_OVER_SIZE(not_launchable)
                _COMPUTE_OVER_SIZE(needs_update)
            }

            void SetFocusedEntryIndex(const u32 idx);
            Entry &GetEntry(const s32 index);

            void OrganizeEntries();

        public:
            EntryMenu(const s32 x, const s32 y, const std::string &path, FocusedEntryInputPressedCallback cur_entry_input_cb, FocusedEntryChangedCallback cur_entry_changed_cb, FocusedEntryChangeStartedCallback cur_entry_change_started_cb);
            PU_SMART_CTOR(EntryMenu)

            void Initialize(const u32 last_idx, const std::string &new_path = "");

            inline s32 GetX() override {
                return this->x;
            }

            inline s32 GetY() override {
                return this->y;
            }

            inline void SetX(const s32 x) {
                this->x = x;
            }

            inline void SetY(const s32 y) {
                this->y = y;
            }

            inline constexpr s32 GetWidth() override {
                return pu::ui::render::ScreenWidth - this->x;
            }

            inline s32 GetHeight() override {
                return pu::ui::render::ScreenHeight - this->y;
            }

            inline void SetEnabled(const bool enabled) {
                this->enabled = enabled;
            }

            inline std::string &GetPath() {
                return this->cur_path;
            }

            inline std::vector<Entry> &GetEntries() {
                return this->cur_entries;
            }

            bool IsInRoot();

            inline void SetNxDashCallbacks(SheetActionCallback sheet_cb, ShelfFolderCallback folder_cb) {
                this->sheet_action_cb = sheet_cb;
                this->shelf_folder_cb = folder_cb;
            }

            inline bool IsSheetOpen() {
                return this->sheet_open;
            }

            inline void CloseSheetForResume() {
                this->sheet_open = false;
                this->sheet_anim = 0.0;
            }

            void RenderOverlay(pu::ui::render::Renderer::Ref &drawer);
            void RenderBackdrop(pu::ui::render::Renderer::Ref &drawer);

            inline bool IsMenuStart() {
                return this->base_entry_idx_x == 0;
            }

            void OnRender(pu::ui::render::Renderer::Ref &drawer, const s32 x, const s32 y) override;
            void OnInput(const u64 keys_down, const u64 keys_up, const u64 keys_held, const pu::ui::TouchPoint touch_pos) override;

            void MoveTo(const std::string &new_path = "", const bool force_pop_idx = false);

            void OrganizeUpdateEntries(const s32 prev_entry_suspended_override = -1);
            void NotifyEntryAdded(const Entry &entry);
            void NotifyEntryRemoved(const Entry &entry);

            inline u32 GetFocusedEntryIndex() {
                return this->cur_entry_idx;
            }

            bool IsFocusedNonemptyEntry();

            inline Entry &GetFocusedEntry() {
                return this->GetEntry(this->cur_entry_idx);
            }

            void UpdateEntryProgress(const u32 idx, const float progress = NAN);

            inline bool IsFocusedEntryInProgress() {
                return !std::isnan(this->entry_progresses.at(this->cur_entry_idx));
            }

            void IncrementEntryHeightCount();
            bool CanDecrementEntryHeightCount();
            void DecrementEntryHeightCount();

            bool MoveToPreviousPage();
            bool MoveToNextPage();
            void Rewind();

            void ResetSelection();
            void SetEntrySelected(const u32 idx, const bool selected);
            bool IsEntrySelected(const u32 idx);
            bool IsAnySelected();

            inline Entry &GetSelectedEntry() {
                return this->GetEntry(this->selected_entry_idx);
            }

            bool IsFocusedEntrySuspended();

            inline void ToggleFocusedEntrySelected() {
                this->SetEntrySelected(this->cur_entry_idx, !this->IsEntrySelected(this->cur_entry_idx));
            }

            inline void SetFocusedEntrySelected(const bool selected) {
                this->SetEntrySelected(this->cur_entry_idx, selected);
            }

            inline bool IsFocusedEntrySelected() {
                return this->IsEntrySelected(this->cur_entry_idx);
            }
    };


    // Draws the HOME game sheet above the screen capture background
    class EntryMenuOverlay : public pu::ui::elm::Element {
        private:
            EntryMenu::Ref menu;
        public:
            EntryMenuOverlay(EntryMenu::Ref menu) : menu(menu) {}
            PU_SMART_CTOR(EntryMenuOverlay)
            inline s32 GetX() override { return 0; }
            inline s32 GetY() override { return 0; }
            inline s32 GetWidth() override { return pu::ui::render::ScreenWidth; }
            inline s32 GetHeight() override { return pu::ui::render::ScreenHeight; }
            void OnRender(pu::ui::render::Renderer::Ref &drawer, const s32 x, const s32 y) override {
                this->menu->RenderOverlay(drawer);
            }
            void OnInput(const u64 keys_down, const u64 keys_up, const u64 keys_held, const pu::ui::TouchPoint touch_pos) override {}
    };

    // Draws the ambient light behind everything else (the entry menu is created later, hence the pointer)
    class EntryMenuBackdrop : public pu::ui::elm::Element {
        private:
            EntryMenu::Ref *menu;
        public:
            EntryMenuBackdrop(EntryMenu::Ref *menu) : menu(menu) {}
            PU_SMART_CTOR(EntryMenuBackdrop)
            inline s32 GetX() override { return 0; }
            inline s32 GetY() override { return 0; }
            inline s32 GetWidth() override { return pu::ui::render::ScreenWidth; }
            inline s32 GetHeight() override { return pu::ui::render::ScreenHeight; }
            void OnRender(pu::ui::render::Renderer::Ref &drawer, const s32 x, const s32 y) override {
                if((this->menu != nullptr) && (*this->menu != nullptr)) {
                    (*this->menu)->RenderBackdrop(drawer);
                }
            }
            void OnInput(const u64 keys_down, const u64 keys_up, const u64 keys_held, const pu::ui::TouchPoint touch_pos) override {}
    };

}
