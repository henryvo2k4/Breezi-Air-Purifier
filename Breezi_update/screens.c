#include <string.h>

#include "screens.h"
#include "images.h"
#include "fonts.h"
#include "actions.h"
#include "vars.h"
#include "styles.h"
#include "ui.h"

#include <string.h>

objects_t objects;
lv_obj_t *tick_value_change_obj;
uint32_t active_theme_index = 0;

void create_screen_main() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.main = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 320, 240);
    lv_obj_set_style_bg_image_src(obj, &img_background, LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            // tron
            lv_obj_t *obj = lv_animimg_create(parent_obj);
            objects.tron = obj;
            lv_obj_set_pos(obj, 10, 10);
            lv_obj_set_size(obj, 135, 135);
            static const lv_image_dsc_t *images[6] = {
                &img_tron1,
                &img_tron2,
                &img_tron3,
                &img_tron4,
                &img_tron5,
                &img_tron6,
            };
            lv_animimg_set_src(obj, (const void **)images, 6);
            lv_animimg_set_duration(obj, 0);
            lv_animimg_set_repeat_count(obj, 1);
            lv_animimg_start(obj);
        }
        {
            // thongtin
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.thongtin = obj;
            lv_obj_set_pos(obj, 160, 16);
            lv_obj_set_size(obj, 141, 120);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0xffd5f8f2), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(0xff205623), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 500, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj0 = obj;
            lv_obj_set_pos(obj, 179, 31);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &ui_font_pop_m, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xff1d5427), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "Main Pollutain");
        }
        {
            // Main_pollutain
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.main_pollutain = obj;
            lv_obj_set_pos(obj, 179, 48);
            lv_obj_set_size(obj, 103, 25);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
            lv_obj_set_style_text_font(obj, &ui_font_pop_b, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xff1e8d7e), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "OZONE");
        }
        {
            // air_is
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.air_is = obj;
            lv_obj_set_pos(obj, 171, 69);
            lv_obj_set_size(obj, 119, 17);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL_CIRCULAR);
            lv_obj_set_style_text_font(obj, &ui_font_pop12, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xff1d5427), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "AIR IS FRESH ");
        }
        {
            // Cach_xu_ly
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.cach_xu_ly = obj;
            lv_obj_set_pos(obj, 171, 83);
            lv_obj_set_size(obj, 119, 49);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_pop12, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xff326835), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "Keep your purifier on low and enjoy the day!");
        }
        {
            // temp
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.temp = obj;
            lv_obj_set_pos(obj, 60, 178);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xff1e8d7e), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_pop_m, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "00°C");
        }
        {
            // humi
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.humi = obj;
            lv_obj_set_pos(obj, 142, 178);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xff1e8d7e), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_pop_m, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "00%");
        }
        {
            // weather
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.weather = obj;
            lv_obj_set_pos(obj, 227, 178);
            lv_obj_set_size(obj, 69, 23);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xff1e8d7e), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_pop_m, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "Cloudy");
        }
        {
            // AQI
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.aqi = obj;
            lv_obj_set_pos(obj, 37, 51);
            lv_obj_set_size(obj, 81, 50);
            lv_obj_set_style_text_font(obj, &ui_font_pop40, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xff3b933f), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "00");
        }
    }
    
    tick_screen_main();
}

void tick_screen_main() {
}



typedef void (*tick_screen_func_t)();
tick_screen_func_t tick_screen_funcs[] = {
    tick_screen_main,
};
void tick_screen(int screen_index) {
    tick_screen_funcs[screen_index]();
}
void tick_screen_by_id(enum ScreensEnum screenId) {
    tick_screen_funcs[screenId - 1]();
}

void create_screens() {
    lv_disp_t *dispp = lv_disp_get_default();
    lv_theme_t *theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), false, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, theme);
    
    create_screen_main();
}
