#!/bin/bash

python firmware/nanopb/generator/nanopb_generator.py proto/wsg.proto
echo 'Generated proto files'

mv proto/*.c firmware/esp_idf/components/nanopb/src

mv proto/*.h firmware/esp_idf/components/nanopb/include 