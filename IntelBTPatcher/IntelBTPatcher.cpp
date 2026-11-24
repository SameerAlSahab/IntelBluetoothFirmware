//
//  IntelBTPatcher.cpp
//  IntelBTPatcher
//
//  Created by zxystd <zxystd@foxmail.com> on 2021/2/8.
//

#include <Headers/kern_api.hpp>
#include <Headers/kern_util.hpp>
#include <Headers/plugin_start.hpp>

#include "IntelBTPatcher.hpp"

static CIntelBTPatcher ibtPatcher;
static CIntelBTPatcher *callbackIBTPatcher = nullptr;

static const char *bootargOff[] {
    "-ibtcompatoff"
};

static const char *bootargDebug[] {
    "-ibtcompatdbg"
};

static const char *bootargBeta[] {
    "-ibtcompatbeta"
};

PluginConfiguration ADDPR(config) {
    xStringify(PRODUCT_NAME),
    parseModuleVersion(xStringify(MODULE_VERSION)),
    LiluAPI::AllowNormal | LiluAPI::AllowInstallerRecovery | LiluAPI::AllowSafeMode,
    bootargOff,
    arrsize(bootargOff),
    bootargDebug,
    arrsize(bootargDebug),
    bootargBeta,
    arrsize(bootargBeta),
    KernelVersion::MountainLion,
    KernelVersion::Tahoe,
    []() {
        ibtPatcher.init();
    }
};

static const char *IntelBTPatcher_IOBluetoothFamily[] { "/System/Library/Extensions/IOBluetoothFamily.kext/Contents/MacOS/IOBluetoothFamily" };

static KernelPatcher::KextInfo IntelBTPatcher_IOBluetoothInfo {
    "com.apple.iokit.IOBluetoothFamily",
    IntelBTPatcher_IOBluetoothFamily,
    1,
    {true, true},
    {},
    KernelPatcher::KextInfo::Unloaded
};

static const char *IntelBTPatcher_IOUSBHostFamily[] {
    "/System/Library/Extensions/IOUSBHostFamily.kext/Contents/MacOS/IOUSBHostFamily" };

static KernelPatcher::KextInfo IntelBTPatcher_IOUsbHostInfo {
    "com.apple.iokit.IOUSBHostFamily",
    IntelBTPatcher_IOUSBHostFamily,
    1,
    {true, true},
    {},
    KernelPatcher::KextInfo::Unloaded
};

bool CIntelBTPatcher::_randomAddressInit = false;

bool CIntelBTPatcher::init()
{
    DBGLOG(DRV_NAME, "%s", __PRETTY_FUNCTION__);
    callbackIBTPatcher = this;
    if (getKernelVersion() < KernelVersion::Monterey) {
        lilu.onKextLoadForce(&IntelBTPatcher_IOBluetoothInfo, 1,
        [](void *user, KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
            callbackIBTPatcher->processKext(patcher, index, address, size);
        }, this);
    } else {
        lilu.onKextLoadForce(&IntelBTPatcher_IOUsbHostInfo, 1,
        [](void *user, KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
            callbackIBTPatcher->processKext(patcher, index, address, size);
        }, this);
    }
    return true;
}

void CIntelBTPatcher::free()
{
    DBGLOG(DRV_NAME, "%s", __PRETTY_FUNCTION__);
}

void CIntelBTPatcher::processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size)
{
    DBGLOG(DRV_NAME, "%s", __PRETTY_FUNCTION__);
    if (getKernelVersion() < KernelVersion::Monterey) {
        if (IntelBTPatcher_IOBluetoothInfo.loadIndex == index) {
            DBGLOG(DRV_NAME, "%s", IntelBTPatcher_IOBluetoothInfo.id);
            
            KernelPatcher::RouteRequest findQueueRequestRequest {
                "__ZN25IOBluetoothHostController17FindQueuedRequestEtP22BluetoothDeviceAddresstbPP21IOBluetoothHCIRequest",
                newFindQueueRequest,
                oldFindQueueRequest
            };
            patcher.routeMultiple(index, &findQueueRequestRequest, 1, address, size);
            if (patcher.getError() == KernelPatcher::Error::NoError) {
                DBGLOG(DRV_NAME, "routed %s", findQueueRequestRequest.symbol);
            } else {
                SYSLOG(DRV_NAME, "failed to resolve %s, error = %d", findQueueRequestRequest.symbol, patcher.getError());
                patcher.clearError();
            }
            
        }
    } else {
        if (IntelBTPatcher_IOUsbHostInfo.loadIndex == index) {
            SYSLOG(DRV_NAME, "%s", IntelBTPatcher_IOUsbHostInfo.id);
            
            KernelPatcher::RouteRequest hostDeviceRequest {
            "__ZN15IOUSBHostDevice13deviceRequestEP9IOServiceRN11StandardUSB13DeviceRequestEPvP18IOMemoryDescriptorRjP19IOUSBHostCompletionj",
                newHostDeviceRequest,
                oldHostDeviceRequest
            };
            patcher.routeMultiple(index, &hostDeviceRequest, 1, address, size);
            if (patcher.getError() == KernelPatcher::Error::NoError) {
                SYSLOG(DRV_NAME, "routed %s", hostDeviceRequest.symbol);
            } else {
                SYSLOG(DRV_NAME, "failed to resolve %s, error = %d", hostDeviceRequest.symbol, patcher.getError());
                patcher.clearError();
            }
        }
    }
}

