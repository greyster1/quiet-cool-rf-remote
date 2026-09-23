#pragma once

#include "esphome/components/fan/fan.h"
#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"
#include "quietcool.h"
#include <array>
#include <memory>
#include <vector>

namespace esphome {
    namespace quiet_cool {

        class QuietCoolFan :
	    public Component,
	    public fan::Fan,
	    public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_1KHZ>
	{
        public:

            void dump_config() override;
            fan::FanTraits get_traits() override;
            void setup() override;  // initialise radio
            float get_setup_priority() const override { return setup_priority::BUS; }
            void set_pins(uint8_t csn, uint8_t gdo0, uint8_t gdo2) {
                this->csn_pin_ = csn;
                this->gdo0_pin_ = gdo0;
                this->gdo2_pin_ = gdo2;
                this->pins_set_ = true;
            }
	    void set_frequencies(float center_freq_mhz, float deviation_khz) {
		this->center_freq_mhz = center_freq_mhz;
		this->deviation_khz = deviation_khz;
	    }
	    void set_tx_power(int tx_power_dbm) { this->tx_power_dbm = tx_power_dbm; }
            void set_remote_id(const std::vector<uint8_t> &remote_id) {
                for (size_t i = 0; i < 7 && i < remote_id.size(); ++i) remote_id_[i] = remote_id[i];
            }

            // Runtime tuning, e.g. from a template number's set_action
            void set_center_frequency(float mhz);
            float get_center_frequency() const { return this->center_freq_mhz; }
            // Re-transmit the command for the current state (handy while sweeping frequency)
            void resend();
            // Remote's timer setting: 0 = on (no timer), or 1, 2, 4, 8, 12 hours.
            // Sent with every "on" command; re-sent immediately if the fan is running.
            void set_duration_hours(int hours);

        protected:
            void control(const fan::FanCall &call) override;
            void transmit_state_();
        private:
            std::unique_ptr<QuietCool> qc_;

            uint8_t csn_pin_{};
            uint8_t gdo0_pin_{};
            uint8_t gdo2_pin_{};
	    float center_freq_mhz{433.897};
	    float deviation_khz{10};
	    int tx_power_dbm{10};
	    QuietCoolDuration duration_{QUIETCOOL_DURATION_ON};
	    int duration_hours_{0};
            bool pins_set_{false};
            std::array<uint8_t, 7> remote_id_{{0x2D, 0xD4, 0x06, 0xCB, 0x00, 0xF7, 0xF2}};
        };

    }  // namespace quiet_cool
}  // namespace esphome
