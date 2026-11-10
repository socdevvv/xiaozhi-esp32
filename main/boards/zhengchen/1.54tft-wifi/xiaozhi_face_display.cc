#include "xiaozhi_face_display.h"

#include "application.h"
#include "device_state.h"
#include "lvgl_theme.h"

#include <esp_log.h>
#include <esp_random.h>

#include <cmath>
#include <cstring>

namespace {
constexpr int kWidth = 240;
constexpr int kHeight = 240;
constexpr int kCenterX = 120;
constexpr int kCenterY = 115;
constexpr uint32_t kBackground = 0x030507;
constexpr uint32_t kText = 0xE7EEF7;
constexpr uint32_t kPupil = 0x071018;
constexpr int kMouthPointCount = 17;
constexpr int kEyeWidth = 31;
constexpr int kEyeCenterY = 102;
constexpr int kEyeOffsetX = 15;

lv_obj_t* MakeObject(lv_obj_t* parent)
{
    lv_obj_t* object = lv_obj_create(parent);
    lv_obj_remove_style_all(object);
    lv_obj_set_scrollbar_mode(object, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(object, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
    lv_obj_set_style_pad_all(object, 0, 0);
    return object;
}

lv_obj_t* MakeLabel(lv_obj_t* parent, const lv_font_t* font, uint32_t color, const char* text)
{
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_label_set_text(label, text);
    return label;
}

lv_color_t MakeColor(uint8_t red, uint8_t green, uint8_t blue)
{
    return lv_color_make(red, green, blue);
}
}

XiaozhiFaceDisplay::~XiaozhiFaceDisplay()
{
    if (timer_ != nullptr) {
        lv_timer_delete(timer_);
        timer_ = nullptr;
    }
}

void XiaozhiFaceDisplay::TimerCallback(lv_timer_t* timer)
{
    auto* display = static_cast<XiaozhiFaceDisplay*>(lv_timer_get_user_data(timer));
    if (display != nullptr) {
        display->Tick();
    }
}

uint32_t XiaozhiFaceDisplay::ColorForState(FaceState state)
{
    switch (state) {
        case FaceState::Idle:
            return 0xF6C177;
        case FaceState::Listening:
            return 0x5EEAD4;
        case FaceState::Thinking:
            return 0xA78BFA;
        case FaceState::Speaking:
            return 0xFFB86B;
        case FaceState::Happy:
            return 0xFDE047;
        case FaceState::Curious:
            return 0x7DD3FC;
        case FaceState::Sleepy:
            return 0x94A3B8;
        case FaceState::Error:
            return 0xFB7185;
        case FaceState::Sad:
            return 0x60A5FA;
        case FaceState::Angry:
            return 0xF97316;
    }
    return 0xF6C177;
}

const char* XiaozhiFaceDisplay::LabelForState(FaceState state)
{
    switch (state) {
        case FaceState::Idle:
            return "IDLE";
        case FaceState::Listening:
            return "LISTENING";
        case FaceState::Thinking:
            return "THINKING";
        case FaceState::Speaking:
            return "SPEAKING";
        case FaceState::Happy:
            return "HAPPY";
        case FaceState::Curious:
            return "CURIOUS";
        case FaceState::Sleepy:
            return "SLEEPY";
        case FaceState::Error:
            return "ERROR";
        case FaceState::Sad:
            return "SAD";
        case FaceState::Angry:
            return "ANGRY";
    }
    return "IDLE";
}

float XiaozhiFaceDisplay::GlowForState(FaceState state)
{
    switch (state) {
        case FaceState::Idle:
            return 0.72f;
        case FaceState::Listening:
            return 1.0f;
        case FaceState::Thinking:
            return 0.90f;
        case FaceState::Speaking:
            return 1.0f;
        case FaceState::Happy:
            return 1.0f;
        case FaceState::Curious:
            return 0.92f;
        case FaceState::Sleepy:
            return 0.50f;
        case FaceState::Error:
            return 1.0f;
        case FaceState::Sad:
            return 0.62f;
        case FaceState::Angry:
            return 1.0f;
    }
    return 0.72f;
}

bool XiaozhiFaceDisplay::StateFromEmotion(const char* emotion, FaceState& state)
{
    if (emotion == nullptr || emotion[0] == '\0') {
        return false;
    }
    if (strcmp(emotion, "happy") == 0 || strcmp(emotion, "laughing") == 0 ||
        strcmp(emotion, "funny") == 0) {
        state = FaceState::Happy;
        return true;
    }
    if (strcmp(emotion, "sad") == 0 || strcmp(emotion, "crying") == 0) {
        state = FaceState::Sad;
        return true;
    }
    if (strcmp(emotion, "angry") == 0 || strcmp(emotion, "annoyed") == 0) {
        state = FaceState::Angry;
        return true;
    }
    if (strcmp(emotion, "curious") == 0 || strcmp(emotion, "surprised") == 0) {
        state = FaceState::Curious;
        return true;
    }
    if (strcmp(emotion, "sleepy") == 0) {
        state = FaceState::Sleepy;
        return true;
    }
    return false;
}

void XiaozhiFaceDisplay::SetupUI()
{
    if (setup_ui_called_) {
        return;
    }

    Display::SetupUI();
    DisplayLockGuard lock(this);

    LvglTheme* theme = static_cast<LvglTheme*>(current_theme_);
    if (theme != nullptr && theme->text_font() != nullptr) {
        text_font_ = theme->text_font()->font();
    }
    if (text_font_ == nullptr) {
        text_font_ = LV_FONT_DEFAULT;
    }

    lv_obj_t* screen = lv_screen_active();
    lv_obj_clean(screen);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(kBackground), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(screen, text_font_, 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(kText), 0);

    halo_outer_ = MakeObject(screen);
    lv_obj_set_size(halo_outer_, 220, 220);
    lv_obj_set_pos(halo_outer_, 10, 5);
    lv_obj_set_style_radius(halo_outer_, 110, 0);
    lv_obj_set_style_bg_color(halo_outer_, lv_color_hex(0xF6C177), 0);
    lv_obj_set_style_bg_opa(halo_outer_, 20, 0);

    halo_mid_ = MakeObject(screen);
    lv_obj_set_size(halo_mid_, 170, 170);
    lv_obj_set_pos(halo_mid_, 35, 30);
    lv_obj_set_style_radius(halo_mid_, 85, 0);
    lv_obj_set_style_bg_color(halo_mid_, lv_color_hex(0xF6C177), 0);
    lv_obj_set_style_bg_opa(halo_mid_, 18, 0);

    ring_ = MakeObject(screen);
    lv_obj_set_size(ring_, 196, 196);
    lv_obj_set_pos(ring_, 22, 17);
    lv_obj_set_style_radius(ring_, 98, 0);
    lv_obj_set_style_bg_opa(ring_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ring_, 1, 0);
    lv_obj_set_style_border_color(ring_, lv_color_hex(0xF6C177), 0);
    lv_obj_set_style_border_opa(ring_, 60, 0);

    for (int i = 0; i < 22; ++i) {
        particles_[i] = MakeObject(screen);
        int size = (i % 5 == 0) ? 3 : 2;
        lv_obj_set_size(particles_[i], size, size);
        lv_obj_set_style_radius(particles_[i], size, 0);
        lv_obj_set_style_bg_color(particles_[i], lv_color_hex(0xF6C177), 0);
        lv_obj_set_style_bg_opa(particles_[i], 90, 0);
    }

    face_group_ = MakeObject(screen);
    lv_obj_set_size(face_group_, kWidth, kHeight);
    lv_obj_set_pos(face_group_, 0, 0);
    lv_obj_set_style_bg_opa(face_group_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_transform_pivot_x(face_group_, kCenterX, 0);
    lv_obj_set_style_transform_pivot_y(face_group_, kCenterY, 0);
    lv_obj_add_flag(face_group_, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    auto make_eye = [this](int center_x, lv_obj_t*& eye, lv_obj_t*& pupil, lv_obj_t*& glint) {
        eye = MakeObject(face_group_);
        lv_obj_set_size(eye, kEyeWidth, 37);
        lv_obj_set_pos(eye, center_x - kEyeOffsetX, kEyeCenterY - 18);
        lv_obj_set_style_radius(eye, 16, 0);
        lv_obj_set_style_bg_color(eye, lv_color_white(), 0);
        lv_obj_set_style_bg_grad_color(eye, lv_color_hex(0xF6C177), 0);
        lv_obj_set_style_bg_grad_dir(eye, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_bg_opa(eye, LV_OPA_COVER, 0);
        lv_obj_set_style_shadow_width(eye, 18, 0);
        lv_obj_set_style_shadow_spread(eye, 1, 0);
        lv_obj_set_style_shadow_color(eye, lv_color_hex(0xF6C177), 0);
        lv_obj_set_style_shadow_opa(eye, 70, 0);
        lv_obj_add_flag(eye, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

        pupil = MakeObject(eye);
        lv_obj_set_size(pupil, 12, 18);
        lv_obj_set_style_radius(pupil, 6, 0);
        lv_obj_set_style_bg_color(pupil, lv_color_hex(kPupil), 0);
        lv_obj_set_style_bg_opa(pupil, LV_OPA_COVER, 0);
        lv_obj_align(pupil, LV_ALIGN_CENTER, 1, 4);

        glint = MakeObject(eye);
        lv_obj_set_size(glint, 4, 4);
        lv_obj_set_style_radius(glint, 2, 0);
        lv_obj_set_style_bg_color(glint, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(glint, 220, 0);
        lv_obj_align(glint, LV_ALIGN_CENTER, -4, -7);
    };

    make_eye(86, left_eye_, left_pupil_, left_glint_);
    make_eye(154, right_eye_, right_pupil_, right_glint_);

    happy_left_points_[0] = {71, 108};
    happy_left_points_[1] = {78, 96};
    happy_left_points_[2] = {86, 91};
    happy_left_points_[3] = {94, 96};
    happy_left_points_[4] = {101, 108};
    happy_right_points_[0] = {139, 108};
    happy_right_points_[1] = {146, 96};
    happy_right_points_[2] = {154, 91};
    happy_right_points_[3] = {162, 96};
    happy_right_points_[4] = {169, 108};

    auto make_line = [this](lv_obj_t*& line, lv_point_precise_t* points, int count, int width) {
        line = MakeObject(face_group_);
        lv_obj_set_size(line, kWidth, kHeight);
        lv_obj_set_pos(line, 0, 0);
        lv_obj_set_style_bg_opa(line, LV_OPA_TRANSP, 0);
        lv_obj_set_style_line_width(line, width, 0);
        lv_obj_set_style_line_color(line, lv_color_hex(0xF6C177), 0);
        lv_obj_set_style_line_rounded(line, true, 0);
        lv_obj_set_style_line_opa(line, LV_OPA_COVER, 0);
        lv_line_set_points(line, points, count);
    };

    make_line(left_happy_, happy_left_points_, 5, 4);
    make_line(right_happy_, happy_right_points_, 5, 4);
    make_line(mouth_line_, mouth_points_, 5, 3);
    lv_obj_add_flag(left_happy_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(right_happy_, LV_OBJ_FLAG_HIDDEN);

    mouth_circle_ = MakeObject(face_group_);
    lv_obj_set_size(mouth_circle_, 16, 16);
    lv_obj_set_pos(mouth_circle_, kCenterX - 8, 143);
    lv_obj_set_style_radius(mouth_circle_, 8, 0);
    lv_obj_set_style_bg_opa(mouth_circle_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(mouth_circle_, 3, 0);
    lv_obj_set_style_border_color(mouth_circle_, lv_color_hex(0xF6C177), 0);
    lv_obj_add_flag(mouth_circle_, LV_OBJ_FLAG_HIDDEN);

    mouth_dot_ = MakeObject(face_group_);
    lv_obj_set_size(mouth_dot_, 8, 8);
    lv_obj_set_pos(mouth_dot_, kCenterX - 4, 147);
    lv_obj_set_style_radius(mouth_dot_, 4, 0);
    lv_obj_set_style_bg_color(mouth_dot_, lv_color_hex(0xF6C177), 0);
    lv_obj_set_style_bg_opa(mouth_dot_, LV_OPA_COVER, 0);
    lv_obj_add_flag(mouth_dot_, LV_OBJ_FLAG_HIDDEN);

    state_label_ = MakeLabel(screen, text_font_, kText, "IDLE");
    lv_obj_align(state_label_, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_text_letter_space(state_label_, 1, 0);
    lv_obj_set_style_text_opa(state_label_, 180, 0);

    subtitle_panel_ = MakeObject(screen);
    lv_obj_set_size(subtitle_panel_, 224, 46);
    lv_obj_align(subtitle_panel_, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_set_style_radius(subtitle_panel_, 12, 0);
    lv_obj_set_style_bg_color(subtitle_panel_, lv_color_hex(0x05080C), 0);
    lv_obj_set_style_bg_opa(subtitle_panel_, 210, 0);
    lv_obj_set_style_border_width(subtitle_panel_, 1, 0);
    lv_obj_set_style_border_color(subtitle_panel_, lv_color_hex(0xF6C177), 0);
    lv_obj_set_style_border_opa(subtitle_panel_, 80, 0);
    lv_obj_set_style_pad_all(subtitle_panel_, 6, 0);
    lv_obj_add_flag(subtitle_panel_, LV_OBJ_FLAG_HIDDEN);

    subtitle_label_ = MakeLabel(subtitle_panel_, text_font_, kText, "");
    lv_obj_set_width(subtitle_label_, 210);
    lv_obj_set_style_text_align(subtitle_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(subtitle_label_, LV_LABEL_LONG_WRAP);
    lv_obj_center(subtitle_label_);

    if (high_temp_popup_requested_) {
        temp_popup_ = MakeObject(screen);
        lv_obj_set_size(temp_popup_, 216, 30);
        lv_obj_align(temp_popup_, LV_ALIGN_TOP_MID, 0, 34);
        lv_obj_set_style_radius(temp_popup_, 8, 0);
        lv_obj_set_style_bg_color(temp_popup_, lv_color_hex(0xFCA5A5), 0);
        lv_obj_set_style_bg_opa(temp_popup_, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(temp_popup_, 0, 0);
        lv_obj_add_flag(temp_popup_, LV_OBJ_FLAG_HIDDEN);
        temp_label_ = MakeLabel(temp_popup_, text_font_, 0x450A0A, "HIGH TEMP");
        lv_obj_center(temp_label_);
    }

    uint32_t now = lv_tick_get();
    state_started_ = now;
    idle_started_ = now;
    next_blink_ = now + 2600;
    uint32_t initial_color = ColorForState(state_);
    target_color_ = {
        static_cast<uint8_t>((initial_color >> 16) & 0xFF),
        static_cast<uint8_t>((initial_color >> 8) & 0xFF),
        static_cast<uint8_t>(initial_color & 0xFF),
    };
    color_ = target_color_;
    glow_ = GlowForState(state_);
    lv_label_set_text(state_label_, LabelForState(state_));
    ApplyColor();
    UpdateEyes();
    UpdateMouth();
    UpdatePose();
    UpdateParticles();

    if (high_temp_ >= high_temp_threshold_) {
        ShowHighTempWarning();
    }

    timer_ = lv_timer_create(TimerCallback, 50, this);
    if (timer_ == nullptr) {
        ESP_LOGE("XiaozhiFaceDisplay", "Failed to create animation timer");
    }
}

void XiaozhiFaceDisplay::Tick()
{
    if (!setup_ui_called_ || halo_outer_ == nullptr) {
        return;
    }

    uint32_t now = lv_tick_get();
    FaceState desired = FaceState::Idle;
    DeviceState device_state = Application::GetInstance().GetDeviceState();

    switch (device_state) {
        case kDeviceStateListening:
            desired = FaceState::Listening;
            break;
        case kDeviceStateSpeaking:
        case kDeviceStateNotifying:
            desired = FaceState::Speaking;
            break;
        case kDeviceStateStarting:
        case kDeviceStateWifiConfiguring:
        case kDeviceStateConnecting:
        case kDeviceStateUpgrading:
        case kDeviceStateActivating:
            desired = FaceState::Thinking;
            break;
        case kDeviceStateAudioTesting:
            desired = FaceState::Curious;
            break;
        case kDeviceStateFatalError:
            desired = FaceState::Error;
            break;
        case kDeviceStateIdle:
            if (static_cast<int32_t>(now - idle_started_) >= 45000) {
                desired = FaceState::Sleepy;
            }
            break;
        case kDeviceStateUnknown:
        default:
            break;
    }

    if (emotion_until_ != 0) {
        if (static_cast<int32_t>(now - emotion_until_) >= 0) {
            emotion_until_ = 0;
        } else {
            desired = emotion_state_;
        }
    }

    if (desired != state_) {
        UpdateState(desired);
    }

    UpdateColor();
    UpdateEyes();
    UpdateMouth();
    UpdatePose();
    UpdateParticles();
    UpdateSubtitle();
}

void XiaozhiFaceDisplay::UpdateState(FaceState state)
{
    state_ = state;
    state_started_ = lv_tick_get();
    uint32_t color = ColorForState(state);
    target_color_ = {
        static_cast<uint8_t>((color >> 16) & 0xFF),
        static_cast<uint8_t>((color >> 8) & 0xFF),
        static_cast<uint8_t>(color & 0xFF),
    };
    glow_ = GlowForState(state);

    if (state == FaceState::Idle) {
        idle_started_ = state_started_;
    }

    if (state_label_ == nullptr) {
        return;
    }

    lv_label_set_text(state_label_, LabelForState(state));
    ApplyColor();
}

void XiaozhiFaceDisplay::UpdateColor()
{
    if (halo_outer_ == nullptr) {
        return;
    }

    auto step = [](uint8_t current, uint8_t target) -> uint8_t {
        int difference = static_cast<int>(target) - static_cast<int>(current);
        if (difference == 0) {
            return current;
        }
        int amount = difference / 3;
        if (amount == 0) {
            amount = difference > 0 ? 1 : -1;
        }
        return static_cast<uint8_t>(current + amount);
    };

    Color next = {
        step(color_.r, target_color_.r),
        step(color_.g, target_color_.g),
        step(color_.b, target_color_.b),
    };

    if (next.r == color_.r && next.g == color_.g && next.b == color_.b) {
        return;
    }

    color_ = next;
    ApplyColor();
}

void XiaozhiFaceDisplay::ApplyColor()
{
    lv_color_t color = MakeColor(color_.r, color_.g, color_.b);

    lv_obj_set_style_bg_color(halo_outer_, color, 0);
    lv_obj_set_style_bg_color(halo_mid_, color, 0);
    lv_obj_set_style_border_color(ring_, color, 0);
    lv_obj_set_style_bg_opa(halo_outer_, static_cast<lv_opa_t>(8 + glow_ * 20.0f), 0);
    lv_obj_set_style_bg_opa(halo_mid_, static_cast<lv_opa_t>(6 + glow_ * 16.0f), 0);
    lv_obj_set_style_border_opa(ring_, static_cast<lv_opa_t>(18 + glow_ * 52.0f), 0);

    lv_obj_set_style_bg_grad_color(left_eye_, color, 0);
    lv_obj_set_style_bg_grad_color(right_eye_, color, 0);
    lv_obj_set_style_shadow_color(left_eye_, color, 0);
    lv_obj_set_style_shadow_color(right_eye_, color, 0);
    lv_obj_set_style_line_color(left_happy_, color, 0);
    lv_obj_set_style_line_color(right_happy_, color, 0);
    lv_obj_set_style_line_color(mouth_line_, color, 0);
    lv_obj_set_style_border_color(mouth_circle_, color, 0);
    lv_obj_set_style_bg_color(mouth_dot_, color, 0);
    lv_obj_set_style_text_color(state_label_, color, 0);
    lv_obj_set_style_border_color(subtitle_panel_, color, 0);

    for (int i = 0; i < 22; ++i) {
        if (particles_[i] != nullptr) {
            lv_obj_set_style_bg_color(particles_[i], color, 0);
        }
    }
}

void XiaozhiFaceDisplay::UpdateEyes()
{
    if (left_eye_ == nullptr) {
        return;
    }

    bool happy = state_ == FaceState::Happy;
    if (happy) {
        lv_obj_add_flag(left_eye_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(right_eye_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(left_happy_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(right_happy_, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_remove_flag(left_eye_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(right_eye_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(left_happy_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(right_happy_, LV_OBJ_FLAG_HIDDEN);

    uint32_t now = lv_tick_get();
    bool blink_enabled = state_ != FaceState::Error && state_ != FaceState::Sad &&
                         state_ != FaceState::Angry && state_ != FaceState::Sleepy;
    if (!blink_enabled) {
        blink_active_ = false;
    } else if (!blink_active_ && static_cast<int32_t>(now - next_blink_) >= 0) {
        blink_active_ = true;
        blink_start_ = now;
    }

    float blink = 1.0f;
    if (blink_active_) {
        float progress = static_cast<float>(now - blink_start_) / 180.0f;
        if (progress >= 1.0f) {
            blink_active_ = false;
            next_blink_ = now + 2400 + (esp_random() % 4200);
        } else if (progress < 0.5f) {
            blink = 1.0f - progress * 2.0f;
        } else {
            blink = (progress - 0.5f) * 2.0f;
        }
    }

    float base_height = 37.0f;
    switch (state_) {
        case FaceState::Listening:
            base_height = 42.0f;
            break;
        case FaceState::Speaking:
            base_height = 27.0f;
            break;
        case FaceState::Curious:
            base_height = 38.0f;
            break;
        case FaceState::Sleepy:
            base_height = 3.0f;
            break;
        case FaceState::Sad:
            base_height = 33.0f;
            break;
        case FaceState::Angry:
            base_height = 23.0f;
            break;
        default:
            break;
    }

    int left_height = static_cast<int>(base_height * blink);
    int right_height = left_height;
    if (state_ == FaceState::Curious) {
        left_height = static_cast<int>(left_height * 0.84f);
    }
    if (left_height < 3) {
        left_height = 3;
    }
    if (right_height < 3) {
        right_height = 3;
    }

    int left_rotation = 0;
    int right_rotation = 0;
    if (state_ == FaceState::Sad) {
        left_rotation = 10;
        right_rotation = -10;
    } else if (state_ == FaceState::Angry) {
        left_rotation = -9;
        right_rotation = 9;
    }

    int gaze_x = 0;
    int gaze_y = 0;
    if (state_ == FaceState::Thinking) {
        gaze_y = -4;
    }

    lv_obj_set_size(left_eye_, kEyeWidth, left_height);
    lv_obj_set_pos(left_eye_, 86 - kEyeOffsetX, kEyeCenterY - left_height / 2);
    lv_obj_set_size(right_eye_, kEyeWidth, right_height);
    lv_obj_set_pos(right_eye_, 154 - kEyeOffsetX, kEyeCenterY - right_height / 2);
    lv_obj_set_style_transform_pivot_x(left_eye_, kEyeOffsetX, 0);
    lv_obj_set_style_transform_pivot_y(left_eye_, left_height / 2, 0);
    lv_obj_set_style_transform_pivot_x(right_eye_, kEyeOffsetX, 0);
    lv_obj_set_style_transform_pivot_y(right_eye_, right_height / 2, 0);
    lv_obj_set_style_transform_rotation(left_eye_, left_rotation * 10, 0);
    lv_obj_set_style_transform_rotation(right_eye_, right_rotation * 10, 0);
    lv_obj_set_style_shadow_width(left_eye_, left_height < 12 ? 8 : 18, 0);
    lv_obj_set_style_shadow_width(right_eye_, right_height < 12 ? 8 : 18, 0);

    bool show_pupils = state_ != FaceState::Sleepy && left_height >= 12 && right_height >= 12;
    lv_obj_align(left_pupil_, LV_ALIGN_CENTER, gaze_x + 1, gaze_y + 4);
    lv_obj_align(right_pupil_, LV_ALIGN_CENTER, gaze_x + 1, gaze_y + 4);
    lv_obj_align(left_glint_, LV_ALIGN_CENTER, -4, -7);
    lv_obj_align(right_glint_, LV_ALIGN_CENTER, -4, -7);
    if (show_pupils) {
        lv_obj_remove_flag(left_pupil_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(right_pupil_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(left_glint_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(right_glint_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(left_pupil_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(right_pupil_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(left_glint_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(right_glint_, LV_OBJ_FLAG_HIDDEN);
    }
}

void XiaozhiFaceDisplay::UpdateMouth()
{
    if (mouth_line_ == nullptr) {
        return;
    }

    bool show_line = true;
    bool show_circle = false;
    bool show_dot = false;
    int point_count = 5;
    uint32_t now = lv_tick_get();

    switch (state_) {
        case FaceState::Listening:
            show_line = false;
            show_dot = true;
            break;
        case FaceState::Thinking:
            show_line = false;
            show_dot = true;
            break;
        case FaceState::Speaking:
            point_count = kMouthPointCount;
            for (int i = 0; i < point_count; ++i) {
                float x = 104.0f + i * 2.0f;
                float y = 151.0f + sinf(static_cast<float>(now) * 0.012f + i * 0.60f) * 5.0f;
                mouth_points_[i].x = static_cast<int32_t>(x);
                mouth_points_[i].y = static_cast<int32_t>(y);
            }
            break;
        case FaceState::Happy:
            point_count = kMouthPointCount;
            for (int i = 0; i < point_count; ++i) {
                float dx = static_cast<float>(i - 8) * 2.0f;
                float y = 149.0f + 8.0f * (1.0f - (dx / 16.0f) * (dx / 16.0f));
                mouth_points_[i].x = static_cast<int32_t>(104.0f + i * 2.0f);
                mouth_points_[i].y = static_cast<int32_t>(y);
            }
            break;
        case FaceState::Curious:
            show_line = false;
            show_circle = true;
            break;
        case FaceState::Sad:
            point_count = kMouthPointCount;
            for (int i = 0; i < point_count; ++i) {
                float dx = static_cast<float>(i - 8) * 2.0f;
                float y = 154.0f - 8.0f * (1.0f - (dx / 16.0f) * (dx / 16.0f));
                mouth_points_[i].x = static_cast<int32_t>(104.0f + i * 2.0f);
                mouth_points_[i].y = static_cast<int32_t>(y);
            }
            break;
        default:
            for (int i = 0; i < 5; ++i) {
                mouth_points_[i].x = 112 + i * 4;
                mouth_points_[i].y = 151;
            }
            break;
    }

    lv_line_set_points(mouth_line_, mouth_points_, point_count);
    if (show_line) {
        lv_obj_remove_flag(mouth_line_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(mouth_line_, LV_OBJ_FLAG_HIDDEN);
    }
    if (show_circle) {
        lv_obj_remove_flag(mouth_circle_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(mouth_circle_, LV_OBJ_FLAG_HIDDEN);
    }
    if (show_dot) {
        lv_obj_remove_flag(mouth_dot_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(mouth_dot_, LV_OBJ_FLAG_HIDDEN);
    }
}

void XiaozhiFaceDisplay::UpdatePose()
{
    if (face_group_ == nullptr) {
        return;
    }

    float time = static_cast<float>(lv_tick_get()) / 1000.0f;
    float breathe = sinf(time * 1.55f) * 1.5f;
    float bounce = 0.0f;
    float rotation = 0.0f;
    int jitter_x = 0;
    int jitter_y = 0;

    switch (state_) {
        case FaceState::Idle:
            rotation = sinf(time * 1.5f) * 4.0f;
            break;
        case FaceState::Listening:
            rotation = sinf(time * 2.0f) * 6.0f;
            break;
        case FaceState::Thinking:
            rotation = -20.0f;
            break;
        case FaceState::Speaking:
            rotation = sinf(time * 3.2f) * 8.0f;
            break;
        case FaceState::Happy:
            bounce = fabsf(sinf(time * 7.5f)) * 3.2f;
            break;
        case FaceState::Curious:
            rotation = sinf(time * 2.1f) * 43.0f;
            break;
        case FaceState::Sleepy:
            breathe += 2.0f;
            break;
        case FaceState::Error:
            jitter_x = static_cast<int>(esp_random() % 5) - 2;
            jitter_y = static_cast<int>(esp_random() % 5) - 2;
            rotation = static_cast<int>(esp_random() % 17) - 8;
            break;
        case FaceState::Sad:
            rotation = -26.0f;
            break;
        case FaceState::Angry:
            rotation = sinf(time * 18.0f) * 12.0f;
            break;
    }

    lv_obj_set_pos(face_group_, jitter_x, static_cast<int>(breathe + bounce) + jitter_y);
    lv_obj_set_style_transform_rotation(face_group_, static_cast<int>(rotation), 0);
}

void XiaozhiFaceDisplay::UpdateParticles()
{
    if (particles_[0] == nullptr) {
        return;
    }

    uint32_t now = lv_tick_get();
    float time = static_cast<float>(now) / 1000.0f;
    float speed = 1.0f;
    if (state_ == FaceState::Thinking) {
        speed = 1.5f;
    } else if (state_ == FaceState::Sleepy) {
        speed = 0.25f;
    } else if (state_ == FaceState::Error) {
        speed = 2.0f;
    }

    for (int i = 0; i < 22; ++i) {
        float angle = i * 2.399963f + time * speed * 0.12f;
        int radius = 66 + (i * 17) % 43;
        int x = kCenterX + static_cast<int>(cosf(angle) * radius);
        int y = kCenterY + static_cast<int>(sinf(angle) * radius * 0.92f);
        lv_obj_set_pos(particles_[i], x - 1, y - 1);
    }
}

void XiaozhiFaceDisplay::UpdateSubtitle()
{
    if (!subtitle_visible_ || subtitle_panel_ == nullptr) {
        return;
    }
    if (static_cast<int32_t>(lv_tick_get() - subtitle_until_) >= 0) {
        subtitle_visible_ = false;
        lv_obj_add_flag(subtitle_panel_, LV_OBJ_FLAG_HIDDEN);
    }
}

void XiaozhiFaceDisplay::SetStatus(const char* status)
{
    (void)status;
}

void XiaozhiFaceDisplay::SetEmotion(const char* emotion)
{
    FaceState state = FaceState::Idle;
    if (!StateFromEmotion(emotion, state)) {
        emotion_until_ = 0;
        return;
    }

    DisplayLockGuard lock(this);
    emotion_state_ = state;
    emotion_until_ = lv_tick_get() + 2600;
    UpdateState(state);
}

void XiaozhiFaceDisplay::SetChatMessage(const char* role, const char* content)
{
    (void)role;
    if (subtitle_label_ == nullptr) {
        return;
    }

    DisplayLockGuard lock(this);
    if (content == nullptr || content[0] == '\0' || hide_subtitle_) {
        subtitle_visible_ = false;
        subtitle_until_ = 0;
        lv_obj_add_flag(subtitle_panel_, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_label_set_text(subtitle_label_, content);
    lv_obj_remove_flag(subtitle_panel_, LV_OBJ_FLAG_HIDDEN);
    subtitle_visible_ = true;
    subtitle_until_ = lv_tick_get() + 6500;
}

void XiaozhiFaceDisplay::ClearChatMessages()
{
    if (subtitle_label_ == nullptr) {
        return;
    }

    DisplayLockGuard lock(this);
    lv_label_set_text(subtitle_label_, "");
    subtitle_visible_ = false;
    subtitle_until_ = 0;
    lv_obj_add_flag(subtitle_panel_, LV_OBJ_FLAG_HIDDEN);
}

void XiaozhiFaceDisplay::ShowNotification(const char* notification, int duration_ms)
{
    if (subtitle_label_ == nullptr) {
        return;
    }

    DisplayLockGuard lock(this);
    if (notification == nullptr || notification[0] == '\0') {
        return;
    }

    lv_label_set_text(subtitle_label_, notification);
    lv_obj_remove_flag(subtitle_panel_, LV_OBJ_FLAG_HIDDEN);
    subtitle_visible_ = true;
    subtitle_until_ = lv_tick_get() + (duration_ms > 0 ? duration_ms : 3000);
}

void XiaozhiFaceDisplay::UpdateStatusBar(bool update_all)
{
    (void)update_all;
}

void XiaozhiFaceDisplay::SetTheme(Theme* theme)
{
    if (theme == nullptr) {
        return;
    }

    DisplayLockGuard lock(this);
    current_theme_ = theme;

    LvglTheme* lvgl_theme = static_cast<LvglTheme*>(theme);
    if (lvgl_theme != nullptr && lvgl_theme->text_font() != nullptr) {
        const lv_font_t* font = lvgl_theme->text_font()->font();
        if (font != nullptr) {
            text_font_ = font;
            if (state_label_ != nullptr) {
                lv_obj_set_style_text_font(state_label_, font, 0);
            }
            if (subtitle_label_ != nullptr) {
                lv_obj_set_style_text_font(subtitle_label_, font, 0);
            }
            if (temp_label_ != nullptr) {
                lv_obj_set_style_text_font(temp_label_, font, 0);
            }
        }
    }
}

void XiaozhiFaceDisplay::SetHideSubtitle(bool hide)
{
    DisplayLockGuard lock(this);
    hide_subtitle_ = hide;
    if (subtitle_panel_ == nullptr) {
        return;
    }

    if (hide) {
        subtitle_visible_ = false;
        lv_obj_add_flag(subtitle_panel_, LV_OBJ_FLAG_HIDDEN);
    } else if (subtitle_label_ != nullptr) {
        const char* text = lv_label_get_text(subtitle_label_);
        if (text != nullptr && text[0] != '\0') {
            lv_obj_remove_flag(subtitle_panel_, LV_OBJ_FLAG_HIDDEN);
            subtitle_visible_ = true;
            subtitle_until_ = lv_tick_get() + 5000;
        }
    }
}

void XiaozhiFaceDisplay::SetupHighTempWarningPopup()
{
    high_temp_popup_requested_ = true;
}

void XiaozhiFaceDisplay::UpdateHighTempWarning(float chip_temp, float threshold)
{
    high_temp_ = chip_temp;
    high_temp_threshold_ = threshold;
    if (temp_popup_ == nullptr) {
        return;
    }

    if (chip_temp >= threshold) {
        ShowHighTempWarning();
    } else {
        HideHighTempWarning();
    }
}

void XiaozhiFaceDisplay::ShowHighTempWarning()
{
    if (temp_popup_ != nullptr) {
        lv_obj_remove_flag(temp_popup_, LV_OBJ_FLAG_HIDDEN);
    }
}

void XiaozhiFaceDisplay::HideHighTempWarning()
{
    if (temp_popup_ != nullptr) {
        lv_obj_add_flag(temp_popup_, LV_OBJ_FLAG_HIDDEN);
    }
}