#pragma mark - For Bigsur-, patch unhandled 0x2019 opcode

#define HCI_OP_LE_START_ENCRYPTION 0x2019

IOReturn CIntelBTPatcher::newFindQueueRequest(void *that, unsigned short arg1, void *addr, unsigned short arg2, bool arg3, void **hciRequestPtr)
{
    IOReturn ret = FunctionCast(newFindQueueRequest, callbackIBTPatcher->oldFindQueueRequest)(that, arg1, addr, arg2, arg3, hciRequestPtr);
    if (ret != 0 && arg1 == HCI_OP_LE_START_ENCRYPTION) {
        ret = FunctionCast(newFindQueueRequest, callbackIBTPatcher->oldFindQueueRequest)(that, arg1, addr, 0xffff, arg3, hciRequestPtr);
        DBGLOG(DRV_NAME, "%s ret: %d arg1: 0x%04x arg2: 0x%04x arg3: %d ptr: %p", __FUNCTION__, ret, arg1, arg2, arg3, *hciRequestPtr);
    }
    return ret;
}

#pragma mark - For Monterey+ patch for intercept HCI REQ and RESP

#define VENDOR_INTEL                0x8087
#define MAX_HCI_BUF_LEN             255
#define HCI_OP_RESET                0x0c03
#define HCI_OP_SET_EVENT_MASK       0x0c01
#define HCI_OP_WRITE_LE_HOST_SUPP   0x0c6d
#define HCI_OP_READ_CLOCK           0x1407
#define HCI_OP_LE_SET_EVENT_MASK    0x2001
#define HCI_OP_LE_SET_RANDOM_ADDR   0x2005
#define HCI_OP_LE_SET_SCAN_PARAM    0x200B
#define HCI_OP_LE_SET_SCAN_ENABLE   0x200C

// Valid static random address (top two bits of MSB byte 8 must be 11 for static random address compliance)
static const uint8_t randomAddressHci[9] = {0x05, 0x20, 0x06, 0x94, 0x50, 0x64, 0xD0, 0x78, 0xEB};

