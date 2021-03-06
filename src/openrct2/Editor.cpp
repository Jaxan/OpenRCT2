#pragma region Copyright (c) 2014-2017 OpenRCT2 Developers
/*****************************************************************************
 * OpenRCT2, an open source clone of Roller Coaster Tycoon 2.
 *
 * OpenRCT2 is the work of many authors, a full list can be found in contributors.md
 * For more information, visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * A full copy of the GNU General Public License can be found in licence.txt
 *****************************************************************************/
#pragma endregion

#include <array>
#include <vector>
#include "Context.h"
#include "Editor.h"
#include "EditorObjectSelectionSession.h"
#include "FileClassifier.h"
#include "Game.h"
#include "OpenRCT2.h"
#include "ParkImporter.h"
#include "audio/audio.h"
#include "core/Math.hpp"
#include "interface/Viewport.h"
#include "localisation/Localisation.h"
#include "management/NewsItem.h"
#include "object/ObjectManager.h"
#include "object/ObjectRepository.h"
#include "object/ObjectList.h"
#include "peep/Staff.h"
#include "rct1/RCT1.h"
#include "util/Util.h"
#include "windows/Intent.h"
#include "world/Climate.h"
#include "interface/Window_internal.h"

namespace Editor
{
    static std::array<std::vector<uint8>, OBJECT_TYPE_COUNT> _editorSelectedObjectFlags;

    static void ConvertSaveToScenarioCallback(sint32 result, const utf8 * path);
    static void SetAllLandOwned();
    static bool LoadLandscapeFromSV4(const char * path);
    static bool LoadLandscapeFromSC4(const char * path);
    static void FinaliseMainView();
    static bool ReadS6(const char * path);
    static void ClearMapForEditing(bool fromSave);

    /**
     *
     *  rct2: 0x0066FFE1
     */
    void Load()
    {
        audio_stop_all_music_and_sounds();
        object_manager_unload_all_objects();
        object_list_load();
        game_init_all(150);
        gScreenFlags = SCREEN_FLAGS_SCENARIO_EDITOR;
        gS6Info.editor_step = EDITOR_STEP_OBJECT_SELECTION;
        gParkFlags |= PARK_FLAGS_SHOW_REAL_GUEST_NAMES;
        gS6Info.category = SCENARIO_CATEGORY_OTHER;
        viewport_init_all();
        rct_window * mainWindow = context_open_window_view(WV_EDITOR_MAIN);
        window_set_location(mainWindow, 2400, 2400, 112);
        load_palette();
        gScreenAge = 0;

        safe_strcpy(gScenarioName, language_get_string(STR_MY_NEW_SCENARIO), 64);
    }

    /**
     *
     *  rct2: 0x00672781
     */
    void ConvertSaveToScenario()
    {
        tool_cancel();
        auto intent = Intent(WC_LOADSAVE);
        intent.putExtra(INTENT_EXTRA_LOADSAVE_TYPE, LOADSAVETYPE_LOAD | LOADSAVETYPE_GAME);
        intent.putExtra(INTENT_EXTRA_CALLBACK, (void *) ConvertSaveToScenarioCallback);
        context_open_intent(&intent);
    }

    static void ConvertSaveToScenarioCallback(sint32 result, const utf8 * path)
    {
        if (result != MODAL_RESULT_OK)
        {
            return;
        }

        if (!context_load_park_from_file(path))
        {
            return;
        }

        if (gParkFlags & PARK_FLAGS_NO_MONEY)
        {
            gParkFlags |= PARK_FLAGS_NO_MONEY_SCENARIO;
        }
        else
        {
            gParkFlags &= ~PARK_FLAGS_NO_MONEY_SCENARIO;
        }
        gParkFlags |= PARK_FLAGS_NO_MONEY;

        safe_strcpy(gS6Info.name, gScenarioName, 64);
        safe_strcpy(gS6Info.details, gScenarioDetails, 256);
        gS6Info.objective_type  = gScenarioObjectiveType;
        gS6Info.objective_arg_1 = gScenarioObjectiveYear;
        gS6Info.objective_arg_2 = gScenarioObjectiveCurrency;
        gS6Info.objective_arg_3 = gScenarioObjectiveNumGuests;
        climate_reset(gClimate);

        gScreenFlags = SCREEN_FLAGS_SCENARIO_EDITOR;
        gS6Info.editor_step = EDITOR_STEP_OBJECTIVE_SELECTION;
        gS6Info.category    = SCENARIO_CATEGORY_OTHER;
        viewport_init_all();
        news_item_init_queue();
        context_open_window_view(WV_EDITOR_MAIN);
        FinaliseMainView();
        gScreenAge = 0;
    }

