#pragma once

#include "display/lcd_display.h"

#include <lvgl.h>

class XiaozhiFaceDisplay : public SpiLcdDisplay {
public:
    using SpiLcdDisplay::SpiLcdDisplay;
    using LvglDisplay::ShowNotification;

    ~XiaozhiFaceDisplay() override;

    void SetupUI() override;
    void SetStatus(const char* status) override;
    void SetEmotion(const char* emotion) override;
    void SetChatMessage(const char* role, const char* content) override;
    void ClearChatMessages() override;
    void ShowNotification(const char* notification, int duration_ms = 3000) override;
    void UpdateStatusBar(bool update_all = false) override;
    void SetTheme(Theme* theme) override;
    void SetHideSubtitle(bool hide) override;

    void SetupHighTempWarningPopup();
    void UpdateHighTempWarning(float chip_temp, float threshold = 75.0f);
    void ShowHighTempWarning();
    void HideHighTempWarning();

private:
    enum class FaceState {
        Idle,
        Listening,
        Thinking,
        Speaking,
        Happy,
        Curious,
        Sleepy,
        Error,
        Sad,
        Angry,
    };

    struct Color {
        uint8_t r;
        uint8_t g;
        uint8_t b;
    };

    static void TimerCallback(lv_timer_t* timer);
    static uint32_t ColorForState(FaceState state);
    static const char* LabelForState(FaceState state);
    static float GlowForState(FaceState state);
    static bool StateFromEmotion(const char* emotion, FaceState& state);
    void Tick();
    void UpdateState(FaceState state);
    void UpdateColor();
    void ApplyColor();
    void UpdateEyes();
    void UpdateMouth();
    void UpdatePose();
    void UpdateParticles();
    void UpdateSubtitle();

    lv_obj_t* halo_outer_ = nullptr;
    lv_obj_t* halo_mid_ = nullptr;
    lv_obj_t* ring_ = nullptr;
    lv_obj_t* face_group_ = nullptr;
    lv_obj_t* left_eye_ = nullptr;
    lv_obj_t* right_eye_ = nullptr;
    lv_obj_t* left_pupil_ = nullptr;
    lv_obj_t* right_pupil_ = nullptr;
    lv_obj_t* left_glint_ = nullptr;
    lv_obj_t* right_glint_ = nullptr;
    lv_obj_t* left_happy_ = nullptr;
    lv_obj_t* right_happy_ = nullptr;
    lv_obj_t* mouth_line_ = nullptr;
    lv_obj_t* mouth_circle_ = nullptr;
    lv_obj_t* mouth_dot_ = nullptr;
    lv_obj_t* state_label_ = nullptr;
    lv_obj_t* subtitle_panel_ = nullptr;
    lv_obj_t* subtitle_label_ = nullptr;
    lv_obj_t* temp_popup_ = nullptr;
    lv_obj_t* temp_label_ = nullptr;
    lv_obj_t* particles_[22] = {};

    lv_point_precise_t mouth_points_[17] = {};
    lv_point_precise_t happy_left_points_[5] = {};
    lv_point_precise_t happy_right_points_[5] = {};

    lv_timer_t* timer_ = nullptr;
    const lv_font_t* text_font_ = nullptr;

    FaceState state_ = FaceState::Idle;
    FaceState emotion_state_ = FaceState::Idle;
    Color color_ = {0xF6, 0xC1, 0x77};
    Color target_color_ = {0xF6, 0xC1, 0x77};
    float glow_ = 0.72f;

    uint32_t state_started_ = 0;
    uint32_t idle_started_ = 0;
    uint32_t emotion_until_ = 0;
    uint32_t subtitle_until_ = 0;
    uint32_t next_blink_ = 0;
    uint32_t blink_start_ = 0;

    float high_temp_ = 0.0f;
    float high_temp_threshold_ = 75.0f;

    bool blink_active_ = false;
    bool subtitle_visible_ = false;
    bool hide_subtitle_ = false;
    bool high_temp_popup_requested_ = false;
};
