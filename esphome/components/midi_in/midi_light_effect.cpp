#include "midi_light_effect.h"
#include "esphome/core/log.h"

namespace esphome {
namespace midi_in {

static const char *const TAG = "midi_light_effect";

static const uint32_t ADALIGHT_ACK_INTERVAL = 1000;
static const uint32_t ADALIGHT_RECEIVE_TIMEOUT = 1000;

MidiLightEffect::MidiLightEffect(midi_in::MidiInComponent *midi, const std::string &name) : AddressableLightEffect(name) {
    midi_ = midi;
}

void MidiLightEffect::start() {
  AddressableLightEffect::start();

  led_transitions_.clear();
  led_transitions_.reserve(this->get_addressable_()->size());
  led_transitions_.resize(this->get_addressable_()->size());

  // this->set_background_rainbow_(this->get_addressable_(), Color::WHITE, 1000);
}

void MidiLightEffect::stop() {
  // we don't need transitions array when the effect is not running
  led_transitions_.resize(0);

  AddressableLightEffect::stop();
}

void MidiLightEffect::apply(light::AddressableLight &it, const Color &current_color) {
  const uint32_t now = millis();

  light::AddressableLight *fg_light{nullptr};
  if (this->foreground_light_ != nullptr) {
    fg_light = static_cast<light::AddressableLight *>(this->foreground_light_->get_output());
  }

  light::AddressableLight *bg_light{nullptr};
  if (this->background_light_ != nullptr) {
    bg_light = static_cast<light::AddressableLight *>(this->background_light_->get_output());
  }

  for (int i = it.size() - 1; i >= 0; i--)
  {
      uint8_t note_index = this->start_note_ + i;
      if (this->midi_->note_velocity(note_index) > 0)
      {
          //ESP_LOGD(TAG, "%i: note is ON: %#02x. status: %i", i, this->midi_->note_velocity(note_index), this->note_statuses_[note_index]);
          // note is on

          Color target_color;
          if (fg_light != nullptr) {
            this->led_transitions_[i].target = fg_light->get(i).get();
          }

          if (this->note_statuses_[note_index] != NoteStatus::PRESSED)
          {
              // turn on light
              uint8_t scaled_velocity = 128 + this->midi_->note_velocity(note_index);
              if (this->midi_->control_value(midi::MidiControlChangeNumber::SoftPedal) > 0)
              {
                  // soft pedal
                  scaled_velocity = scaled_velocity / 2;
              }
              this->note_statuses_[note_index] = NoteStatus::PRESSED;
              this->note_on_time_[note_index] = now;

              if (fg_light == nullptr) {
                this->led_transitions_[i].target = Color::WHITE;
              }
              Color current_color = this->get_interpolated_color_(i, now);
              this->led_transitions_[i].start = current_color;
              this->led_transitions_[i].start_time = now;
              this->led_transitions_[i].length = this->note_on_fade_;
              //ESP_LOGD(TAG, "%i: begin ON transition: %#08x - %#08x. length: %i", i, this->start_led_colors_[i], this->target_led_colors_[i], this->transition_length_[i]);
          }
      }
      else
      {
          //ESP_LOGD(TAG, "%i: note OFF. status: %i", i, this->note_statuses_[note_index]);

          // note released

          if (this->note_statuses_[note_index] == NoteStatus::PRESSED && this->midi_->control_value(midi::MidiControlChangeNumber::Sustain) > 0)
          {
            this->note_statuses_[note_index] = NoteStatus::SUSTAINED;
            if (fg_light != nullptr) {
              this->led_transitions_[i].target = fg_light->get(i).get();
            }
          }
          else if (this->note_statuses_[note_index] != NoteStatus::OFF &&  this->midi_->control_value(midi::MidiControlChangeNumber::Sustain) == 0)
          {
              this->note_statuses_[note_index] = NoteStatus::OFF;

              // not sustained. release 
              Color current_color = this->get_interpolated_color_(i, now);
              uint32_t note_length = (now - this->led_transitions_[i].start_time);

              this->led_transitions_[i].start = current_color;
              if (bg_light != nullptr) {
                this->led_transitions_[i].target = bg_light->get(i).get();
              } else {
                this->led_transitions_[i].target = this->led_transitions_[i].background;
              }

              this->led_transitions_[i].start_time = now;
              this->led_transitions_[i].length = std::min(this->note_off_fade_, note_length);

              //ESP_LOGD(TAG, "%i: begin OFF transition: %#08x - %#08x. length: %i", i, this->start_led_colors_[i], this->target_led_colors_[i], this->transition_length_[i]);
          } else if (this->note_statuses_[note_index] == NoteStatus::OFF) {
            if (bg_light != nullptr) {
              this->led_transitions_[i].target = bg_light->get(i).get();
            }
          }
      }

      it[i] = this->get_interpolated_color_(i, now);

      //ESP_LOGD(TAG, "%i: progress: %.2f (%i-%i/%i), color: %#08x", i, p, now, this->transition_start_time_[i], this->transition_length_[i], interpolated_color);
  }

  it.schedule_show();
}

}  // namespace midi_in
}  // namespace esphome