    /**
     *
     *  rct2: 0x00672957
     */
    void LoadTrackDesigner()
    {
        audio_stop_all_music_and_sounds();
        gScreenFlags = SCREEN_FLAGS_TRACK_DESIGNER;
        gScreenAge   = 0;

        object_manager_unload_all_objects();
        object_list_load();
        game_init_all(150);
        SetAllLandOwned();
        gS6Info.editor_step = EDITOR_STEP_OBJECT_SELECTION;
        viewport_init_all();
        rct_window * mainWindow = context_open_window_view(WV_EDITOR_MAIN);
        window_set_location(mainWindow, 2400, 2400, 112);
        load_palette();
    }

    /**
     *
     *  rct2: 0x006729FD
     */
    void LoadTrackManager()
    {
        audio_stop_all_music_and_sounds();
        gScreenFlags = SCREEN_FLAGS_TRACK_MANAGER;
        gScreenAge   = 0;

        object_manager_unload_all_objects();
        object_list_load();
        game_init_all(150);
        SetAllLandOwned();
        gS6Info.editor_step = EDITOR_STEP_OBJECT_SELECTION;
        viewport_init_all();
        rct_window * mainWindow = context_open_window_view(WV_EDITOR_MAIN);
        window_set_location(mainWindow, 2400, 2400, 112);
        load_palette();
    }

    /**
     *
     *  rct2: 0x0068ABEC
     */
    static void SetAllLandOwned()
    {
        sint32 mapSize = gMapSize;

        game_do_command(64, 1, 64, 2, GAME_COMMAND_SET_LAND_OWNERSHIP, (mapSize - 3) * 32, (mapSize - 3) * 32);
    }

    /**
     *
     *  rct2: 0x006758C0
     */
    bool LoadLandscape(const utf8 * path)
    {
        // #4996: Make sure the object selection window closes here to prevent unload objects
        //        after we have loaded a new park.
        window_close_all();

        uint32 extension = get_file_extension_type(path);
        switch (extension)
        {
        case FILE_EXTENSION_SC6:
        case FILE_EXTENSION_SV6:
            return ReadS6(path);
        case FILE_EXTENSION_SC4:
            return LoadLandscapeFromSC4(path);
        case FILE_EXTENSION_SV4:
            return LoadLandscapeFromSV4(path);
        default:
            return false;
        }
    }

    /**
     *
     *  rct2: 0x006A2B02
     */
    static bool LoadLandscapeFromSV4(const char * path)
    {
        load_from_sv4(path);
        ClearMapForEditing(true);

        gS6Info.editor_step = EDITOR_STEP_LANDSCAPE_EDITOR;
        gScreenAge   = 0;
        gScreenFlags = SCREEN_FLAGS_SCENARIO_EDITOR;
        viewport_init_all();
        context_open_window_view(WV_EDITOR_MAIN);
        FinaliseMainView();
        return true;
    }

    static bool LoadLandscapeFromSC4(const char * path)
    {
        load_from_sc4(path);
        ClearMapForEditing(false);

        gS6Info.editor_step = EDITOR_STEP_LANDSCAPE_EDITOR;
        gScreenAge   = 0;
        gScreenFlags = SCREEN_FLAGS_SCENARIO_EDITOR;
        viewport_init_all();
        context_open_window_view(WV_EDITOR_MAIN);
        FinaliseMainView();
        return true;
    }

