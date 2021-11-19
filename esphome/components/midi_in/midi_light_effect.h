#pragma once

#include "esphome/core/component.h"
#include "esphome/components/light/addressable_light_effect.h"
#include "midi_in.h"

namespace esphome {
namespace midi_in {

class MidiLightEffect : public light::AddressableLightEffect {
 public:
  MidiLightEffect(midi_in::MidiInComponent *midi, const std::string &name);

 public:
  void start() override;
  void stop() override;
  void apply(light::AddressableLight &it, const Color &current_color) override;

 public:
  void set_start_note(uint8_t start_note) { this->start_note_ = start_note; }
  void set_keys(uint8_t keys) { this->keys_ = keys; }
  void set_note_on_fade(uint32_t note_on_fade) { this->note_on_fade_ = note_on_fade; }
  void set_note_off_fade(uint32_t note_off_fade) { this->note_off_fade_ = note_off_fade; }

 protected:
  midi_in::MidiInComponent *midi_;

 protected:
  uint8_t start_note_;
  uint8_t keys_;
  uint32_t note_on_fade_;
  uint32_t note_off_fade_;

 protected:
    // This looks crazy, but it reduces to 6x^5 - 15x^4 + 10x^3 which is just a smooth sigmoid-like
    // transition from 0 to 1 on x = [0, 1]
    static float smoothed_progress(float x) { return x * x * x * (x * (x * 6.0f - 15.0f) + 10.0f); }

    void set_background_color_(const Color &current_color, const uint32_t length)
    {
        //ESP_LOGD(TAG, "apply: %#08x (initial_run: %i)", current_color.raw_32, initial_run ? 1 : 0);

        const Color background_color = current_color * 128;
        uint32_t now = millis();

        for (int i = 0; i < led_transitions_.size(); i++)
        {
            auto led_color = this->get_interpolated_color_(i, now);
            this->set_led_transition_(i, 
                led_color, 
                background_color,
                length, 
                now);
        }
    }

    void set_background_rainbow_(light::AddressableLight *it, const Color &current_color, const uint32_t length)
    {
        //ESP_LOGD(TAG, "apply: %#08x (initial_run: %i)", current_color.raw_32, initial_run ? 1 : 0);

        uint32_t now = millis();

        light::ESPHSVColor bachground_hsv;
        bachground_hsv.value = 128;
        bachground_hsv.saturation = 240;
        uint16_t hue = (now * 10) % 0xFFFF;
        const uint16_t add = 0xFFFF / 50;
        
        for (int i = 0; i < led_transitions_.size(); i++)
        {
            bachground_hsv.hue = hue >> 8;
            this->led_transitions_[i].background = bachground_hsv.to_rgb();
            Color led_color = it->get(i).get();// this->get_interpolated_color_(i, now);
            this->set_led_transition_(i, 
                led_color, 
                this->led_transitions_[i].background,
                length, 
                now);
            
            hue += add;
        }
    }

    void set_led_transition_(const int i, Color start_color, Color target_color, const uint32_t length, const uint32_t now)
    {
        //ESP_LOGD(TAG, "apply: %#08x (initial_run: %i)", current_color.raw_32, initial_run ? 1 : 0);

        this->led_transitions_[i].start = start_color;
        this->led_transitions_[i].target = target_color;
        this->led_transitions_[i].start_time = now;
        this->led_transitions_[i].length = length;
    }

    Color get_interpolated_color_(const int i, const uint32_t now)
    {
        float p = this->get_progress_(this->led_transitions_[i].start_time, this->led_transitions_[i].length, now);
        float v = MidiLightEffect::smoothed_progress(p);
        return this->interpolate_color_(this->led_transitions_[i].start, this->led_transitions_[i].target, p);
    }

    /// The progress of this transition, on a scale of 0 to 1.
    float get_progress_(uint32_t start_time, uint32_t length, uint32_t now) {
        if (now < start_time)
            return 0.0f;
        if (now >= start_time + length)
            return 1.0f;

        return clamp((now - start_time) / float(length), 0.0f, 1.0f);
    }

    Color interpolate_color_(Color start_color, Color target_color, float completion) {
        if (completion <= 0.0)
            return start_color;
        if (completion >= 1.0)
            return target_color;
        float r = esphome::lerp(completion, float(start_color.r), float(target_color.r));
        float g = esphome::lerp(completion, float(start_color.g), float(target_color.g));
        float b = esphome::lerp(completion, float(start_color.b), float(target_color.b));
        return Color(static_cast<uint8_t>(roundf(r)), static_cast<uint8_t>(roundf(g)), static_cast<uint8_t>(roundf(b)));
    }

 protected:
  enum NoteStatus : uint8_t {
      OFF         = 0x00,
      PRESSED     = 0x01,
      SUSTAINED   = 0x02,
  };

  struct ColorTransition
  {
      Color background;
      Color start;
      Color target;
      uint32_t start_time;
      uint32_t length;
  };

 protected:

  std::vector<ColorTransition> led_transitions_;

  NoteStatus note_statuses_[128];
  uint32_t note_on_time_[128];

};

}  // namespace midi_in
}  // namespace esphome