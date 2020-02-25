/***************************************************
This is a library for the Adafruit Capacitive Touch Screens

----> http://www.adafruit.com/products/1947

Check out the links above for our tutorials and wiring diagrams
This chipset uses I2C to communicate

Adafruit invests time and resources providing this open source code,
please support Adafruit and open-source hardware by purchasing
products from Adafruit!

Written by Limor Fried/Ladyada for Adafruit Industries.
MIT license, all text above must be included in any redistribution
****************************************************/


#include <Adafruit_FT6206.h>

#if defined(__SAM3X8E__)
#define Wire Wire1
#endif

void delay(uint32_t);

/**************************************************************************/
/*!
@brief  Instantiates a new FT6206 class
*/
/**************************************************************************/
// I2C, no address adjustments or pins
Adafruit_FT6206::Adafruit_FT6206()
: touches{0},
  touchX{0,0},
  touchY{0,0},
  touchID{0,0},
  hI2C{0}
{
}


/**************************************************************************/
/*!
@brief  Setups the HW
*/
/**************************************************************************/
boolean Adafruit_FT6206::begin(uint8_t threshhold) {

    // change threshhold to be higher/lower
    writeRegister8(FT6206_REG_THRESHHOLD, threshhold);

    if ((readRegister8(FT6206_REG_VENDID) != 17) || (readRegister8(FT6206_REG_CHIPID) != 6))
        return false;

    return true;
}

// DONT DO THIS - REALLY - IT DOESNT WORK
void Adafruit_FT6206::autoCalibrate(void) {
    writeRegister8(FT6206_REG_MODE, FT6206_REG_FACTORYMODE);
    delay(100);

    writeRegister8(FT6206_REG_CALIBRATE, 4);
    delay(300);
    for (uint8_t i = 0; i < 100; i++) {
        uint8_t temp;
        temp = readRegister8(FT6206_REG_MODE);

        //return to normal mode, calibration finish
        if (0x0 == ((temp & 0x70) >> 4))
            break;
    }
    delay(200);

    delay(300);
    writeRegister8(FT6206_REG_MODE, FT6206_REG_FACTORYMODE);
    delay(100);
    writeRegister8(FT6206_REG_CALIBRATE, 5);
    delay(300);
    writeRegister8(FT6206_REG_MODE, FT6206_REG_WORKMODE);
    delay(300);
}


boolean Adafruit_FT6206::touched(void) {

    uint8_t n = readRegister8(FT6206_REG_NUMTOUCHES);
    if ((n == 1) || (n == 2)) return true;
    return false;
}

/*****************************/

void Adafruit_FT6206::readData(uint16_t *x, uint16_t *y) {
    uint8_t address(FT6206_ADDR<<1);
    INT32U writeBufSize{1};
    INT32U readBufSize{16};
    uint8_t writeBuf[2] = {address, 0};
    uint8_t readBuf[16] = {address};

    Write(hI2C, writeBuf, &writeBufSize);
    Read(hI2C, readBuf, &readBufSize);

    uint8_t* i2cdat{readBuf};

    touches = i2cdat[0x02];

    if (touches > 2) {
        touches = 0;
        *x = *y = 0;
    }
    if (touches == 0) {
        *x = *y = 0;
        return;
    }


    for (uint8_t i=0; i<2; i++) {
        touchX[i] = i2cdat[0x03 + i*6] & 0x0F;
        touchX[i] <<= 8;
        touchX[i] |= i2cdat[0x04 + i*6];
        touchY[i] = i2cdat[0x05 + i*6] & 0x0F;
        touchY[i] <<= 8;
        touchY[i] |= i2cdat[0x06 + i*6];
        touchID[i] = i2cdat[0x05 + i*6] >> 4;
    }

    *x = touchX[0]; *y = touchY[0];
}

TS_Point Adafruit_FT6206::getPoint(void) {
    uint16_t x, y;
    readData(&x, &y);
    return TS_Point(x, y, 1);
}


uint8_t Adafruit_FT6206::readRegister8(uint8_t reg) {
    uint8_t address(FT6206_ADDR<<1);
    INT32U writeBufSize{1};
    INT32U readBufSize{1};
    uint8_t writeBuf[2] = {address, reg};
    uint8_t readBuf[1] = {address};

    // use i2c
    Write(hI2C, writeBuf, &writeBufSize);
    Read(hI2C, readBuf, &readBufSize);

    return readBuf[0];
}

void Adafruit_FT6206::writeRegister8(uint8_t reg, uint8_t val) {
    uint8_t address{FT6206_ADDR<<1};
    INT32U writeBufSize{2};
    uint8_t writeBuf[3] = {address, reg, val};

    // use i2c
    Write(hI2C, writeBuf, &writeBufSize);
}

void Adafruit_FT6206::setPjdfHandle(HANDLE hI2C) {
    this->hI2C = hI2C;
}

/****************/

TS_Point::TS_Point(void) {
    x = y = 0;
}

TS_Point::TS_Point(int16_t x0, int16_t y0, int16_t z0) {
    x = x0;
    y = y0;
    z = z0;
}

bool TS_Point::operator==(TS_Point p1) {
    return  ((p1.x == x) && (p1.y == y) && (p1.z == z));
}

bool TS_Point::operator!=(TS_Point p1) {
    return  ((p1.x != x) || (p1.y != y) || (p1.z != z));
}