    /**
     *
     *  rct2: 0x006758FE
     */
    static bool ReadS6(const char * path)
    {
        ParkLoadResult * loadResult = nullptr;
        const char     * extension  = path_get_extension(path);
        if (_stricmp(extension, ".sc6") == 0)
        {
            loadResult = load_from_sc6(path);
        }
        else if (_stricmp(extension, ".sv6") == 0)
        {
            loadResult = load_from_sv6(path);
        }
        if (ParkLoadResult_GetError(loadResult) != PARK_LOAD_ERROR_OK)
        {
            ParkLoadResult_Delete(loadResult);
            return false;
        }
        ParkLoadResult_Delete(loadResult);

        ClearMapForEditing(true);

        gS6Info.editor_step = EDITOR_STEP_LANDSCAPE_EDITOR;
        gScreenAge   = 0;
        gScreenFlags = SCREEN_FLAGS_SCENARIO_EDITOR;
        viewport_init_all();
        context_open_window_view(WV_EDITOR_MAIN);
        FinaliseMainView();
        return true;
    }

    static void ClearMapForEditing(bool fromSave)
    {
        map_remove_all_rides();

        //
        for (auto &banner : gBanners)
        {
            if (banner.type == 255)
            {
                banner.flags &= ~BANNER_FLAG_LINKED_TO_RIDE;
            }
        }

        //
        {
            sint32 i;
            Ride * ride;
            FOR_ALL_RIDES(i, ride)
                {
                    user_string_free(ride->name);
                }
        }

        ride_init_all();

        //
        for (sint32 i = 0; i < MAX_SPRITES; i++)
        {
            rct_sprite * sprite = get_sprite(i);
            user_string_free(sprite->unknown.name_string_idx);
        }

        reset_sprite_list();
        staff_reset_modes();
        gNumGuestsInPark         = 0;
        gNumGuestsHeadingForPark = 0;
        gNumGuestsInParkLastWeek = 0;
        gGuestChangeModifier     = 0;
        if (fromSave)
        {
            research_populate_list_random();

            if (gParkFlags & PARK_FLAGS_NO_MONEY)
            {
                gParkFlags |= PARK_FLAGS_NO_MONEY_SCENARIO;
            }
            else
            {
                gParkFlags &= ~PARK_FLAGS_NO_MONEY_SCENARIO;
            }
            gParkFlags |= PARK_FLAGS_NO_MONEY;

            if (gParkEntranceFee == 0)
            {
                gParkFlags |= PARK_FLAGS_PARK_FREE_ENTRY;
            }
            else
            {
                gParkFlags &= ~PARK_FLAGS_PARK_FREE_ENTRY;
            }

            gParkFlags &= ~PARK_FLAGS_SPRITES_INITIALISED;

            gGuestInitialCash = Math::Clamp((money16)MONEY(10, 00), gGuestInitialCash, (money16)MAX_ENTRANCE_FEE);

            gInitialCash = Math::Min(gInitialCash, 100000);
            finance_reset_cash_to_initial();

            gBankLoan = Math::Clamp(
                MONEY(0, 00),
                gBankLoan,
                MONEY(5000000, 00)
            );

            gMaxBankLoan = Math::Clamp(
                MONEY(0, 00),
                gMaxBankLoan,
                MONEY(5000000, 00)
            );

            gBankLoanInterestRate = Math::Clamp((uint8)5, gBankLoanInterestRate, (uint8)80);
        }

        climate_reset(gClimate);

        news_item_init_queue();
    }

    /**
     *
     *  rct2: 0x0067009A
     */
    void OpenWindowsForCurrentStep()
    {
        if (!(gScreenFlags & SCREEN_FLAGS_EDITOR))
        {
            return;
        }

        switch (gS6Info.editor_step)
        {
        case EDITOR_STEP_OBJECT_SELECTION:
            if (window_find_by_class(WC_EDITOR_OBJECT_SELECTION))
            {
                return;
            }

            if (window_find_by_class(WC_INSTALL_TRACK))
            {
                return;
            }

            if (gScreenFlags & SCREEN_FLAGS_TRACK_MANAGER)
            {
                object_manager_unload_all_objects();
            }

            context_open_window(WC_EDITOR_OBJECT_SELECTION);
            break;
        case EDITOR_STEP_INVENTIONS_LIST_SET_UP:
            if (window_find_by_class(WC_EDITOR_INVENTION_LIST))
            {
                return;
            }

            context_open_window(WC_EDITOR_INVENTION_LIST);
            break;
        case EDITOR_STEP_OPTIONS_SELECTION:
            if (window_find_by_class(WC_EDITOR_SCENARIO_OPTIONS))
            {
                return;
            }

            context_open_window(WC_EDITOR_SCENARIO_OPTIONS);
            break;
        case EDITOR_STEP_OBJECTIVE_SELECTION:
            if (window_find_by_class(WC_EDTIOR_OBJECTIVE_OPTIONS))
            {
                return;
            }

            context_open_window(WC_EDTIOR_OBJECTIVE_OPTIONS);
            break;
        }
    }

