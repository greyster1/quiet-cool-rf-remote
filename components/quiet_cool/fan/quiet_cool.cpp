#include "quiet_cool.h"
#include "esphome/core/log.h"
#include "quietcool.h"

namespace esphome {
    namespace quiet_cool {

        static const char *TAG = "quiet_cool.fan";

        void QuietCoolFan::setup() {
            if (!this->pins_set_) {
                ESP_LOGE(TAG, "QuietCool pins not configured via YAML; radio not initialised");
                return;
            }

            if (this->qc_ == nullptr) {
                // Use standard VSPI pins (CLK18, MISO19, MOSI23) for ESP32 dev boards
                this->qc_.reset(new QuietCool(this->csn_pin_, this->gdo0_pin_, this->gdo2_pin_, 18, 19, 23, remote_id_.data(), center_freq_mhz, deviation_khz, tx_power_dbm));
            }

            if (!this->qc_->begin()) {
                this->mark_failed();
                return;
            }
            ESP_LOGD(TAG, "QuietCool initialized");
        }

        fan::FanTraits QuietCoolFan::get_traits() {
            return fan::FanTraits(false, true, false, 3);
        }

        void QuietCoolFan::control(const fan::FanCall &call) {
            ESP_LOGD(TAG, "Control called: state=%s, speed=%s",
                     call.get_state().has_value() ? (*call.get_state() ? "ON" : "OFF") : "<unchanged>",
                     call.get_speed().has_value() ? std::to_string(*call.get_speed()).c_str() : "<unchanged>");
            if (call.get_state().has_value())
                this->state = *call.get_state();
            if (call.get_speed().has_value())
                this->speed = *call.get_speed();

            this->transmit_state_();
            this->publish_state();
        }

        void QuietCoolFan::transmit_state_() {
            if (!this->qc_) return;
            // Any new command supersedes a running timer
            this->cancel_timeout("duration");
            if (!this->state) {
                // Matches what upstream sent for "off" (0x90), which is the tested case
                this->qc_->send(QUIETCOOL_SPEED_LOW, QUIETCOOL_DURATION_OFF);
                return;
            }
            QuietCoolSpeed qcspd = QUIETCOOL_SPEED_HIGH;
            if (this->speed == 1) qcspd = QUIETCOOL_SPEED_LOW;
            else if (this->speed == 2) qcspd = QUIETCOOL_SPEED_MEDIUM;
            this->qc_->send(qcspd, this->duration_);

            if (this->duration_hours_ > 0) {
                // The fan switches itself off when its timer runs out; mirror that in HA
                uint32_t ms = (uint32_t) this->duration_hours_ * 3600UL * 1000UL;
                this->set_timeout("duration", ms, [this]() {
                    ESP_LOGI(TAG, "%dh timer elapsed; marking fan off", this->duration_hours_);
                    this->state = false;
                    this->publish_state();
                });
            }
        }

        void QuietCoolFan::set_duration_hours(int hours) {
            switch (hours) {
                case 1:  this->duration_ = QUIETCOOL_DURATION_1H;  break;
                case 2:  this->duration_ = QUIETCOOL_DURATION_2H;  break;
                case 4:  this->duration_ = QUIETCOOL_DURATION_4H;  break;
                case 8:  this->duration_ = QUIETCOOL_DURATION_8H;  break;
                case 12: this->duration_ = QUIETCOOL_DURATION_12H; break;
                default: hours = 0; this->duration_ = QUIETCOOL_DURATION_ON; break;
            }
            this->duration_hours_ = hours;
            ESP_LOGI(TAG, "Duration set to %s", hours ? (std::to_string(hours) + "h").c_str() : "on (no timer)");
            if (this->state) this->transmit_state_();
        }

        void QuietCoolFan::set_center_frequency(float mhz) {
            this->center_freq_mhz = mhz;
            if (this->qc_) this->qc_->setFrequency(mhz);
        }

        void QuietCoolFan::resend() {
            ESP_LOGI(TAG, "Resending current state at %.4f MHz", this->center_freq_mhz);
            this->transmit_state_();
        }

        void QuietCoolFan::dump_config() {
            LOG_FAN("", "QuietCool fan", this);
            ESP_LOGCONFIG(TAG, "  Remote ID: %02X %02X %02X %02X %02X %02X %02X",
                          remote_id_[0], remote_id_[1], remote_id_[2], remote_id_[3],
                          remote_id_[4], remote_id_[5], remote_id_[6]);
            ESP_LOGCONFIG(TAG, "  Frequency: %.4f MHz, deviation: %.1f kHz, TX power: %d dBm",
                          center_freq_mhz, deviation_khz, tx_power_dbm);
            ESP_LOGCONFIG(TAG, "  Duration: %d h (0 = no timer)", duration_hours_);
        }
    }  // namespace quiet_cool
}  // namespace esphome
