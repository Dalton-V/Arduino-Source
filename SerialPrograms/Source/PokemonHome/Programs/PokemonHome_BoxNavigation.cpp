/*  Home Box Navigation
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <array>
#include <optional>
#include <sstream>
#include <vector>
#include "Common/Cpp/Strings/Unicode.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/Notifications/ProgramInfo.h"
#include "CommonTools/Images/ImageFilter.h"
#include "CommonTools/OCR/OCR_RawOCR.h"
#include "CommonTools/OCR/OCR_Routines.h"
#include "CommonTools/OCR/OCR_StringNormalization.h"
#include "CommonFramework/Tools/ErrorDumper.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/OCR/OCR_NumberReader.h"
#include "CommonTools/VisualDetectors/FrozenImageDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "Pokemon/Resources/Pokemon_PokemonSlugs.h"
#include "Pokemon/Inference/Pokemon_TypeReader.h"
#include "PokemonHome/Inference/PokemonHome_BallReader.h"
#include "PokemonHome/Inference/PokemonHome_BoxGenderDetector.h"
#include "PokemonHome/Inference/PokemonHome_GigantamaxDetector.h"
#include "PokemonHome/Inference/PokemonHome_OriginMarkReader.h"
#include "PokemonHome/Inference/PokemonHome_TeraTypeReader.h"
#include "PokemonHome_BoxNavigation.h"
#include "PokemonHome/Inference/PokemonHome_ButtonDetector.h"
#include "PokemonHome/Inference/PokemonHome_SelectionArrowDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{
using namespace Pokemon;

namespace{

int find_position_index(ImageFloatBox dexted_box, const std::vector<ImageFloatBox>& possible_locations){
    size_t current_index = 0;
    double closest_dist = std::numeric_limits<double>::max();
    double detected_cx = dexted_box.x + dexted_box.width / 2.0;
    double detected_cy = dexted_box.y + dexted_box.height / 2.0;
    for (size_t i = 0; i < possible_locations.size(); i++){
        ImageFloatBox pos_box = possible_locations[i];
        double cx = pos_box.x + pos_box.width / 2.0;
        double cy = pos_box.y + pos_box.height / 2.0;
        double dx = detected_cx - cx;
        double dy = detected_cy - cy;
        double dist = dx * dx + dy * dy;
        if (dist < closest_dist){
            closest_dist = dist;
            current_index = i;
        }
    }

    return static_cast<int>(current_index);
}


}

// Move the red cursor to the first slot of the box
// If the cursor is not at the first slot, move the cursor to the left and up one row at a time until it is at the first slot. 
bool go_to_first_slot(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    Milliseconds VIDEO_DELAY
) {
    ImageFloatBox first_slot_cursor_box(0.07, 0.15, 0.01, 0.01); //cursor position of the first slot of the box
    VideoSnapshot screen = env.console.video().snapshot();
    FloatPixel first_slot_cursor_color = image_stats(extract_box_reference(screen, first_slot_cursor_box)).average;
    env.console.log("BoxCursor color detection: " + first_slot_cursor_color.to_string());
    VideoOverlaySet BoxRender(env.console);
    BoxRender.add(COLOR_BLUE, first_slot_cursor_box);

    // If the cursor is not at the first slot
    if (first_slot_cursor_color.r <= first_slot_cursor_color.g + first_slot_cursor_color.b) {
        bool cursor_found = false;
        for (uint8_t rows = 0; rows < 7; rows++) {
            for (uint8_t column = 0; column < 5; column++) {
                pbf_press_dpad(context, DPAD_LEFT, 80ms, VIDEO_DELAY);
                context.wait_for_all_requests();
                screen = env.console.video().snapshot();
                first_slot_cursor_color = image_stats(extract_box_reference(screen, first_slot_cursor_box)).average;
                env.console.log("BoxCursor color detection: " + first_slot_cursor_color.to_string());

                if (first_slot_cursor_color.r > first_slot_cursor_color.g + first_slot_cursor_color.b) {
                    cursor_found = true;
                    break;
                }
            }
            if (!cursor_found) {
                pbf_press_dpad(context, DPAD_UP, 80ms, VIDEO_DELAY);
                context.wait_for_all_requests();
                screen = env.console.video().snapshot();
                first_slot_cursor_color = image_stats(extract_box_reference(screen, first_slot_cursor_box)).average;
                env.console.log("BoxCursor color detection: " + first_slot_cursor_color.to_string());

                if (first_slot_cursor_color.r > first_slot_cursor_color.g + first_slot_cursor_color.b) {
                    cursor_found = true;
                    break;
                }
            }
            else {
                break;
            }
        }
        if (!cursor_found) {
            return false;
        }
    }
    BoxRender.clear();
    return true;
}

//Move the cursor to the given coordinates, knowing current pos via the cursor struct
[[nodiscard]] BoxCursor move_cursor_to(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    const BoxCursor& cur_cursor,
    const BoxCursor& dest_cursor,
    Milliseconds GAME_DELAY
) {

    std::ostringstream ss;
    ss << "Moving cursor from " << cur_cursor << " to " << dest_cursor;
    env.console.log(ss.str());

    // TODO: shortest path movement through pages, boxes
    for (size_t i = cur_cursor.box; i < dest_cursor.box; ++i) {
        pbf_press_button(context, BUTTON_R, 80ms, GAME_DELAY + 240ms);
    }
    for (size_t i = dest_cursor.box; i < cur_cursor.box; ++i) {
        pbf_press_button(context, BUTTON_L, 80ms, GAME_DELAY + 240ms);
    }


    // direct nav up or down through rows
    if (!(cur_cursor.row == 0 && dest_cursor.row == 4) && !(dest_cursor.row == 0 && cur_cursor.row == 4)) {
        for (size_t i = cur_cursor.row; i < dest_cursor.row; ++i) {
            pbf_press_dpad(context, DPAD_DOWN, 80ms, GAME_DELAY);
        }
        for (size_t i = dest_cursor.row; i < cur_cursor.row; ++i) {
            pbf_press_dpad(context, DPAD_UP, 80ms, GAME_DELAY);
        }
    }
    else { // wrap around is faster to move between first or last row
        if (cur_cursor.row == 0 && dest_cursor.row == 4) {
            for (size_t i = 0; i <= 2; ++i) {
                pbf_press_dpad(context, DPAD_UP, 80ms, GAME_DELAY);
            }
        }
        else {
            for (size_t i = 0; i <= 2; ++i) {
                pbf_press_dpad(context, DPAD_DOWN, 80ms, GAME_DELAY);
            }
        }
    }

    // direct nav forward or backward through columns
    if ((dest_cursor.column > cur_cursor.column && dest_cursor.column - cur_cursor.column <= 3) || (cur_cursor.column > dest_cursor.column && cur_cursor.column - dest_cursor.column <= 3)) {
        for (size_t i = cur_cursor.column; i < dest_cursor.column; ++i) {
            pbf_press_dpad(context, DPAD_RIGHT, 80ms, GAME_DELAY);
        }
        for (size_t i = dest_cursor.column; i < cur_cursor.column; ++i) {
            pbf_press_dpad(context, DPAD_LEFT, 80ms, GAME_DELAY);
        }
    }
    else { // wrap around is faster if direct movement is more than 3 away
        if (dest_cursor.column > cur_cursor.column) {
            for (size_t i = 0; i < BOX_COLS - (dest_cursor.column - cur_cursor.column); ++i) {
                pbf_press_dpad(context, DPAD_LEFT, 80ms, GAME_DELAY);
            }
        }
        if (cur_cursor.column > dest_cursor.column) {
            for (size_t i = 0; i < BOX_COLS - (cur_cursor.column - dest_cursor.column); ++i) {
                pbf_press_dpad(context, DPAD_RIGHT, 80ms, GAME_DELAY);
            }
        }
    }

    context.wait_for_all_requests();
    return dest_cursor;
}

//[[nodiscard]] BoxCursor move_cursor_to(
//    SingleSwitchProgramEnvironment& env,
//    ProControllerContext& context,
//    const BoxCursor& cur_cursor,
//    const BoxCursor& dest_cursor
//){
//
//}
//

BoxCursor open_box_spaces(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context
){
    SelectionArrowWatcher right_arrow(COLOR_RED, &env.console.overlay(), SelectionArrowType::RIGHT, { 0.101, 0.080, 0.252, 0.703 });
    SelectionArrowWatcher down_arrow(COLOR_RED, &env.console.overlay(), SelectionArrowType::DOWN, { 0.025, 0.131, 0.459, 0.566 });

    WallClock start = current_time();
    while (true){

        if (current_time() - start > std::chrono::seconds(60)){
            OperationFailedException::fire(
                ErrorReport::SEND_ERROR_REPORT,
                "open_box_spaces(): Unable to determine current box after 1 minute.",
                env.console
            );
        }

        env.log("Navigating to box spaces...");
        int ret = wait_until(env.console, context, Seconds(5), { right_arrow, down_arrow });
        
        int current_index = 0;
        int current_row;
        int current_col;
        switch (ret){
        case 0: {
            env.log("Detected right selection arrow, navigating to box spaces.");

            current_index = find_position_index(right_arrow.last_detected(), right_arrow.right_arrow_positions());
            
            if (current_index < 0 || current_index > 2){
                env.log("Detected right selection arrow at unexpected position, retrying.");
                pbf_mash_button(context, BUTTON_B, 500ms);
                context.wait_for_all_requests();
                continue;
            }

            SelectionArrowRightPosition right_arrow_pos = static_cast<SelectionArrowRightPosition>(current_index);
            switch (right_arrow_pos){
            case SelectionArrowRightPosition::BOX_SPACES:
                pbf_press_button(context, BUTTON_A, 150ms, 150ms);
                context.wait_for_all_requests();
                break;
            case SelectionArrowRightPosition::BOX_NAME:
                pbf_press_dpad(context, DPAD_DOWN, 150ms, 150ms);
                pbf_press_button(context, BUTTON_A, 150ms, 150ms);
                context.wait_for_all_requests();
                break;
            case SelectionArrowRightPosition::NEWEST_30:
                pbf_press_dpad(context, DPAD_LEFT, 150ms, 150ms);
                pbf_press_button(context, BUTTON_A, 150ms, 150ms);
                context.wait_for_all_requests();
                break;
            }
            break;
        }
        case 1: {
            env.log("Detected down selection arrow, navigating to box spaces.");

            current_index = find_position_index(down_arrow.last_detected(), down_arrow.down_arrow_positions());
            current_row = current_index / 6;
            current_col = current_index % 6;

            env.log("Current index: " + std::to_string(current_index));
            env.log("Detected down arrow at row " + std::to_string(current_row) + " col " + std::to_string(current_col));

            // The detection grid for the down arrow is 5x6 but there is a button above the grid that can be travelled over. 
            // Add one to include that distance in the following calculation.
            ++current_row;

            int total_rows = 7;
            int destination_index = 6; // Box Spaces is at the bottom
            int up = (current_row - destination_index + total_rows) % total_rows;
            int down = (destination_index - current_row + total_rows) % total_rows;

            if (up <= down){
                for (int i = 0; i < up; i++){
                    pbf_press_dpad(context, DPAD_UP, 150ms, 150ms);
                }
            } else{
                for (int i = 0; i < down; i++){
                    pbf_press_dpad(context, DPAD_DOWN, 150ms, 150ms);
                }
            }

            if (current_col > 4){
                pbf_press_dpad(context, DPAD_LEFT, 150ms, 150ms);
            }

            pbf_press_button(context, BUTTON_A, 150ms, 150ms);
            context.wait_for_all_requests();
            break;
        }
        default:
            env.log("Unable to detect selection arrow, retrying.");
            pbf_mash_button(context, BUTTON_B, 500ms);
            context.wait_for_all_requests();
            continue;
        }

        ButtonWatcher b_button_watcher(COLOR_RED, ButtonType::ButtonB, { 0.183, 0.714, 0.032, 0.055 }, &env.console.overlay());

        ret = wait_until(env.console, context, Seconds(5), { b_button_watcher });

        if (ret != 0){
            env.log("Unable to detect box spaces page, retrying.");
            pbf_mash_button(context, BUTTON_B, 500ms);
            context.wait_for_all_requests();
            continue;
        }

        if (!down_arrow.detect(env.console.video().snapshot())){
            env.log("Unable to detect down arrow after opening box spaces, retrying.");
            pbf_mash_button(context, BUTTON_B, 500ms);
            context.wait_for_all_requests();
            continue;
        }

        current_index = find_position_index(down_arrow.last_detected(), down_arrow.down_arrow_positions());

        current_row = current_index / 6;
        current_col = current_index % 6;

        if (current_row == 0){
            env.log("Cursor is on the top row, moving down to read the page number.");
            pbf_press_dpad(context, DPAD_DOWN, 150ms, 150ms);
            context.wait_for_all_requests();
        }

        ImageFloatBox page_number_box(0.277, 0.101, 0.013, 0.042);
        const int page_number = OCR::read_number_waterfill(env.console, extract_box_reference(env.console.video().snapshot(), page_number_box), 0xff505050, 0xff787878);
        if (page_number <= 0 || page_number > 7){
            OperationFailedException::fire(
                ErrorReport::SEND_ERROR_REPORT,
                "open_box_spaces: Unable to read a correct page number, found: " + std::to_string(page_number),
                env.console
            );
        }

        if (current_row == 0){
            env.log("Moving back up to the top row after reading page number.");
            pbf_press_dpad(context, DPAD_UP, 150ms, 150ms);
            context.wait_for_all_requests();
        }

        return BoxCursor(page_number, current_row, current_col);
    }
    
    return BoxCursor(0,0,0);
}


// Read current screen to find occupied and empty slots in the box.
// Add a placeholder value for each slot in order into `boxes_data`. For empty slot the value is just std::nullopt, while
// for the occupied slot it is an empty pokemon struct `CollectedPokemonInfo`.
// Return the (row, col) index of the first pokemon (aka non-empty) slot in the box.
std::array<size_t, 2> find_occupied_slots_in_box(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    std::vector<std::optional<CollectedPokemonInfo>>& boxes_data,
    const std::vector<SortingRule>& sort_preferences)
{
    //BoxSorter_Descriptor::Stats& stats = env.current_stats< BoxSorter_Descriptor::Stats>();

    VideoSnapshot screen = env.console.video().snapshot();

    std::ostringstream ss;
    ss << "\n";

    std::array<size_t, 2> first_pokemon_slot = { SIZE_MAX, SIZE_MAX };

    int num_empty_slots = 0;
    for (size_t row = 0; row < BOX_ROWS; row++) {
        for (size_t col = 0; col < BOX_COLS; col++) {
            ImageFloatBox slot_box(0.06 + (0.072 * col), 0.2 + (0.1035 * row), 0.03, 0.057);
            int current_box_value = (int)image_stddev(extract_box_reference(screen, slot_box)).sum();

            ss << current_box_value;

            //checking color to know if a pokemon is on the slot or not
            if (current_box_value < 10) {
                //stats.empty++;
                num_empty_slots++;
                boxes_data.push_back(std::nullopt); //empty optional to make sorting easier later
                ss << "\u274c ";    //  "X"
            }
            else {
                if (first_pokemon_slot[0] == SIZE_MAX) {
                    first_pokemon_slot = { row, col };
                }
                //stats.pkmn++;
                boxes_data.push_back(
                    CollectedPokemonInfo{
                        .preferences = &sort_preferences
                    }
                ); //default initialised pokemon to know there is a pokemon here that needs a value
                ss << "\u2705 ";    //  checkbox
            }
        }
        ss << "\n";
    }

    //env.update_stats();
    env.log(ss.str());
    env.add_overlay_log("Empty: " + std::to_string(num_empty_slots) + "/30");

    return first_pokemon_slot;
}

// Read the current summary screen and assign various pokemon info into `cur_pokemon_info`
void read_summary_screen(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    CollectedPokemonInfo& cur_pokemon_info, Language ot_name_language
) {
    VideoOverlaySet video_overlay_set(env.console);

    ImageFloatBox national_dex_number_box(0.448, 0.245, 0.049, 0.04); //pokemon national dex number pos
    ImageFloatBox shiny_symbol_box(0.702, 0.09, 0.04, 0.06); // shiny symbol pos
    ImageFloatBox gmax_tera_symbol_box(0.463, 0.09, 0.04, 0.06); // gmax OR tera symbol pos
    ImageFloatBox origin_symbol_box(0.617, 0.084, 0.044, 0.069); // origin symbol pos
    ImageFloatBox pokemon_box(0.69, 0.18, 0.28, 0.46); // pokemon render pos
    ImageFloatBox level_box(0.546, 0.099, 0.044, 0.041); // Level
    ImageFloatBox ot_id_box(0.782, 0.719, 0.193, 0.046); // OT ID
    ImageFloatBox ot_box(0.492, 0.719, 0.165, 0.049); // OT
    ImageFloatBox nature_box(0.157, 0.783, 0.212, 0.042); // Nature
    ImageFloatBox ability_box(0.158, 0.838, 0.213, 0.042); // Ability
    ImageFloatBox alpha_box(0.787, 0.095, 0.024, 0.046); // Alpha symbol
    ImageFloatBox type_box(0.615, 0.240, 0.071, 0.057); // Type symbols


    video_overlay_set.add(COLOR_WHITE, national_dex_number_box);
    video_overlay_set.add(COLOR_BLUE, shiny_symbol_box);
    video_overlay_set.add(COLOR_RED, gmax_tera_symbol_box);
    video_overlay_set.add(COLOR_RED, alpha_box);
    video_overlay_set.add(COLOR_DARKGREEN, origin_symbol_box);
    video_overlay_set.add(COLOR_DARK_BLUE, pokemon_box);
    video_overlay_set.add(COLOR_RED, level_box);
    video_overlay_set.add(COLOR_RED, ot_id_box);
    video_overlay_set.add(COLOR_RED, ot_box);
    video_overlay_set.add(COLOR_RED, nature_box);
    video_overlay_set.add(COLOR_RED, ability_box);
    video_overlay_set.add(COLOR_RED, type_box);
    BoxGenderDetector::make_overlays(video_overlay_set);


    // Wait for the summary screen transition to end
    FrozenImageDetector frozen_image_detector(COLOR_GREEN, { 0.388, 0.238, 0.109, 0.062 }, Milliseconds(80), 20);
    frozen_image_detector.make_overlays(video_overlay_set);
    wait_until(env.console, context, 5s, { frozen_image_detector });

    VideoSnapshot screen = env.console.video().snapshot();

    const int dex_number = OCR::read_number_waterfill(env.console, extract_box_reference(screen, national_dex_number_box), 0xff808080, 0xffffffff);
    if (dex_number <= 0 || dex_number > static_cast<int>(NATIONAL_DEX_SLUGS().size())) {
        OperationFailedException::fire(
            ErrorReport::SEND_ERROR_REPORT,
            "BoxSorter Check Summary: Unable to read a correct dex number, found: " + std::to_string(dex_number),
            env.console
        );
    }
    cur_pokemon_info.dex_number = (uint16_t)dex_number;
    cur_pokemon_info.name_slug = NATIONAL_DEX_SLUGS()[dex_number - 1];

    const int shiny_stddev_value = (int)image_stddev(extract_box_reference(screen, shiny_symbol_box)).sum();
    const bool is_shiny = shiny_stddev_value > 30;
    cur_pokemon_info.shiny = is_shiny;
    env.console.log("Shiny detection stddev:" + std::to_string(shiny_stddev_value) + " is shiny:" + std::to_string(is_shiny));

    GigantamaxDetector gmax_detector(COLOR_RED, &env.console.overlay(), gmax_tera_symbol_box);
    cur_pokemon_info.gmax = gmax_detector.detect(screen);

    cur_pokemon_info.tera_type = read_pokemon_tera_type(screen, gmax_tera_symbol_box);

    const int alpha_stddev_value = (int)image_stddev(extract_box_reference(screen, alpha_box)).sum();
    const bool is_alpha = alpha_stddev_value > 40;
    cur_pokemon_info.alpha = is_alpha;
    env.console.log("Alpha detection stddev:" + std::to_string(alpha_stddev_value) + " is alpha:" + std::to_string(is_alpha));

    BallReader ball_reader(env.console);
    cur_pokemon_info.ball_slug = ball_reader.read_ball(screen);

    const StatsHuntGenderFilter gender = BoxGenderDetector::detect(screen);
    env.console.log("Gender: " + gender_to_string(gender), COLOR_GREEN);
    cur_pokemon_info.gender = gender;

    const int ot_id = OCR::read_number_waterfill(env.console, extract_box_reference(screen, ot_id_box), 0xff808080, 0xffffffff);
    if (ot_id < 0 || ot_id > 999'999) {
        dump_image(env.console, ProgramInfo(), "ReadSummary_OT", screen);
    }
    cur_pokemon_info.ot_id = ot_id;

    auto [primary_type, secondary_type] = read_pokemon_types(screen, type_box, PokemonTypeGeneration::GEN9);

    cur_pokemon_info.primary_type = primary_type;
    cur_pokemon_info.secondary_type = secondary_type;

    if (ot_name_language != Language::None){
        const std::vector<OCR::TextColorRange>& text_filters = OCR::WHITE_TEXT_FILTERS();
        std::vector<BlackWhiteRgb32Range> bw;
        bw.reserve(text_filters.size());
        for (const auto& f : text_filters){
            bw.push_back({ true, f.mins, f.maxs });
        }

        auto filtered_images = to_blackwhite_rgb32_range(extract_box_reference(screen, ot_box), bw);

        std::string best_raw;
        for (auto& [img, px_count] : filtered_images){
            if (px_count == 0){ 
                continue; 
            }
            std::string candidate = OCR::ocr_read(ot_name_language, img, OCR::PageSegMode::SINGLE_LINE);
            if (!candidate.empty() && best_raw.empty()){
                best_raw = candidate;
            }
        }

        env.log("Raw trainer name: " + best_raw);
        std::string normalized = utf32_to_str(OCR::normalize_utf32(best_raw));
        env.log("Normalized trainer name: " + normalized);
        cur_pokemon_info.ot_name = normalized;
    }

    cur_pokemon_info.origin_mark = OriginMarkReader().read_mark(screen, origin_symbol_box);

    env.add_overlay_log(create_overlay_info(cur_pokemon_info));
    video_overlay_set.clear();

    // NOTE edit when adding new struct members (detections go here likely)

    // level_box
    // nature_box
    // ability_box

    // Press button R to go to next summary screen
    pbf_press_button(context, BUTTON_R, 80ms, 300ms);
    context.wait_for_all_requests();
}

void print_boxes_data(const std::vector<std::optional<CollectedPokemonInfo>>& boxes_data, SingleSwitchProgramEnvironment& env) {
    std::ostringstream ss;
    for (const std::optional<CollectedPokemonInfo>& pokemon : boxes_data) {
        ss << pokemon << "\n";
    }
    env.console.log(ss.str());
}

void exit_menus(SingleSwitchProgramEnvironment& env, ProControllerContext& context, std::chrono::milliseconds video_delay) {
    VideoSnapshot screen = env.console.video().snapshot();
    VideoOverlaySet video_overlay_set(env.console);

    ImageFloatBox select_check(0.495, 0.0045, 0.01, 0.005); // square color to check which mode is active
    FloatPixel image_value = image_stats(extract_box_reference(screen, select_check)).average;

    env.console.log("Color detected from the select square: " + image_value.to_string());

    //if the correct color is not detected, getting out of every possible menu to make sure the program work no matter where you start it in your pokemon home
    video_overlay_set.add(COLOR_BLUE, select_check);
    if (image_value.r <= image_value.g + image_value.b) {
        for (int var = 0; var < 5; ++var) {
            pbf_press_button(context, BUTTON_B, 80ms, video_delay + 80ms);
        }
        context.wait_for_all_requests();
        context.wait_for(std::chrono::milliseconds(video_delay));
        screen = env.console.video().snapshot();
        image_value = image_stats(extract_box_reference(screen, select_check)).average;
        env.console.log("Color detected from the select square: " + image_value.to_string());
        if (image_value.r <= image_value.g + image_value.b) {
            for (int i = 0; i < 2; ++i) {
                pbf_press_button(context, BUTTON_ZR, 80ms, video_delay + 240ms); //additional delay because this animation is slower than the rest
                context.wait_for_all_requests();
                screen = env.console.video().snapshot();
                image_value = image_stats(extract_box_reference(screen, select_check)).average;
                env.console.log("Color detected from the select square: " + image_value.to_string());
                if (image_value.r > image_value.g + image_value.b) {
                    break;
                }
                else if (i == 1) {
                    dump_image(env.console, ProgramInfo(), "SelectSquare", screen);
                    env.console.log("ERROR: Could not find correct color mode please check color logs and timings\n", COLOR_RED);
                    return;
                }
            }
        }
    }

    video_overlay_set.clear();
}

}
}
}