    static void FinaliseMainView()
    {
        rct_window   * w        = window_get_main();
        rct_viewport * viewport = window_get_viewport(w);

        w->viewport_target_sprite = SPRITE_INDEX_NULL;
        w->saved_view_x           = gSavedViewX;
        w->saved_view_y           = gSavedViewY;
        gCurrentRotation = gSavedViewRotation;

        sint32 zoom_difference = gSavedViewZoom - viewport->zoom;
        viewport->zoom = gSavedViewZoom;
        if (zoom_difference != 0)
        {
            if (zoom_difference >= 0)
            {
                viewport->view_width <<= zoom_difference;
                viewport->view_height <<= zoom_difference;
            }
            else
            {
                zoom_difference = -zoom_difference;
                viewport->view_width >>= zoom_difference;
                viewport->view_height >>= zoom_difference;
            }
        }
        w->saved_view_x -= viewport->view_width >> 1;
        w->saved_view_y -= viewport->view_height >> 1;

        window_invalidate(w);
        reset_all_sprite_quadrant_placements();
        scenery_set_default_placement_configuration();

        auto intent = Intent(INTENT_ACTION_REFRESH_NEW_RIDES);
        context_broadcast_intent(&intent);

        gWindowUpdateTicks = 0;
        load_palette();

        intent = Intent(INTENT_ACTION_CLEAR_TILE_INSPECTOR_CLIPBOARD);
        context_broadcast_intent(&intent);
    }

    /**
     *
     *  rct2: 0x006AB9B8
     */
    sint32 CheckObjectSelection()
    {
        bool isTrackDesignerManager =
                 gScreenFlags & (SCREEN_FLAGS_TRACK_DESIGNER | SCREEN_FLAGS_TRACK_MANAGER);

        if (!isTrackDesignerManager)
        {
            if (!editor_check_object_group_at_least_one_selected(OBJECT_TYPE_PATHS))
            {
                gGameCommandErrorText = STR_AT_LEAST_ONE_PATH_OBJECT_MUST_BE_SELECTED;
                return OBJECT_TYPE_PATHS;
            }
        }

        if (!editor_check_object_group_at_least_one_selected(OBJECT_TYPE_RIDE))
        {
            gGameCommandErrorText = STR_AT_LEAST_ONE_RIDE_OBJECT_MUST_BE_SELECTED;
            return OBJECT_TYPE_RIDE;
        }

        if (!isTrackDesignerManager)
        {
            if (!editor_check_object_group_at_least_one_selected(OBJECT_TYPE_PARK_ENTRANCE))
            {
                gGameCommandErrorText = STR_PARK_ENTRANCE_TYPE_MUST_BE_SELECTED;
                return OBJECT_TYPE_PARK_ENTRANCE;
            }

            if (!editor_check_object_group_at_least_one_selected(OBJECT_TYPE_WATER))
            {
                gGameCommandErrorText = STR_WATER_TYPE_MUST_BE_SELECTED;
                return OBJECT_TYPE_WATER;
            }
        }

        return -1;
    }

