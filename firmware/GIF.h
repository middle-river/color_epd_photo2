// Library for Decoding GIF Images.
// 2022-04-17  T. Nakagawa

#include <functional>

class GIF {
public:
  static bool read(File *file, int width, int height, std::function<void(uint8_t *, int)> callback) {
    GifHeader gif_header;
    file->read((uint8_t *)&gif_header, 13);
    const int colors = gif_header.field & 0x7;
    if (gif_header.width != width || gif_header.height != height || colors != 2) return false;
    file->seek(3 * 8, SeekCur);  // Palette.
    BlockHeader blk_header;
    file->read((uint8_t *)&blk_header, 10);
    if (blk_header.id != 0x2c || blk_header.width != width || blk_header.height != height || blk_header.field != 0) return false;
    const uint8_t min_code_size = file->read();
    const int clear_code = (1 << min_code_size);
    const int stop_code = (1 << min_code_size) + 1;

    uint8_t *buf = new uint8_t[width / 2];
    uint8_t *tmp = new uint8_t[4096];
    Dictionary *dic = new Dictionary();
    int code_size = min_code_size;
    int prev = -1;
    BitStream bs(file);
    int code;
    int buf_size = 0;
    while ((code = bs.read(code_size + 1)) != stop_code) {
      if (code < 0) break;

      if (code == clear_code) {
        code_size = min_code_size;
        for (dic->size = 0; dic->size < (1 << code_size); dic->size++) {
          dic->data[dic->size] = dic->size;
          dic->next[dic->size] = -1;
        }
        dic->size += 2;  // clear_code and stop_code.
        prev = -1;
        continue;
      }

      if (prev >= 0) {
        int ptr = (code < dic->size) ? code : prev;
        for (; dic->next[ptr] >= 0; ptr = dic->next[ptr]) ;
        dic->data[dic->size] = dic->data[ptr];
        dic->next[dic->size] = prev;
        dic->size++;
        if (dic->size == (1 << (code_size + 1)) && code_size < 11) code_size++;
      }
      prev = code;
      int tmp_size = 0;
      for (; code >= 0; code = dic->next[code]) tmp[tmp_size++] = dic->data[code];
      for (int i = 0; i < tmp_size; i++) {
        if ((buf_size & 0x1) == 0) {
          buf[buf_size++ >> 1] = (tmp[tmp_size - 1 - i] << 4);
        } else {
          buf[buf_size++ >> 1] |= tmp[tmp_size - 1 - i];
        }
        if (buf_size == width) {
          callback(buf, buf_size / 2);
          buf_size = 0;
        }
      }
    }
    delete dic;
    delete[] tmp;
    delete[] buf;
    if (file->read() != 0x00 || file->read() != 0x3b) return false;  // Trailer.
    return true;
  }

private:
  class BitStream {
  public:
    BitStream(File *file) : file_(file), size_(0), len_(0), buf_(0) {
    }

    int read(int bits) {
      while (len_ < bits) {
        if (size_ <= 0) {
          size_ = file_->read();
          if (size_ <= 0) return -1;
        }
        buf_ |= (file_->read() << len_);
        size_--;
        len_ += 8;
      }
      const int data = (buf_ & ((1 << bits) - 1));
      buf_ >>= bits;
      len_ -= bits;
      return data;
    }

  private:
    File *file_;
    int size_;
    int len_;
    uint32_t buf_;
  };

  struct Dictionary {
    int size;
    uint8_t data[4096];
    int16_t next[4096];
  };

  struct GifHeader {
    uint8_t id[3];
    uint8_t version[3];
    uint16_t width;
    uint16_t height;
    uint8_t field;
    uint8_t bg_color;
    uint8_t aspect_ratio;
  } __attribute__((__packed__));

  struct BlockHeader  {
    uint8_t id;
    uint16_t left;
    uint16_t top;
    uint16_t width;
    uint16_t height;
    uint8_t field;
  } __attribute__((__packed__));
};
