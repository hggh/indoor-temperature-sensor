# indoor-atmel-328-bme280
low power indoor bme280 sensor node


  * 328P on 1Mhz, no BOD
  * RFM69
  * BME280
  * CR2032 (later a solar panel with a supercap)
  

## compile
  - git clone git@github.com:hggh/indoor-atmel-328-bme280.git
  - cp src/config.h.example src/config.h
  - platformio run --target program  


## PCB front/back

![PCB](https://raw.githubusercontent.com/hggh/indoor-atmel-328-bme280/master/pics/pcb.jpg)