    /**
     *
     *  rct2: 0x0066FEAC
     */
    bool CheckPark()
    {
        sint32 parkSize = park_calculate_size();
        if (parkSize == 0)
        {
            gGameCommandErrorText = STR_PARK_MUST_OWN_SOME_LAND;
            return false;
        }

        for (sint32 i = 0; i < MAX_PARK_ENTRANCES; i++)
        {
            if (gParkEntrances[i].x != LOCATION_NULL)
            {
                break;
            }

            if (i == MAX_PARK_ENTRANCES - 1)
            {
                gGameCommandErrorText = STR_NO_PARK_ENTRANCES;
                return false;
            }
        }

        for (const auto &parkEntrance : gParkEntrances)
        {
            if (parkEntrance.x == LOCATION_NULL)
            {
                continue;
            }

            sint32 x         = parkEntrance.x;
            sint32 y         = parkEntrance.y;
            sint32 z         = parkEntrance.z / 8;
            sint32 direction = parkEntrance.direction ^ 2;

            switch (footpath_is_connected_to_map_edge(x, y, z, direction, 0))
            {
            case FOOTPATH_SEARCH_NOT_FOUND:
                gGameCommandErrorText = STR_PARK_ENTRANCE_WRONG_DIRECTION_OR_NO_PATH;
                return false;
            case FOOTPATH_SEARCH_INCOMPLETE:
            case FOOTPATH_SEARCH_TOO_COMPLEX:
                gGameCommandErrorText = STR_PARK_ENTRANCE_PATH_INCOMPLETE_OR_COMPLEX;
                return false;
            case FOOTPATH_SEARCH_SUCCESS:
                // Run the search again and unown the path
                footpath_is_connected_to_map_edge(x, y, z, direction, (1 << 5));
                break;
            }
        }

        for (sint32 i = 0; i < MAX_PEEP_SPAWNS; i++)
        {
            if (gPeepSpawns[i].x != PEEP_SPAWN_UNDEFINED)
            {
                break;
            }

            if (i == MAX_PEEP_SPAWNS - 1)
            {
                gGameCommandErrorText = STR_PEEP_SPAWNS_NOT_SET;
                return false;
            }
        }

        return true;
    }

