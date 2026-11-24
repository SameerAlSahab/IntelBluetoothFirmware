//
//  BtIntelFirmware.cpp
//  IntelBluetoothFirmware
//
//  Created by qcwap on 2021/6/16.
//  Copyright © 2021 zxystd. All rights reserved.
//

#include "Log.h"
#include "BtIntel.h"
#include "FwData.h"

OSData *BtIntel::
firmwareConvertion(OSData *originalFirmware)
{
    unsigned int origLen = originalFirmware->getLength();
    unsigned int numBytes = origLen * 4;
    if (numBytes < 1024 * 1024)
        numBytes = 1024 * 1024;
    unsigned int actualBytes = numBytes;
    OSData *fwData = NULL;
    unsigned char* _fwBytes = (unsigned char *)IOMalloc(numBytes);
    if (!_fwBytes)
        return NULL;
    if (!uncompressFirmware(_fwBytes, &actualBytes, (unsigned char *)originalFirmware->getBytesNoCopy(), origLen)) {
        IOFree(_fwBytes, numBytes);
        return NULL;
    }
    fwData = OSData::withBytes(_fwBytes, actualBytes);
    IOFree(_fwBytes, numBytes);
    return fwData;
}

OSData *BtIntel::
requestFirmwareData(const char *fwName, bool noWarn)
{
    OSData * _fwData = getFWDescByName(fwName);
    if (!_fwData) {
        if (!noWarn)
            XYLog("Firmware: %s Not found!\n", fwName);
        return NULL;
    }
    XYLog("Found device firmware %s \n", fwName);
    OSData *fwData = firmwareConvertion(_fwData);
    OSSafeReleaseNULL(_fwData);
    if (fwData == NULL) {
        XYLog("Firmware %s uncompress fail!\n", fwName);
        return NULL;
    }
    return fwData;
}