IOReturn CIntelBTPatcher::newHostDeviceRequest(void *that, IOService *provider, StandardUSB::DeviceRequest &request, void *data, IOMemoryDescriptor *descriptor, unsigned int &length, IOUSBHostCompletion *completion, unsigned int timeout)
{
    // Fast path & Safety: Verify that 'that' belongs to an Intel Bluetooth USB device.
    // IOUSBHostDevice::deviceRequest is called globally for EVERY USB device on macOS (storage disks, APFS volumes, hubs, etc.).
    // Touching descriptors or modifying parameters of non-Intel devices causes storage corruption and "Volume Hash Mismatch".
    auto device = static_cast<IOUSBHostDevice *>(that);
    if (!device) {
        return FunctionCast(newHostDeviceRequest, callbackIBTPatcher->oldHostDeviceRequest)(that, provider, request, data, descriptor, length, completion, timeout);
    }

    const StandardUSB::DeviceDescriptor *devDesc = device->getDeviceDescriptor();
    if (!devDesc || USBToHost16(devDesc->idVendor) != VENDOR_INTEL) {
        return FunctionCast(newHostDeviceRequest, callbackIBTPatcher->oldHostDeviceRequest)(that, provider, request, data, descriptor, length, completion, timeout);
    }

    uint8_t reqType = (request.bmRequestType & kDeviceRequestTypeMask) >> kDeviceRequestTypePhase;

    // Track USB standard configuration / interface reset events.
    // On wake from sleep (S3, S4, modern standby, S0ix, darkwake), macOS reconfigures the device interfaces.
    if (reqType == kRequestTypeStandard) {
        if (request.bRequest == StandardUSB::kDeviceRequestSetConfiguration ||
            request.bRequest == StandardUSB::kDeviceRequestSetInterface) {
            _randomAddressInit = false;
        }
        return FunctionCast(newHostDeviceRequest, callbackIBTPatcher->oldHostDeviceRequest)(that, provider, request, data, descriptor, length, completion, timeout);
    }

    // HCI commands over USB endpoint 0 are Class requests
    HciCommandHdr *hdr = nullptr;
    uint32_t hdrLen = 0;
    char hciBuf[MAX_HCI_BUF_LEN] = {0};
    bool didPrepare = false;

    if (data == nullptr) {
        if (descriptor != nullptr && descriptor->getLength() > 0) {
            if (getKernelVersion() >= KernelVersion::Sequoia) {
                if (descriptor->prepare(kIODirectionOut) == kIOReturnSuccess) {
                    didPrepare = true;
                }
            }
            descriptor->readBytes(0, hciBuf, min((size_t)descriptor->getLength(), sizeof(hciBuf)));
            hdrLen = (uint32_t)min((size_t)descriptor->getLength(), sizeof(hciBuf));
            if (didPrepare) {
                descriptor->complete(kIODirectionOut);
            }
        }
        hdr = (HciCommandHdr *)hciBuf;
    } else {
        hdr = (HciCommandHdr *)data;
        hdrLen = (request.wLength >= 3) ? (request.wLength - 3) : 0;
    }

    if (hdr && hdrLen >= 2) {
        uint16_t opcode = hdr->opcode;

        // Reset or stack re-init commands indicate device wake or state reset across any sleep states
        if (opcode == HCI_OP_RESET ||
            opcode == HCI_OP_SET_EVENT_MASK ||
            opcode == HCI_OP_WRITE_LE_HOST_SUPP ||
            opcode == HCI_OP_LE_SET_EVENT_MASK) {
            _randomAddressInit = false;
        }

        // Fix AirPods & Bluetooth headphone "no sound" / audio channel drop:
        // When setting up an A2DP stream, macOS sends HCI_Read_Clock (0x1407) with Which_Clock = 0x01 (Piconet Clock).
        // Intel controllers reject piconet clock reading before ACL sync, causing bluetoothd to fail
        // the Fast-Connect Gate and terminate the audio route, resulting in complete silence.
        // Falling back to 0x00 (Local Clock) is natively supported by Intel firmware with status 0 (Success),
        // allowing bluetoothd to proceed and immediately opening the A2DP audio stream.
        if (opcode == HCI_OP_READ_CLOCK) {
            if (data != nullptr && hdr->len >= 3) {
                if (hdr->data[2] == 0x01) {
                    hdr->data[2] = 0x00;
                }
            } else if (descriptor != nullptr && descriptor->getLength() >= 6) {
                uint8_t whichClock = 0;
                descriptor->readBytes(5, &whichClock, 1);
                if (whichClock == 0x01) {
                    uint8_t localClock = 0x00;
                    bool prep = false;
                    if (getKernelVersion() >= KernelVersion::Sequoia) {
                        prep = (descriptor->prepare(kIODirectionOut) == kIOReturnSuccess);
                    }
                    descriptor->writeBytes(5, &localClock, 1);
                    if (prep) {
                        descriptor->complete(kIODirectionOut);
                    }
                }
            }
        }

        // Inject Random Address before LE scan parameters if not initialized or after waking from sleep
        if (opcode == HCI_OP_LE_SET_SCAN_PARAM) {
            if (!_randomAddressInit) {
                StandardUSB::DeviceRequest randReq;
                randReq.bmRequestType = makeDeviceRequestbmRequestType(kRequestDirectionOut, kRequestTypeClass, kRequestRecipientInterface);
                randReq.bRequest = 0xE0;
                randReq.wIndex = 0;
                randReq.wValue = 0;
                randReq.wLength = sizeof(randomAddressHci);

                // Use an independent local length variable so the caller's 'length' reference is NEVER corrupted!
                unsigned int randLength = sizeof(randomAddressHci);
                IOBufferMemoryDescriptor *randDesc = IOBufferMemoryDescriptor::withBytes(randomAddressHci, sizeof(randomAddressHci), kIODirectionOut);
                if (randDesc) {
                    randDesc->prepare(kIODirectionOut);
                    IOReturn ret = FunctionCast(newHostDeviceRequest, callbackIBTPatcher->oldHostDeviceRequest)(that, provider, randReq, nullptr, randDesc, randLength, nullptr, timeout);
                    randDesc->complete(kIODirectionOut);
                    randDesc->release();
                    _randomAddressInit = true;
                    DBGLOG(DRV_NAME, "[PATCH] Sending Random Address HCI ret: %d", ret);
                }
            }
        }
    }

    return FunctionCast(newHostDeviceRequest, callbackIBTPatcher->oldHostDeviceRequest)(that, provider, request, data, descriptor, length, completion, timeout);
}