    void GameCommandEditScenarioOptions(sint32 * eax, sint32 * ebx, sint32 * ecx, sint32 * edx, sint32 * esi, sint32 * edi, sint32 * ebp)
    {
        if (!(*ebx & GAME_COMMAND_FLAG_APPLY))
        {
            *ebx = 0;
            return;
        }

        switch (*ecx)
        {
        case EDIT_SCENARIOOPTIONS_SETNOMONEY:
            if (gScreenFlags & SCREEN_FLAGS_SCENARIO_EDITOR)
            {
                if (*edx != 0)
                {
                    gParkFlags |= PARK_FLAGS_NO_MONEY_SCENARIO;
                }
                else
                {
                    gParkFlags &= ~PARK_FLAGS_NO_MONEY_SCENARIO;
                }
            }
            else
            {
                if (*edx != 0)
                {
                    gParkFlags |= PARK_FLAGS_NO_MONEY;
                }
                else
                {
                    gParkFlags &= ~PARK_FLAGS_NO_MONEY;
                }
                // Invalidate all windows that have anything to do with finance
                window_invalidate_by_class(WC_RIDE);
                window_invalidate_by_class(WC_PEEP);
                window_invalidate_by_class(WC_PARK_INFORMATION);
                window_invalidate_by_class(WC_FINANCES);
                window_invalidate_by_class(WC_BOTTOM_TOOLBAR);
                window_invalidate_by_class(WC_TOP_TOOLBAR);
            }
            break;
        case EDIT_SCENARIOOPTIONS_SETINITIALCASH:
            gInitialCash   = Math::Clamp(MONEY(0, 00), *edx, MONEY(1000000, 00));
            gCash = gInitialCash;
            window_invalidate_by_class(WC_FINANCES);
            window_invalidate_by_class(WC_BOTTOM_TOOLBAR);
            break;
        case EDIT_SCENARIOOPTIONS_SETINITIALLOAN:
            gBankLoan    = Math::Clamp(MONEY(0, 00), *edx, MONEY(5000000, 00));
            gMaxBankLoan = Math::Max(gBankLoan, gMaxBankLoan);
            window_invalidate_by_class(WC_FINANCES);
            break;
        case EDIT_SCENARIOOPTIONS_SETMAXIMUMLOANSIZE:
            gMaxBankLoan = Math::Clamp(MONEY(0, 00), *edx, MONEY(5000000, 00));
            gBankLoan    = Math::Min(gBankLoan, gMaxBankLoan);
            window_invalidate_by_class(WC_FINANCES);
            break;
        case EDIT_SCENARIOOPTIONS_SETANNUALINTERESTRATE:
            gBankLoanInterestRate = Math::Clamp(0, *edx, 80);
            window_invalidate_by_class(WC_FINANCES);
            break;
        case EDIT_SCENARIOOPTIONS_SETFORBIDMARKETINGCAMPAIGNS:
            if (*edx != 0)
            {
                gParkFlags |= PARK_FLAGS_FORBID_MARKETING_CAMPAIGN;
            }
            else
            {
                gParkFlags &= ~PARK_FLAGS_FORBID_MARKETING_CAMPAIGN;
            }
            break;
        case EDIT_SCENARIOOPTIONS_SETAVERAGECASHPERGUEST:
            gGuestInitialCash = Math::Clamp(MONEY(0, 00), *edx, MONEY(1000, 00));
            break;
        case EDIT_SCENARIOOPTIONS_SETGUESTINITIALHAPPINESS:
            gGuestInitialHappiness = Math::Clamp(40, *edx, 250);
            break;
        case EDIT_SCENARIOOPTIONS_SETGUESTINITIALHUNGER:
            gGuestInitialHunger = Math::Clamp(40, *edx, 250);
            break;
        case EDIT_SCENARIOOPTIONS_SETGUESTINITIALTHIRST:
            gGuestInitialThirst = Math::Clamp(40, *edx, 250);
            break;
        case EDIT_SCENARIOOPTIONS_SETGUESTSPREFERLESSINTENSERIDES:
            if (*edx != 0)
            {
                gParkFlags |= PARK_FLAGS_PREF_LESS_INTENSE_RIDES;
            }
            else
            {
                gParkFlags &= ~PARK_FLAGS_PREF_LESS_INTENSE_RIDES;
            }
            break;
        case EDIT_SCENARIOOPTIONS_SETGUESTSPREFERMOREINTENSERIDES:
            if (*edx != 0)
            {
                gParkFlags |= PARK_FLAGS_PREF_MORE_INTENSE_RIDES;
            }
            else
            {
                gParkFlags &= ~PARK_FLAGS_PREF_MORE_INTENSE_RIDES;
            }
            break;
        case EDIT_SCENARIOOPTIONS_SETCOSTTOBUYLAND:
            gLandPrice = Math::Clamp(MONEY(5, 00), *edx, MONEY(200, 00));
            break;
        case EDIT_SCENARIOOPTIONS_SETCOSTTOBUYCONSTRUCTIONRIGHTS:
            gConstructionRightsPrice = Math::Clamp(MONEY(5, 00), *edx, MONEY(200, 00));
            break;
        case EDIT_SCENARIOOPTIONS_SETPARKCHARGEMETHOD:
            if (gScreenFlags & SCREEN_FLAGS_SCENARIO_EDITOR)
            {
                if (*edx == 0)
                {
                    gParkFlags |= PARK_FLAGS_PARK_FREE_ENTRY;
                    gParkFlags &= ~PARK_FLAGS_UNLOCK_ALL_PRICES;
                    gParkEntranceFee = MONEY(0, 00);
                }
                else if (*edx == 1)
                {
                    gParkFlags &= ~PARK_FLAGS_PARK_FREE_ENTRY;
                    gParkFlags &= ~PARK_FLAGS_UNLOCK_ALL_PRICES;
                    gParkEntranceFee = MONEY(10, 00);
                }
                else
                {
                    gParkFlags |= PARK_FLAGS_PARK_FREE_ENTRY;
                    gParkFlags |= PARK_FLAGS_UNLOCK_ALL_PRICES;
                    gParkEntranceFee = MONEY(10, 00);
                }
            }
            else
            {
                if (*edx == 0)
                {
                    gParkFlags |= PARK_FLAGS_PARK_FREE_ENTRY;
                    gParkFlags &= ~PARK_FLAGS_UNLOCK_ALL_PRICES;
                }
                else if (*edx == 1)
                {
                    gParkFlags &= ~PARK_FLAGS_PARK_FREE_ENTRY;
                    gParkFlags &= ~PARK_FLAGS_UNLOCK_ALL_PRICES;
                }
                else
                {
                    gParkFlags |= PARK_FLAGS_PARK_FREE_ENTRY;
                    gParkFlags |= PARK_FLAGS_UNLOCK_ALL_PRICES;
                }
                window_invalidate_by_class(WC_PARK_INFORMATION);
                window_invalidate_by_class(WC_RIDE);
            }
            break;
        case EDIT_SCENARIOOPTIONS_SETPARKCHARGEENTRYFEE:
            gParkEntranceFee = Math::Clamp(MONEY(0, 00), *edx, MAX_ENTRANCE_FEE);
            window_invalidate_by_class(WC_PARK_INFORMATION);
            break;
        case EDIT_SCENARIOOPTIONS_SETFORBIDTREEREMOVAL:
            if (*edx != 0)
            {
                gParkFlags |= PARK_FLAGS_FORBID_TREE_REMOVAL;
            }
            else
            {
                gParkFlags &= ~PARK_FLAGS_FORBID_TREE_REMOVAL;
            }
            break;
        case EDIT_SCENARIOOPTIONS_SETFORBIDLANDSCAPECHANGES:
            if (*edx != 0)
            {
                gParkFlags |= PARK_FLAGS_FORBID_LANDSCAPE_CHANGES;
            }
            else
            {
                gParkFlags &= ~PARK_FLAGS_FORBID_LANDSCAPE_CHANGES;
            }
            break;
        case EDIT_SCENARIOOPTIONS_SETFORBIDHIGHCONSTRUCTION:
            if (*edx != 0)
            {
                gParkFlags |= PARK_FLAGS_FORBID_HIGH_CONSTRUCTION;
            }
            else
            {
                gParkFlags &= ~PARK_FLAGS_FORBID_HIGH_CONSTRUCTION;
            }
            break;
        case EDIT_SCENARIOOPTIONS_SETPARKRATINGHIGHERDIFFICULTLEVEL:
            if (*edx != 0)
            {
                gParkFlags |= PARK_FLAGS_DIFFICULT_PARK_RATING;
            }
            else
            {
                gParkFlags &= ~PARK_FLAGS_DIFFICULT_PARK_RATING;
            }
            break;
        case EDIT_SCENARIOOPTIONS_SETGUESTGENERATIONHIGHERDIFFICULTLEVEL:
            if (*edx != 0)
            {
                gParkFlags |= PARK_FLAGS_DIFFICULT_GUEST_GENERATION;
            }
            else
            {
                gParkFlags &= ~PARK_FLAGS_DIFFICULT_GUEST_GENERATION;
            }
            break;
        }
        window_invalidate_by_class(WC_EDITOR_SCENARIO_OPTIONS);
        *ebx = 0;
    }

    uint8 GetSelectedObjectFlags(sint32 objectType, size_t index)
    {
        uint8 result = 0;
        auto &list = _editorSelectedObjectFlags[objectType];
        if (list.size() > index)
        {
            result = list[index];
        }
        return result;
    }

    void ClearSelectedObject(sint32 objectType, size_t index, uint32 flags)
    {
        auto &list = _editorSelectedObjectFlags[objectType];
        if (list.size() <= index)
        {
            list.resize(index + 1);
        }
        list[index] &= ~flags;
    }

    void SetSelectedObject(sint32 objectType, size_t index, uint32 flags)
    {
        auto &list = _editorSelectedObjectFlags[objectType];
        if (list.size() <= index)
        {
            list.resize(index + 1);
        }
        list[index] |= flags;
    }
}

void editor_open_windows_for_current_step()
{
    Editor::OpenWindowsForCurrentStep();
}

void game_command_edit_scenario_options(sint32 * eax, sint32 * ebx, sint32 * ecx, sint32 * edx, sint32 * esi, sint32 * edi, sint32 * ebp)
{
    Editor::GameCommandEditScenarioOptions(eax, ebx, ecx, edx, esi, edi, ebp);
}

