update_meta(1, 0);
uint16_t column_addr = 0;
column_addr &= 0xFFF;
uint32_t page_addr = 0;
std::vector<uint8_t> metadata(METADATA_SIZE);

read_meta(metadata);
interpret_meta_data(metadata, page_addr, column_addr);
if (metadata[0] == 0) {
    ESP_LOGE(TAG, "error! metadata says that page 0 is the starting page :(");
    return;
}
std::srand(1);
wsgs.at(0) = rand();
wsgs.at(1) = rand();
wsgs.at(2) = rand();

for (int i = 0; i < 3; i++) {
    ESP_LOGI(TAG, "WSG Data: %hhu", wsgs[i]);
}
for (int i = 0; i < 16; i++) {
    if ((i % METADATA_UPDATE_INT) == 0) {
        update_meta(page_addr, column_addr);
    }

    if ((column_addr + (CHUNK_SIZE * 8)) <= 2047) {
        cont_write(wsgs, page_addr, column_addr);

    } else {
        page_addr++;
        column_addr = 0;
        cont_write(wsgs, page_addr, column_addr);
    }
    column_addr += (CHUNK_SIZE * 8);
    vTaskDelay(2);
}
read_meta(metadata);
interpret_meta_data(metadata, page_addr, column_addr);

read_all(page_addr, column_addr);