#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "ht16k33_display.h"
#include "font.h"

#ifndef USE_ESP8266
#define pgm_read_word(s) (*s)
#endif

namespace esphome
{
  namespace ht16k33_7seg
  {

    static const char *TAG = "ht16k33_7seg";

    // First set bit determines command, bits after that are the data.
    static const uint8_t DISPLAY_COMMAND_SET_DDRAM_ADDR = 0x00;
    static const uint8_t DISPLAY_COMMAND_SYSTEM_SETUP = 0x21;
    static const uint8_t DISPLAY_COMMAND_DISPLAY_OFF = 0x80;
    static const uint8_t DISPLAY_COMMAND_DISPLAY_ON = 0x81;
    static const uint8_t DISPLAY_COMMAND_DIMMING = 0xE0;

    static const bool ACTIVE_COLS[] = {true, true, false, true, true};
    static const int COL_COUNT = sizeof(ACTIVE_COLS);
    static const int CHAR_COL_COUNT = 4;

    void HT16K337SegDisplay::setup()
    {
      for (auto *display : this->displays_)
      {
        display->write_bytes(DISPLAY_COMMAND_SYSTEM_SETUP, nullptr, 0);
        display->write_bytes(DISPLAY_COMMAND_DISPLAY_ON, nullptr, 0);
      }
      this->set_brightness(1);
    }

    void HT16K337SegDisplay::loop()
    {
      unsigned long now = millis();
      int characterCount = this->displays_.size() * CHAR_COL_COUNT;
      int bufferLength = this->buffer_.size();
      if (!this->scroll_ || (bufferLength <= characterCount))
        return;
      if ((this->offset_ == 0) && (now - this->last_scroll_ < this->scroll_delay_))
        return;
      if ((!this->continuous_ && (this->offset_ + characterCount >= bufferLength)) ||
          (this->continuous_ && (this->offset_ > bufferLength - 1)))
      {
        if (this->continuous_ || (now - this->last_scroll_ >= this->scroll_dwell_))
        {
          this->offset_ = 0;
          this->last_scroll_ = now;
          this->display_();
        }
      }
      else if (now - this->last_scroll_ >= this->scroll_speed_)
      {
        this->offset_++;
        this->last_scroll_ = now;
        this->display_();
      }
    }

    float HT16K337SegDisplay::get_setup_priority() const { return setup_priority::PROCESSOR; }

    void HT16K337SegDisplay::display_()
    {
      int dataCount = this->displays_.size() * COL_COUNT;
      int characterCount = this->displays_.size() * CHAR_COL_COUNT;
      int bufferLength = this->buffer_.size();
      uint16_t data[dataCount];
      memset(data, 0, dataCount);
      int pos = this->offset_;
      for (int i = 0; i < dataCount; i++)
      {
        if (!ACTIVE_COLS[i])
        {
          data[i] = 0;
          continue;
        }
        if (pos >= bufferLength)
        {
          if (!this->continuous_)
            data[i] = 0;
          continue;
          pos %= bufferLength;
        }
        data[i] = this->buffer_[pos++];
      }
      pos = 0;
      for (auto *display : this->displays_)
      {
        display->write_bytes_16(DISPLAY_COMMAND_SET_DDRAM_ADDR, data + pos, COL_COUNT);
        pos += COL_COUNT;
      }
    }

    void HT16K337SegDisplay::update()
    {
      int prev_bufferLength = this->buffer_.size();
      this->buffer_.clear();
      this->call_writer();
      int characterCount = this->displays_.size() * CHAR_COL_COUNT;
      int bufferLength = this->buffer_.size();
      if ((this->scroll_ && (prev_bufferLength != bufferLength) && !this->continuous_) || (bufferLength <= characterCount))
      {
        this->last_scroll_ = millis();
        this->offset_ = 0;
      }
      this->display_();
    }

    void HT16K337SegDisplay::set_brightness(float level)
    {
      int val = (int)round(level * 16);
      if (val < 0)
        val = 0;
      if (val > 16)
        val = 16;
      this->brightness_ = val;
      for (auto *display : this->displays_)
      {
        if (val == 0)
        {
          display->write_bytes(DISPLAY_COMMAND_DISPLAY_OFF, nullptr, 0);
        }
        else
        {
          display->write_bytes(DISPLAY_COMMAND_DIMMING + (val - 1), nullptr, 0);
          display->write_bytes(DISPLAY_COMMAND_DISPLAY_ON, nullptr, 0);
        }
      }
    }

    float HT16K337SegDisplay::get_brightness()
    {
      return this->brightness_ / 16.0;
    }

    void HT16K337SegDisplay::print(const char *str)
    {
      uint16_t fontc = 0;
      while (*str != '\0')
      {
        uint16_t c = *reinterpret_cast<const uint8_t *>(str++);
        if (c > 127)
          fontc = 0;
        else
          fontc = (pgm_read_word(&alphafonttable[c]) << 8);
        c = *reinterpret_cast<const uint8_t *>(str);
        if (c == '.')
        {
          fontc |= (1 << 15);
          str++;
        }
        this->buffer_.push_back(fontc);
      }
    }

    void HT16K337SegDisplay::print(const std::string &str) { this->print(str.c_str()); }

    void HT16K337SegDisplay::printf(const char *format, ...)
    {
      va_list arg;
      va_start(arg, format);
      char buffer[64];
      int ret = vsnprintf(buffer, sizeof(buffer), format, arg);
      va_end(arg);
      if (ret > 0)
        this->print(buffer);
    }

#ifdef USE_TIME
    void HT16K337SegDisplay::strftime(const char *format, ESPTime time)
    {
      char buffer[64];
      size_t ret = time.strftime(buffer, sizeof(buffer), format);
      if (ret > 0)
        this->print(buffer);
    }
#endif

  } // namespace ht16k33_7seg
} // namespace esphome
