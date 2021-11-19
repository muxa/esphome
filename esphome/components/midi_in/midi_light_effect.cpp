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

  this->set_background_rainbow_(this->get_addressable_(), Color::WHITE, 1000);
}

void MidiLightEffect::stop() {
  // we don't need transitions array when the effect is not running
  led_transitions_.resize(0);

  AddressableLightEffect::stop();
}

void MidiLightEffect::apply(light::AddressableLight &it, const Color &current_color) {
  const uint32_t now = millis();

  for (int i = it.size() - 1; i >= 0; i--)
  {
      uint8_t note_index = this->start_note_ + i;
      if (this->midi_->note_velocities[note_index] > 0)
      {
          //ESP_LOGD(TAG, "%i: note is ON: %#02x. status: %i", i, this->midi_->note_velocities[note_index], this->note_statuses_[note_index]);

          // note is on
          if (this->note_statuses_[note_index] != NoteStatus::PRESSED)
          {
              // turn on light
              uint8_t scaled_velocity = 128 + this->midi_->note_velocities[note_index];
              if (this->midi_->soft_pedal > 0)
              {
                  // soft pedal
                  scaled_velocity = scaled_velocity / 2;
              }
              this->note_statuses_[note_index] = NoteStatus::PRESSED;
              this->note_on_time_[note_index] = now;

              Color current_color = this->get_interpolated_color_(i, now);
              this->set_led_transition_(i, current_color, Color::WHITE, this->note_on_fade_, now);
              // (Color::random_color() * scaled_velocity).raw_32;

              //ESP_LOGD(TAG, "%i: begin ON transition: %#08x - %#08x. length: %i", i, this->start_led_colors_[i], this->target_led_colors_[i], this->transition_length_[i]);
          }
      }
      else
      {
          //ESP_LOGD(TAG, "%i: note OFF. status: %i", i, this->note_statuses_[note_index]);

          // note released
          if (this->midi_->sustain_pedal > 0)
          {
              this->note_statuses_[note_index] = NoteStatus::SUSTAINED;
          }
          else if (this->note_statuses_[note_index] != NoteStatus::OFF)
          {
              this->note_statuses_[note_index] = NoteStatus::OFF;

              // not sustained. release

              Color current_color = this->get_interpolated_color_(i, now);
              uint32_t note_length = (now - this->led_transitions_[i].start_time);

              this->set_led_transition_(i, 
                      current_color, 
                      this->led_transitions_[i].background,
                      std::min(this->note_off_fade_, note_length), 
                      now);

              //ESP_LOGD(TAG, "%i: begin OFF transition: %#08x - %#08x. length: %i", i, this->start_led_colors_[i], this->target_led_colors_[i], this->transition_length_[i]);
          }
      }

      it[i] = this->get_interpolated_color_(i, now);

      //ESP_LOGD(TAG, "%i: progress: %.2f (%i-%i/%i), color: %#08x", i, p, now, this->transition_start_time_[i], this->transition_length_[i], interpolated_color);
  }

  it.schedule_show();
}

}  // namespace midi_in
}  // namespace esphome