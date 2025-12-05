#define CHUNK_SIZE    20
#define METADATA_SIZE 6

typedef struct wsg_data {
    uint64_t time;
    std::array<uint32_t, 3> wsgs;
};

class WSG_MEM {
  public:
    esp_err_t update_meta(uint32_t& page_addr, uint16_t& column_addr);
    esp_err_t reset(uint32_t last_page, uint16_t last_column = 0);

  private:
    uint8_t block_size = (1 << 6);
}