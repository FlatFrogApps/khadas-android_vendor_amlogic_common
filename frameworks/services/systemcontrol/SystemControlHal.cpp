/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *  @author   Tellen Yu
 *  @version  1.0
 *  @date     2017/8/30
 *  @par function description:
 *  - 1 system control hwbinder service
 */

#define LOG_TAG "SystemControl"

#include <log/log.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <inttypes.h>
#include <string>
#include <vector>

#include <utils/KeyedVector.h>
#include <utils/String8.h>

#include "SystemControlHal.h"

namespace vendor {
namespace amlogic {
namespace hardware {
namespace systemcontrol {
namespace V1_1 {
namespace implementation {

#define ENABLE_LOG_PRINT 0

SystemControlHal::SystemControlHal(SystemControlService * control)
    : mSysControl(control),
    mDeathRecipient(new DeathRecipient(this)) {

    control->setListener(this);
    control->setPqListener(this);
}

SystemControlHal::~SystemControlHal() {
}

/*
event:
EVENT_OUTPUT_MODE_CHANGE            = 0,
EVENT_DIGITAL_MODE_CHANGE           = 1,
EVENT_HDMI_PLUG_OUT                 = 2,
EVENT_HDMI_PLUG_IN                  = 3,
EVENT_HDMI_AUDIO_OUT                = 4,
EVENT_HDMI_AUDIO_IN                 = 5,
*/
void SystemControlHal::onEvent(int event) {
    int clientSize = mClients.size();
    AutoMutex _l( mLock );

    SYS_LOGI("onEvent event:%d, client size:%d", event, clientSize);

    for (auto it = mClients.begin(); it != mClients.end();) {
        if (it->second == nullptr) {
            it = mClients.erase(it);
            continue;
        }
        auto ret = (it->second)->notifyCallback(event);
        if (!ret.isOk() && ret.isDeadObject()) {
            SYS_LOGE("%s event:%d notifyCallback fail\n", __FUNCTION__, event);
            it = mClients.erase(it);
        } else {
            ++it;
        }
    }
}

void SystemControlHal::onFBCUpgradeEvent(int32_t state, int32_t param) {
    int clientSize = mClients.size();
    AutoMutex _l( mLock );

    if (ENABLE_LOG_PRINT)
        ALOGI("onFBCUpgradeEvent: state:%d, param:%d, client size:%d", state, param, clientSize);

    for (int i = 0; i < clientSize; i++) {
        if (mClients[i] != nullptr) {
            if (ENABLE_LOG_PRINT)
                ALOGI("%s, client cookie:%d notifyCallback", __FUNCTION__, i);
            mClients[i]->notifyFBCUpgradeCallback(state, param);
        }
    }
}

void SystemControlHal::onSetDisplayMode(int mode) {
    AutoMutex _l(mLock);
    if (ENABLE_LOG_PRINT) ALOGI("onSetDisplaymode mode:%d", mode);
    for (auto it = mClients.begin(); it != mClients.end();) {
        if (it->second == nullptr) {
            it = mClients.erase(it);
            continue;
        }
        auto ret = (it->second)->notifySetDisplayModeCallback(mode);
        if (!ret.isOk() && ret.isDeadObject()) {
            it = mClients.erase(it);
        } else {
            ++it;
        }
    }
}

Return<void> SystemControlHal::readAiPqTable(readAiPqTable_cb _hidl_cb) {
    AutoMutex _l(mLock);
    std::string aiPqTable;
    mSysControl->readAiPqTable(&aiPqTable);
    _hidl_cb(Result::OK, aiPqTable);
    return Void();
}

void SystemControlHal::onHdrInfoChange(int newHdrInfo) {
    AutoMutex _l(mLock);
    //ALOGI("%s: newHdrInfo is %d", __FUNCTION__ , newHdrInfo);
    for (auto it = mClients.begin(); it != mClients.end();) {
        if (it->second == nullptr) {
            it = mClients.erase(it);
            continue;
        }
        auto ret = (it->second)->notifyHdrInfoChangedCallback(newHdrInfo);
        if (!ret.isOk() && ret.isDeadObject()) {
            it = mClients.erase(it);
        } else {
            ++it;
        }
    }
}

void SystemControlHal::onAudioEvent(int32_t param1, int32_t param2, int32_t param3, int32_t param4) {
    AutoMutex _l(mLock);
    if (ENABLE_LOG_PRINT) ALOGI("onAudioEvent param1:%d. param2:%d, param3:%d, param4:%d", param1, param2, param3, param4);
    for (auto it = mClients.begin(); it != mClients.end();) {
        if (it->second == nullptr) {
            it = mClients.erase(it);
            continue;
        }
        auto ret = (it->second)->notifyAudioCallback(param1, param2, param3, param4);
        if (!ret.isOk() && ret.isDeadObject()) {
            it = mClients.erase(it);
        } else {
            ++it;
        }
    }
}

Return<void> SystemControlHal::getSupportDispModeList(getSupportDispModeList_cb _hidl_cb) {
    std::vector<std::string> supportModes;
    mSysControl->getSupportDispModeList(&supportModes);

    hidl_vec<hidl_string> hidlList;
    hidlList.resize(supportModes.size());
    for (size_t i = 0; i < supportModes.size(); ++i) {
        hidlList[i] = supportModes[i];

        if (ENABLE_LOG_PRINT)
            ALOGI("getSupportDispModeList index:%ld mode :%s", (unsigned long)i, supportModes[i].c_str());
    }

    _hidl_cb(Result::OK, hidlList);
    return Void();
}

Return<void> SystemControlHal::getActiveDispMode(getActiveDispMode_cb _hidl_cb) {
    std::string mode;
    mSysControl->getActiveDispMode(&mode);

    if (ENABLE_LOG_PRINT)
        ALOGI("getActiveDispMode mode :%s", mode.c_str());
    _hidl_cb(Result::OK, mode);
    return Void();
}

Return<Result> SystemControlHal::setActiveDispMode(const hidl_string &activeDispMode) {
    std::string mode = activeDispMode;

    if (ENABLE_LOG_PRINT)
        ALOGI("setActiveDispMode mode :%s", mode.c_str());
    mSysControl->setActiveDispMode(mode);
    return Result::OK;
}

Return<Result> SystemControlHal::isHDCPTxAuthSuccess() {
    int status;
    mSysControl->isHDCPTxAuthSuccess(status);

    if (ENABLE_LOG_PRINT)
        ALOGI("isHDCPTxAuthSuccess status :%d", status);
    return (status==1)?Result::OK:Result::FAIL;
}

Return<void> SystemControlHal::getProperty(const hidl_string &key, getProperty_cb _hidl_cb) {
    std::string value;
    mSysControl->getProperty(key, &value);

    if (ENABLE_LOG_PRINT)
        ALOGI("getProperty key :%s, value:%s", key.c_str(), value.c_str());
    _hidl_cb(Result::OK, value);
    return Void();
}

Return<void> SystemControlHal::getPropertyString(const hidl_string &key, const hidl_string &def, getPropertyString_cb _hidl_cb) {
    std::string value;
    mSysControl->getPropertyString(key, &value, def);

    if (ENABLE_LOG_PRINT)
        ALOGI("getPropertyString key :%s, value:%s", key.c_str(), value.c_str());
    _hidl_cb(Result::OK, value);
    return Void();
}

Return<void> SystemControlHal::getPropertyInt(const hidl_string &key, int32_t def, getPropertyInt_cb _hidl_cb) {
    int32_t value = mSysControl->getPropertyInt(key, def);

    if (ENABLE_LOG_PRINT)
        ALOGI("getPropertyInt key :%s, value:%d", key.c_str(), value);
    _hidl_cb(Result::OK, value);
    return Void();
}

Return<void> SystemControlHal::getPropertyLong(const hidl_string &key, int64_t def, getPropertyLong_cb _hidl_cb) {
    int64_t value = mSysControl->getPropertyLong(key, def);

    if (ENABLE_LOG_PRINT) {
        ALOGI("getPropertyLong key :%s, value:%lld", key.c_str(), (long long)value);
    }
    _hidl_cb(Result::OK, value);
    return Void();
}

Return<void> SystemControlHal::getPropertyBoolean(const hidl_string &key, bool def, getPropertyBoolean_cb _hidl_cb) {
    bool value = mSysControl->getPropertyBoolean(key, def);

    if (ENABLE_LOG_PRINT)
        ALOGI("getPropertyBoolean key :%s, value:%d", key.c_str(), value);
    _hidl_cb(Result::OK, value);
    return Void();
}

Return<Result> SystemControlHal::setProperty(const hidl_string &key, const hidl_string &value) {
    mSysControl->setProperty(key, value);

    if (ENABLE_LOG_PRINT)
        ALOGI("setProperty key :%s, value:%s", key.c_str(), value.c_str());
    return Result::OK;
}

Return<void> SystemControlHal::readSysfs(const hidl_string &path, readSysfs_cb _hidl_cb) {
    std::string value;
    mSysControl->readSysfs(path, value);

    if (ENABLE_LOG_PRINT)
        ALOGI("readSysfs path :%s, value:%s", path.c_str(), value.c_str());
    _hidl_cb(Result::OK, value);
    return Void();
}
Return<void> SystemControlHal::readSysfsOri(const hidl_string &path, readSysfs_cb _hidl_cb) {
    std::string value;
    mSysControl->readSysfsOri(path, value);

    if (ENABLE_LOG_PRINT)
        ALOGI("readSysfsiOri path :%s, value:%s", path.c_str(), value.c_str());
    _hidl_cb(Result::OK, value);
    return Void();
}

Return<Result> SystemControlHal::writeSysfs(const hidl_string &path, const hidl_string &value) {
    if (ENABLE_LOG_PRINT)
        ALOGI("writeSysfs path :%s, value:%s", path.c_str(), value.c_str());
    return mSysControl->writeSysfs(path, value)?Result::OK:Result::FAIL;
}

Return<Result> SystemControlHal::writeSysfsBin(const hidl_string &path, const hidl_array<int32_t, 4096>& key, int32_t size) {
    if (ENABLE_LOG_PRINT)
        ALOGI("writeSysfs bin");
    char *value = (char *)malloc(size);
    memset(value, 0, size);
    int i;
    bool result = false;

    for (i = 0; i < size; ++i) {
        value[i] = key[i];
    }

    result = mSysControl->writeSysfs(path, value, size);
    free(value);
    if(result)
        return Result::OK;
    else
        return Result::FAIL;
}

Return<Result> SystemControlHal::memcContrl(bool on) {
    if (mSysControl->memcContrl(on)) {
        return Result::OK;
    }
    return Result::FAIL;
}

Return<Result> SystemControlHal::scCpyFile(const hidl_string& src,const hidl_string& dest, bool usethread) {
    if (mSysControl->scCpyFile(src, dest, usethread)) {
        return Result::OK;
    }
    return Result::FAIL;
}

Return<Result> SystemControlHal::writeHdcpRXImg(const hidl_string &path) {
    if (ENABLE_LOG_PRINT) ALOGI("writeHdcpRXImg path:%s", path.c_str());
    return mSysControl->writeHdcpRXImg(path)?Result::OK:Result::FAIL;
}

Return<Result> SystemControlHal::updataLogoBmp(const hidl_string &path) {
    if (ENABLE_LOG_PRINT) ALOGI("updataLogoBmp path:%s", path.c_str());
    return mSysControl->updataLogoBmp(path)?Result::OK:Result::FAIL;
}

//Provision key start
Return<Result> SystemControlHal::writeUnifyKey(const hidl_string &path, const hidl_string &value) {
    if (ENABLE_LOG_PRINT) ALOGI("writeUnifyKey path :%s, value:%s", path.c_str(), value.c_str());
    return mSysControl->writeUnifyKey(path, value)?Result::OK:Result::FAIL;
}

Return<Result> SystemControlHal::writePlayreadyKey(const hidl_array<int32_t, 4096>& value, int32_t size) {
    if (ENABLE_LOG_PRINT) ALOGI("writePlayreadyKey");
    char *result = (char *)malloc(size);
    memset(result, 0, size);
    int i;
    bool ret = false;
    for (i = 0; i < size; ++i) {
        result[i] = value[i];
    }
    ret = mSysControl->writePlayreadyKey(result, size);
    free(result);
    if(ret)
        return Result::OK;
    else
        return Result::FAIL;
}

Return<Result> SystemControlHal::writeNetflixKey(const hidl_array<int32_t, 4096>& value, int32_t size) {
    if (ENABLE_LOG_PRINT) ALOGI("writeNetflixKey");
    char *result = (char *)malloc(size);
    memset(result, 0, size);
    int i;
    bool ret = false;
    for (i = 0; i < size; ++i) {
        result[i] = value[i];
    }
    ret = mSysControl->writeNetflixKey(result, size);
    free(result);
    if(ret)
        return Result::OK;
    else
        return Result::FAIL;
}

Return<Result> SystemControlHal::writeWidevineKey(const hidl_array<int32_t, 4096>& value, int32_t size) {
    if (ENABLE_LOG_PRINT) ALOGI("writeWidevineKey");
    char *result = (char *)malloc(size);
    memset(result, 0, size);
    int i;
    bool ret = false;
    for (i = 0; i < size; ++i) {
        result[i] = value[i];
    }
    ret = mSysControl->writeWidevineKey(result, size);
    free(result);
    if(ret)
        return Result::OK;
    else
        return Result::FAIL;
}

Return<Result> SystemControlHal::writeAttestationKey(const hidl_array<int32_t, 10240>& value, int32_t size) {
    if (ENABLE_LOG_PRINT) ALOGI("writeAttestationKey");
    char *result = (char *)malloc(size);
    memset(result, 0, size);
    int i;
    bool ret = false;
    for (i = 0; i < size; ++i) {
        result[i] = value[i];
    }
    ret = mSysControl->writeAttestationKey(result, size);
    free(result);
    if(ret)
        return Result::OK;
    else
        return Result::FAIL;
}

Return<Result> SystemControlHal::writeHDCP14Key(const hidl_array<int32_t, 4096>& value, int32_t size) {
    if (ENABLE_LOG_PRINT) ALOGI("writeHDCP14Key");
    char *result = (char *)malloc(size);
    memset(result, 0, size);
    int i;
    bool ret = false;
    for (i = 0; i < size; ++i) {
        result[i] = value[i];
    }
    ret = mSysControl->writeHDCP14Key(result, size);
    free(result);
    if(ret)
        return Result::OK;
    else
        return Result::FAIL;
}

Return<Result> SystemControlHal::writeHdcpRX14Key(const hidl_array<int32_t, 4096>& value, int32_t size) {
    if (ENABLE_LOG_PRINT) ALOGI("writeHdcpRX14Key");
    char *result = (char *)malloc(size);
    memset(result, 0, size);
    int i;
    bool ret = false;
    for (i = 0; i < size; ++i) {
        result[i] = value[i];
    }
    ret = mSysControl->writeHdcpRX14Key(result, size);
    free(result);
    if(ret)
        return Result::OK;
    else
        return Result::FAIL;
}

Return<Result> SystemControlHal::writeHDCP22Key(const hidl_array<int32_t, 4096>& value, int32_t size) {
    if (ENABLE_LOG_PRINT) ALOGI("writeHDCP22Key");
    char *result = (char *)malloc(size);
    memset(result, 0, size);
    int i;
    bool ret = false;
    for (i = 0; i < size; ++i) {
        result[i] = value[i];
    }
    ret = mSysControl->writeHDCP22Key(result, size);
    free(result);
    if(ret)
        return Result::OK;
    else
        return Result::FAIL;
}

Return<Result> SystemControlHal::writeHdcpRX22Key(const hidl_array<int32_t, 4096>& value, int32_t size) {
    if (ENABLE_LOG_PRINT) ALOGI("writeHdcpRX22Key");
    char *result = (char *)malloc(size);
    memset(result, 0, size);
    int i;
    bool ret = false;
    for (i = 0; i < size; ++i) {
        result[i] = value[i];
    }
    ret = mSysControl->writeHdcpRX22Key(result, size);
    free(result);
    if(ret)
        return Result::OK;
    else
        return Result::FAIL;
}

Return<Result> SystemControlHal::writePFIDKey(const hidl_array<int32_t, 4096>& value, int32_t size) {
    if (ENABLE_LOG_PRINT) ALOGI("writePFIDKey");
    char *result = (char *)malloc(size);
    memset(result, 0, size);
    int i;
    bool ret = false;

    for (i = 0; i < size; ++i) {
        result[i] = value[i];
    }

    ret = mSysControl->writePFIDKey(result, size);
    free(result);
    if(ret)
        return Result::OK;
    else
        return Result::FAIL;
}

Return<Result> SystemControlHal::writePFPKKey(const hidl_array<int32_t, 4096>& value, int32_t size) {
    if (ENABLE_LOG_PRINT) ALOGI("writePFPKKey");
    char *result = (char *)malloc(size);
    memset(result, 0, size);
    int i;
    bool ret = false;

    for (i = 0; i < size; ++i) {
        result[i] = value[i];
    }

    ret = mSysControl->writePFPKKey(result, size);
    free(result);
    if(ret)
        return Result::OK;
    else
        return Result::FAIL;
}


Return<void> SystemControlHal::readUnifyKey(const hidl_string &key, readUnifyKey_cb _hidl_cb) {
    std::string value;
    mSysControl->readUnifyKey(key, value);

    if (ENABLE_LOG_PRINT) ALOGI("readUnifyKey key :%s, value:%s", key.c_str(), value.c_str());
    _hidl_cb(Result::OK, value);
    return Void();
}

Return<Result> SystemControlHal::readPlayreadyKey(const hidl_string &path, const uint32_t key_type, int32_t size) {
    bool ret = false;
    ret = mSysControl->readPlayreadyKey(path, key_type, size);
    if(ret)
        return Result::OK;
    else
        return Result::FAIL;
}

Return<Result> SystemControlHal::readNetflixKey(const uint32_t key_type, int32_t size) {
    bool ret = false;
    ret = mSysControl->readNetflixKey(key_type, size);
    if(ret)
        return Result::OK;
    else
        return Result::FAIL;
}

Return<Result> SystemControlHal::readWidevineKey(const uint32_t key_type, int32_t size) {
    bool ret = false;
    ret = mSysControl->readWidevineKey(key_type, size);
    if(ret)
        return Result::OK;
    else
        return Result::FAIL;
}

Return<Result> SystemControlHal::readAttestationKey(const uint32_t key_type, int32_t size) {
    bool ret = false;
    ret = mSysControl->readAttestationKey(key_type, size);
    if(ret)
        return Result::OK;
    else
        return Result::FAIL;
}

Return<Result> SystemControlHal::readHDCP14Key(const uint32_t key_type, int32_t size) {
    bool ret = false;
    ret = mSysControl->readHDCP14Key(key_type, size);
    if(ret)
        return Result::OK;
    else
        return Result::FAIL;
}

Return<Result> SystemControlHal::readHdcpRX14Key(const uint32_t key_type, int32_t size) {
    bool ret = false;
    ret = mSysControl->readHdcpRX14Key(key_type, size);
    if(ret)
        return Result::OK;
    else
        return Result::FAIL;
}

Return<Result> SystemControlHal::readHDCP22Key(const uint32_t key_type, int32_t size) {
    bool ret = false;
    ret = mSysControl->readHDCP22Key(key_type, size);
    if(ret)
        return Result::OK;
    else
        return Result::FAIL;
}

Return<Result> SystemControlHal::readHdcpRX22Key(const uint32_t key_type, int32_t size) {
    bool ret = false;
    ret = mSysControl->readHdcpRX22Key(key_type, size);
    if(ret)
        return Result::OK;
    else
        return Result::FAIL;
}


Return<Result> SystemControlHal::checkPlayreadyKey(const hidl_string &path, const hidl_array<int32_t, 4096>& value, const uint32_t key_type, int32_t size) {
    if (ENABLE_LOG_PRINT) ALOGI("SystemControlHal checkPlayreadyKey");
    char *result = (char *)malloc(size);
    memset(result, 0, size);
    int i;
    bool ret = false;
    for (i = 0; i < size; ++i) {
        result[i] = value[i];
    }
    ret = mSysControl->checkPlayreadyKey(path, result, key_type);
    free(result);
    if(ret)
        return Result::OK;
    else
        return Result::FAIL;
}

Return<Result> SystemControlHal::checkNetflixKey(const hidl_array<int32_t, 4096>& value, const uint32_t key_type, int32_t size) {
    if (ENABLE_LOG_PRINT) ALOGI("SystemControlHal checkNetflixKey");
    char *result = (char *)malloc(size);
    memset(result, 0, size);
    int i;
    bool ret = false;
    for (i = 0; i < size; ++i) {
        result[i] = value[i];
    }
    ret = mSysControl->checkNetflixKey(result, key_type);
    free(result);
    if(ret)
        return Result::OK;
    else
        return Result::FAIL;
}

Return<Result> SystemControlHal::checkWidevineKey(const hidl_array<int32_t, 4096>& value, const uint32_t key_type, int32_t size) {
    if (ENABLE_LOG_PRINT) ALOGI("SystemControlHal checkWidevineKey");
    char *result = (char *)malloc(size);
    memset(result, 0, size);
    int i;
    bool ret = false;
    for (i = 0; i < size; ++i) {
        result[i] = value[i];
    }
    ret = mSysControl->checkWidevineKey(result, key_type);
    free(result);
    if(ret)
        return Result::OK;
    else
        return Result::FAIL;
}

Return<Result> SystemControlHal::checkAttestationKey(const hidl_array<int32_t, 10240>& value, const uint32_t key_type, int32_t size) {
    if (ENABLE_LOG_PRINT) ALOGI("SystemControlHal checkAttestationKey");
    char *result = (char *)malloc(size);
    memset(result, 0, size);
    int i;
    bool ret = false;
    for (i = 0; i < size; ++i) {
        result[i] = value[i];
    }
    ret = mSysControl->checkAttestationKey(result, key_type);
    free(result);
    if(ret)
        return Result::OK;
    else
        return Result::FAIL;
}

Return<Result> SystemControlHal::checkHDCP14Key(const hidl_array<int32_t, 4096>& value, const uint32_t key_type, int32_t size) {
    if (ENABLE_LOG_PRINT) ALOGI("SystemControlHal checkHDCP14Key");
    char *result = (char *)malloc(size);
    memset(result, 0, size);
    int i;
    bool ret = false;
    for (i = 0; i < size; ++i) {
        result[i] = value[i];
    }
    ret = mSysControl->checkHDCP14Key(result, key_type);
    free(result);
    if(ret)
        return Result::OK;
    else
        return Result::FAIL;
}

Return<Result> SystemControlHal::checkHDCP14KeyIsExist(const uint32_t key_type) {
     return mSysControl->checkHDCP14KeyIsExist(key_type) ? Result::OK:Result::FAIL;
}

Return<Result> SystemControlHal::checkHDCP22Key(const hidl_string &path, const hidl_array<int32_t, 4096>& value, const uint32_t key_type, int32_t size) {
    if (ENABLE_LOG_PRINT) ALOGI("SystemControlHal checkHDCP22Key");
    char *result = (char *)malloc(size);
    memset(result, 0, size);
    int i;
    bool ret = false;
    for (i = 0; i < size; ++i) {
        result[i] = value[i];
    }
    ret = mSysControl->checkHDCP22Key(path, result, key_type);
    free(result);
    if(ret)
        return Result::OK;
    else
        return Result::FAIL;
}

Return<Result> SystemControlHal::checkHDCP22KeyIsExist(const uint32_t key_type_first, const uint32_t key_type_second) {
     return mSysControl->checkHDCP22KeyIsExist(key_type_first, key_type_second) ? Result::OK:Result::FAIL;
}

Return<Result> SystemControlHal::checkPFIDKeyIsExist(const uint32_t key_type) {
     return mSysControl->checkPFIDKeyIsExist(key_type) ? Result::OK:Result::FAIL;
}

Return<Result> SystemControlHal::checkPFPKKeyIsExist(const uint32_t key_type) {
     return mSysControl->checkPFPKKeyIsExist(key_type) ? Result::OK:Result::FAIL;
}

Return<void> SystemControlHal::calcChecksumKey(const hidl_array<int32_t, 10240>& value, int32_t size, calcChecksumKey_cb _hidl_cb) {
    if (ENABLE_LOG_PRINT) ALOGI("calcChecksumKey");
    std::string keyCheckSum;
    char *result = (char *)malloc(size);
    memset(result, 0, size);
    int i;
    bool ret = false;
    for (i = 0; i < size; ++i) {
        result[i] = value[i];
    }
    ret = mSysControl->calcChecksumKey(result, size, &keyCheckSum);
    free(result);
    if (ret)
        _hidl_cb(Result::OK, keyCheckSum);
    else
        _hidl_cb(Result::FAIL, keyCheckSum);
    return Void();
}
//Provision key end


Return<void> SystemControlHal::getBootEnv(const hidl_string &key, getBootEnv_cb _hidl_cb) {
    std::string value;
    bool ret = mSysControl->getBootEnv(key, value);

    if (ENABLE_LOG_PRINT)
        ALOGI("getBootEnv key :%s, value:%s", key.c_str(), value.c_str());

    if (ret == true) {
        _hidl_cb(Result::OK, value);
    } else {
        _hidl_cb(Result::FAIL, value);
    }

    return Void();
}

Return<void> SystemControlHal::setBootEnv(const hidl_string &key, const hidl_string &value) {
    mSysControl->setBootEnv(key, value);
    if (ENABLE_LOG_PRINT)
        ALOGI("setBootEnv key :%s, value:%s", key.c_str(), value.c_str());
    return Void();
}

Return<void> SystemControlHal::setHdrStrategy(const hidl_string &value) {
    mSysControl->setHdrStrategy(value);
    if (ENABLE_LOG_PRINT)
        ALOGI("setBootEnv value:%s",value.c_str());
    return Void();
}

Return<void> SystemControlHal::setHdrPriority(const hidl_string &value) {
    mSysControl->setHdrPriority(value);
    if (ENABLE_LOG_PRINT)
        ALOGI("setHdrPriority value:%s",value.c_str());
    return Void();
}

Return<Result> SystemControlHal::getModeSupportDeepColorAttr(const hidl_string &mode, const hidl_string &color) {
    if (ENABLE_LOG_PRINT) ALOGI("getModeSupportDeepColorAttr mode = %s color = %s", mode.c_str(), color.c_str());
    return mSysControl->getModeSupportDeepColorAttr(mode, color)?Result::OK:Result::FAIL;
}


Return<void> SystemControlHal::getDroidDisplayInfo(getDroidDisplayInfo_cb _hidl_cb) {
    DroidDisplayInfo info;

    /*std::string type;
    std::string ui;
    mSysControl->getDroidDisplayInfo(info.type, type, ui,
        info.fb0w, info.fb0h, info.fb0bits, info.fb0trip,
        info.fb1w, info.fb1h, info.fb1bits, info.fb1trip);

    info.socType = type;
    info.defaultUI = ui;*/
    _hidl_cb(Result::OK, info);
    return Void();
}

Return<void> SystemControlHal::loopMountUnmount(int32_t isMount, const hidl_string& path) {
    if (ENABLE_LOG_PRINT) ALOGI("loopMountUnmount isMount :%d, path:%s", isMount, path.c_str());
    mSysControl->loopMountUnmount(isMount, path);
    return Void();
}

Return<void> SystemControlHal::setSourceOutputMode(const hidl_string& mode) {
    ALOGI("setSourceOutputMode mode:%s", mode.c_str());
    mSysControl->setSourceOutputMode(mode);
    return Void();
}

Return<void> SystemControlHal::setSinkOutputMode(const hidl_string& mode) {
    if (ENABLE_LOG_PRINT) ALOGI("setSinkOutputMode mode:%s", mode.c_str());
    mSysControl->setSinkOutputMode(mode);
    return Void();
}

Return<void> SystemControlHal::setDigitalMode(const hidl_string& mode) {
    if (ENABLE_LOG_PRINT) ALOGI("setDigitalMode mode:%s", mode.c_str());
    mSysControl->setDigitalMode(mode);
    return Void();
}

Return<void> SystemControlHal::setOsdMouseMode(const hidl_string& mode) {
    if (ENABLE_LOG_PRINT) ALOGI("setOsdMouseMode mode:%s", mode.c_str());
    mSysControl->setOsdMouseMode(mode);
    return Void();
}

Return<void> SystemControlHal::setOsdMousePara(int32_t x, int32_t y, int32_t w, int32_t h) {
    mSysControl->setOsdMousePara(x, y, w, h);
    return Void();
}

Return<void> SystemControlHal::setPosition(int32_t left, int32_t top, int32_t width, int32_t height) {
    mSysControl->setPosition(left, top, width, height);
    return Void();
}

Return<void> SystemControlHal::getPosition(const hidl_string& mode, getPosition_cb _hidl_cb) {
    int x, y, w, h;
    mSysControl->getPosition(mode, x, y, w, h);
    _hidl_cb(Result::OK, x, y, w, h);
    return Void();
}

Return<void> SystemControlHal::saveDeepColorAttr(const hidl_string& mode, const hidl_string& dcValue) {
    mSysControl->saveDeepColorAttr(mode, dcValue);
    return Void();
}

Return<void> SystemControlHal::getDeepColorAttr(const hidl_string &mode, getDeepColorAttr_cb _hidl_cb) {
    std::string value;
    mSysControl->getDeepColorAttr(mode, value);

    if (ENABLE_LOG_PRINT) ALOGI("getDeepColorAttr mode :%s, value:%s", mode.c_str(), value.c_str());
    _hidl_cb(Result::OK, value);
    return Void();
}

Return<void> SystemControlHal::setDolbyVisionState(int32_t state) {
    mSysControl->setDolbyVisionEnable(state);
    return Void();
}

Return<void> SystemControlHal::setALLMState(int32_t state) {
    mSysControl->setALLMMode(state);
    return Void();
}

Return<void> SystemControlHal::sendHDMIContentType(int32_t state) {
    mSysControl->sendHDMIContentType(state);
    return Void();
}

Return<bool> SystemControlHal::getAllmSupport(void) {
    return mSysControl->isTvSupportALLM();
}

Return<bool> SystemControlHal::getGameContentTypeSupport(void) {
    return mSysControl->getGameContentTypeSupport();
}

Return<void> SystemControlHal::getSupportALLMContentTypeList(getSupportALLMContentTypeList_cb _hidl_cb) {
    std::vector<std::string> supportModes;
    mSysControl->getSupportALLMContentTypeList(&supportModes);

    hidl_vec<hidl_string> hidlList;
    hidlList.resize(supportModes.size());
    for (size_t i = 0; i < supportModes.size(); ++i) {
        hidlList[i] = supportModes[i];

        //if (ENABLE_LOG_PRINT)
            ALOGI("getSupportALLMContentTypeList index:%ld mode :%s", (unsigned long)i, supportModes[i].c_str());
    }

    if(supportModes.size() == 0)
        ALOGI("getSupportALLMContentTypeList supportModes.size() == 0");

    _hidl_cb(Result::OK, hidlList);
    return Void();
}


Return<void> SystemControlHal::sinkSupportDolbyVision(sinkSupportDolbyVision_cb _hidl_cb) {
    std::string mode;
    bool support = true;
    mSysControl->isTvSupportDolbyVision(mode);

    if (ENABLE_LOG_PRINT) ALOGI("getTvDolbyVision mode :%s, dv support:%d", mode.c_str(), support);
    _hidl_cb(Result::OK, mode, support);
    return Void();
}

Return<void> SystemControlHal::getDolbyVisionType(getDolbyVisionType_cb _hidl_cb) {
    int32_t value = mSysControl->getDolbyVisionType();

    if (ENABLE_LOG_PRINT) ALOGI("getDolbyVisionType value:%d", value);
    _hidl_cb(Result::OK, value);
    return Void();
}

Return<void> SystemControlHal::setGraphicsPriority(const hidl_string& mode) {
    if (ENABLE_LOG_PRINT) ALOGI("setGraphicsPriority mode:%s", mode.c_str());
    mSysControl->setGraphicsPriority(mode);
    return Void();
}

Return<void> SystemControlHal::getGraphicsPriority(getGraphicsPriority_cb _hidl_cb) {
    std::string mode;
    mSysControl->getGraphicsPriority(mode);

    if (ENABLE_LOG_PRINT) ALOGI("getGraphicsPriority mode :%s", mode.c_str());
    _hidl_cb(Result::OK, mode);
    return Void();
}

Return<void> SystemControlHal::setHdrMode(const hidl_string& mode) {
    if (ENABLE_LOG_PRINT) ALOGI("setHdrMode mode:%s", mode.c_str());
    mSysControl->setHdrMode(mode);
    return Void();
}

Return<void> SystemControlHal::setSdrMode(const hidl_string& mode) {
    if (ENABLE_LOG_PRINT) ALOGI("setSdrMode mode:%s", mode.c_str());
    mSysControl->setSdrMode(mode);
    return Void();
}

Return<void> SystemControlHal::resolveResolutionValue(const hidl_string& mode, resolveResolutionValue_cb _hidl_cb) {
    int64_t value = mSysControl->resolveResolutionValue(mode);
    _hidl_cb(Result::OK, value);
    return Void();
}

Return<void> SystemControlHal::setCallback(const sp<ISystemControlCallback>& callback) {
    if (callback != nullptr) {
        int cookie = -1;
        int clientSize = mClients.size();
        for (int i = 0; i < clientSize; i++) {
            if (mClients[i] == nullptr) {
                SYS_LOGI("%s, client index:%d had died, this id give the new client", __FUNCTION__, i);
                cookie = i;
                mClients[i] = callback;
                break;
            }
        }

        if (cookie < 0) {
            cookie = clientSize;
            mClients[clientSize] = callback;
        }

        Return<bool> linkResult = callback->linkToDeath(mDeathRecipient, cookie);
        bool linkSuccess = linkResult.isOk() ? static_cast<bool>(linkResult) : false;
        if (!linkSuccess) {
            SYS_LOGE("Couldn't link death recipient for cookie: %d", cookie);
        }

        SYS_LOGI("%s cookie:%d, client size:%d", __FUNCTION__, cookie, (int)mClients.size());
    }

    return Void();
}

Return<Result> SystemControlHal::setAppInfo(const hidl_string& pkg, const hidl_string& cls, const hidl_vec<hidl_string>& proc) {
    std::vector<std::string> procList;
    for (size_t i = 0; i < proc.size(); i++) {
        procList.push_back(proc[i]);
    }

    mSysControl->setAppInfo(pkg, cls, procList);
    return Result::OK;
}

Return<void> SystemControlHal::getPrefHdmiDispMode(getPrefHdmiDispMode_cb _hidl_cb) {
    std::string mode;
    bool ret = mSysControl->getPrefHdmiDispMode(&mode);
    if (ENABLE_LOG_PRINT) ALOGI("getPrefHdmiDispMode mode :%s", mode.c_str());
    if (ret == true)
        _hidl_cb(Result::OK, mode);
    else
        _hidl_cb(Result::FAIL, mode);
    return Void();
}

//for 3D
Return<void> SystemControlHal::set3DMode(const hidl_string& mode) {
    if (ENABLE_LOG_PRINT) ALOGI("set3DMode mode:%s", mode.c_str());
    mSysControl->set3DMode(mode);
    return Void();
}

Return<void> SystemControlHal::init3DSetting() {
    mSysControl->init3DSetting();
    return Void();
}

Return<void> SystemControlHal::getVideo3DFormat(getVideo3DFormat_cb _hidl_cb) {
    int32_t format = mSysControl->getVideo3DFormat();
    _hidl_cb(Result::OK, format);
    return Void();
}

Return<void> SystemControlHal::getDisplay3DTo2DFormat(getDisplay3DTo2DFormat_cb _hidl_cb) {
    int32_t format = mSysControl->getDisplay3DTo2DFormat();
    _hidl_cb(Result::OK, format);
    return Void();
}

Return<void> SystemControlHal::setDisplay3DTo2DFormat(int32_t format) {
    mSysControl->setDisplay3DTo2DFormat(format);
    return Void();
}

Return<void> SystemControlHal::setDisplay3DFormat(int32_t format) {
    mSysControl->setDisplay3DFormat(format);
    return Void();
}

Return<void> SystemControlHal::getDisplay3DFormat(getDisplay3DFormat_cb _hidl_cb) {
    int32_t format = mSysControl->getDisplay3DFormat();
    _hidl_cb(Result::OK, format);
    return Void();
}

Return<void> SystemControlHal::setOsd3DFormat(int32_t format) {
    mSysControl->setOsd3DFormat(format);
    return Void();
}

Return<void> SystemControlHal::switch3DTo2D(int32_t format) {
    mSysControl->switch3DTo2D(format);
    return Void();
}

Return<void> SystemControlHal::switch2DTo3D(int32_t format) {
    mSysControl->switch2DTo3D(format);
    return Void();
}

Return<void> SystemControlHal::autoDetect3DForMbox() {
    mSysControl->autoDetect3DForMbox();
    return Void();
}
//3D end
//PQ
Return<int32_t> SystemControlHal::loadPQSettings(const SourceInputParam& srcInputParam) {
    source_input_param_t srcInput;
    memcpy(&srcInput, &srcInputParam, sizeof(SourceInputParam));
    return mSysControl->loadPQSettings(srcInput);
}

Return<int32_t> SystemControlHal::setPQmode(int32_t mode, int32_t isSave, int32_t is_autoswitch) {
    return mSysControl->setPQmode(mode, isSave, is_autoswitch);
}

Return<int32_t> SystemControlHal::getPQmode(void) {
    return mSysControl->getPQmode();
}

Return<int32_t> SystemControlHal::savePQmode(int32_t mode) {
    return mSysControl->savePQmode(mode);
}

Return<int32_t> SystemControlHal::getLastPQmode(void) {
    return mSysControl->getLastPQmode();
}

Return<int32_t> SystemControlHal::setColorTemperature(int32_t mode, int32_t isSave) {
    return mSysControl->setColorTemperature(mode, isSave);
}

Return<int32_t> SystemControlHal::getColorTemperature(void) {
    return mSysControl->getColorTemperature();
}

Return<int32_t> SystemControlHal::saveColorTemperature(int32_t mode) {
    return mSysControl->saveColorTemperature(mode);
}

Return<int32_t> SystemControlHal::setColorTemperatureUserParam(int32_t mode, int32_t isSave,
                              int32_t param_type, int32_t value) {
    return mSysControl->setColorTemperatureUserParam(mode, isSave, param_type, value);
}

Return<void> SystemControlHal::getColorTemperatureUserParam(getColorTemperatureUserParam_cb _hidl_cb) {
    tcon_rgb_ogo_t tempParam = mSysControl->getColorTemperatureUserParam();
    WhiteBalanceParam param;
    param.en = tempParam.en;
    param.r_gain = tempParam.r_gain;
    param.g_gain = tempParam.g_gain;
    param.b_gain = tempParam.b_gain;
    param.r_pre_offset = tempParam.r_pre_offset;
    param.g_pre_offset = tempParam.g_pre_offset;
    param.b_pre_offset = tempParam.b_pre_offset;
    param.r_post_offset = tempParam.r_post_offset;
    param.g_post_offset = tempParam.g_post_offset;
    param.b_post_offset = tempParam.b_post_offset;
    _hidl_cb(param);
    return Void();
}

Return<int32_t> SystemControlHal::setBrightness(int32_t value, int32_t isSave) {
    return mSysControl->setBrightness(value, isSave);
}

Return<int32_t> SystemControlHal::getBrightness(void) {
    return mSysControl->getBrightness();
}

Return<int32_t> SystemControlHal::saveBrightness(int32_t value) {
    return mSysControl->saveBrightness(value);
}

Return<int32_t> SystemControlHal::setContrast(int32_t value, int32_t isSave) {
    return mSysControl->setContrast(value, isSave);
}

Return<int32_t> SystemControlHal::getContrast(void) {
    return mSysControl->getContrast();
}

Return<int32_t> SystemControlHal::saveContrast(int32_t value) {
    return mSysControl->saveContrast(value);
}

Return<int32_t> SystemControlHal::setSaturation(int32_t value, int32_t isSave) {
    return mSysControl->setSaturation(value, isSave);
}

Return<int32_t> SystemControlHal::getSaturation(void) {
    return mSysControl->getSaturation();
}

Return<int32_t> SystemControlHal::saveSaturation(int32_t value) {
    return mSysControl->saveSaturation(value);
}

Return<int32_t> SystemControlHal::setHue(int32_t value, int32_t isSave) {
    return mSysControl->setHue(value, isSave);
}

Return<int32_t> SystemControlHal::getHue(void) {
    return mSysControl->getHue();
}

Return<int32_t> SystemControlHal::saveHue(int value) {
    return mSysControl->saveHue(value);
}

Return<int32_t> SystemControlHal::setSharpness(int32_t value, int32_t is_enable, int32_t isSave) {
    return mSysControl->setSharpness(value, is_enable, isSave);
}

Return<int32_t> SystemControlHal::getSharpness(void) {
    return mSysControl->getSharpness();
}

Return<int32_t> SystemControlHal::saveSharpness(int32_t value) {
    return mSysControl->saveSharpness(value);
}

Return<int32_t> SystemControlHal::setNoiseReductionMode(int32_t nr_mode, int32_t isSave) {
    return mSysControl->setNoiseReductionMode(nr_mode, isSave);
}

Return<int32_t> SystemControlHal::getNoiseReductionMode(void) {
    return mSysControl->getNoiseReductionMode();
}

Return<int32_t> SystemControlHal::saveNoiseReductionMode(int32_t nr_mode) {
    return mSysControl->saveNoiseReductionMode(nr_mode);
}

Return<int32_t> SystemControlHal::setSmoothPlusMode(int32_t smoothplus_mode, int32_t isSave) {
    return mSysControl->setSmoothPlusMode(smoothplus_mode, isSave);
}

Return<int32_t> SystemControlHal::getSmoothPlusMode(void) {
    return mSysControl->getSmoothPlusMode();
}

Return<int32_t> SystemControlHal::setHDRTMOMode(int32_t hdr_tmo_mode, int32_t isSave) {
    return mSysControl->setHDRTMOMode(hdr_tmo_mode, isSave);
}

Return<int32_t> SystemControlHal::getHDRTMOMode(void) {
    return mSysControl->getHDRTMOMode();
}

Return<int32_t> SystemControlHal::setEyeProtectionMode(int32_t inputSrc, int32_t enable, int32_t isSave) {
    return mSysControl->setEyeProtectionMode(inputSrc, enable, isSave);
}

Return<int32_t> SystemControlHal::getEyeProtectionMode(int32_t inputSrc) {
    return mSysControl->getEyeProtectionMode(inputSrc);
}

Return<int32_t> SystemControlHal::setGammaValue(int32_t gamma_curve, int32_t isSave) {
    return mSysControl->setGammaValue(gamma_curve, isSave);
}

Return<int32_t> SystemControlHal::getGammaValue(void) {
    return mSysControl->getGammaValue();
}

Return<Result> SystemControlHal::hasMemcFunc() {
    if (mSysControl->hasMemcFunc()) {
        return Result::OK;
    }
    return Result::FAIL;
}

Return<Result> SystemControlHal::aisrContrl(bool on) {
    if (mSysControl->aisrContrl(on)) {
        return Result::OK;
    }
    return Result::FAIL;
}
Return<Result> SystemControlHal::hasAisrFunc() {
    if (mSysControl->hasAisrFunc()) {
        return Result::OK;
    }
    return Result::FAIL;
}
Return<Result> SystemControlHal::getAisr() {
    if (mSysControl->getAisr()) {
        return Result::OK;
    }
    return Result::FAIL;
}

Return<int32_t> SystemControlHal::setMemcMode(int32_t memc_mode, int32_t isSave) {
    return mSysControl->setMemcMode(memc_mode, isSave);
}

Return<int32_t> SystemControlHal::getMemcMode(void) {
    return mSysControl->getMemcMode();
}

Return<int32_t> SystemControlHal::setMemcDeBlurLevel(int32_t level, int32_t isSave) {
    return mSysControl->setMemcDeBlurLevel(level, isSave);
}

Return<int32_t> SystemControlHal::getMemcDeBlurLevel(void) {
    return mSysControl->getMemcDeBlurLevel();
}

Return<int32_t> SystemControlHal::setMemcDeJudderLevel(int32_t level, int32_t isSave) {
    return mSysControl->setMemcDeJudderLevel(level, isSave);
}

Return<int32_t> SystemControlHal::getMemcDeJudderLevel(void) {
    return mSysControl->getMemcDeJudderLevel();
}

Return<int32_t> SystemControlHal::setDisplayMode(int32_t inputSrc, int32_t mode, int32_t isSave) {
    return mSysControl->setDisplayMode(inputSrc, mode, isSave);
}

Return<int32_t> SystemControlHal::getDisplayMode(int32_t inputSrc) {
    return mSysControl->getDisplayMode(inputSrc);
}

Return<int32_t> SystemControlHal::saveDisplayMode(int32_t inputSrc, int32_t mode) {
    return mSysControl->saveDisplayMode(inputSrc, mode);
}

Return<int32_t> SystemControlHal::setBacklight(int32_t value, int32_t isSave) {
    return mSysControl->setBacklight(value, isSave);
}

Return<int32_t> SystemControlHal::getBacklight(void) {
    return mSysControl->getBacklight();
}

Return<int32_t> SystemControlHal::saveBacklight(int32_t value) {
    return mSysControl->saveBacklight(value);
}

Return<int32_t> SystemControlHal::setDynamicBacklight(int32_t mode, int32_t isSave) {
    return mSysControl->setDynamicBacklight(mode, isSave);
}

Return<int32_t> SystemControlHal::getDynamicBacklight(void) {
    return mSysControl->getDynamicBacklight();
}

Return<int32_t> SystemControlHal::setLocalContrastMode(int32_t mode, int32_t isSave) {
    return mSysControl->setLocalContrastMode(mode, isSave);
}

Return<int32_t> SystemControlHal::getLocalContrastMode() {
    return mSysControl->getLocalContrastMode();
}

Return<int32_t> SystemControlHal::setBlackExtensionMode(int32_t mode, int32_t isSave) {
    return mSysControl->setBlackExtensionMode(mode, isSave);
}

Return<int32_t> SystemControlHal::getBlackExtensionMode() {
    return mSysControl->getBlackExtensionMode();
}

Return<int32_t> SystemControlHal::setDeblockMode(int32_t mode, int32_t isSave) {
    return mSysControl->setDeblockMode(mode, isSave);
}

Return<int32_t> SystemControlHal::getDeblockMode() {
    return mSysControl->getDeblockMode();
}

Return<int32_t> SystemControlHal::setDemoSquitoMode(int32_t mode, int32_t isSave) {
    return mSysControl->setDemoSquitoMode(mode, isSave);
}

Return<int32_t> SystemControlHal::getDemoSquitoMode() {
    return mSysControl->getDemoSquitoMode();
}

Return<int32_t> SystemControlHal::setColorBaseMode(int32_t mode, int32_t isSave) {
    return mSysControl->setColorBaseMode(mode, isSave);
}

Return<int32_t> SystemControlHal::getColorBaseMode() {
    return mSysControl->getColorBaseMode();
}

Return<int32_t> SystemControlHal::getSourceHdrType() {
    return mSysControl->getSourceHdrType();
}

Return<int32_t> SystemControlHal::checkLdimExist(void) {
    bool ret = false;
    ret = mSysControl->checkLdimExist();
    if (ret) {
        return 1;
    } else {
        return 0;
    }
}

Return<int32_t> SystemControlHal::factoryResetPQMode(void) {
    return mSysControl->factoryResetPQMode();
}

Return<int32_t> SystemControlHal::factorySetPQMode_Brightness(int32_t inputSrc, int32_t sigFmt, int32_t transFmt, int32_t pq_mode, int32_t value) {
    return mSysControl->factorySetPQMode_Brightness(inputSrc, sigFmt, transFmt, pq_mode, value);
}

Return<int32_t> SystemControlHal::factoryGetPQMode_Brightness(int32_t inputSrc, int32_t sigFmt, int32_t transFmt, int32_t pq_mode) {
    return mSysControl->factoryGetPQMode_Brightness(inputSrc, sigFmt, transFmt, pq_mode);
}

Return<int32_t> SystemControlHal::factorySetPQMode_Contrast(int32_t inputSrc, int32_t sigFmt, int32_t transFmt, int32_t pq_mode, int32_t value) {
    return mSysControl->factorySetPQMode_Contrast(inputSrc, sigFmt, transFmt, pq_mode, value);
}

Return<int32_t> SystemControlHal::factoryGetPQMode_Contrast(int32_t inputSrc, int32_t sigFmt, int32_t transFmt, int32_t pq_mode) {
    return mSysControl->factoryGetPQMode_Contrast(inputSrc, sigFmt, transFmt, pq_mode);
}

Return<int32_t> SystemControlHal::factorySetPQMode_Saturation(int32_t inputSrc, int32_t sigFmt, int32_t transFmt, int32_t pq_mode, int32_t value) {
    return mSysControl->factorySetPQMode_Saturation(inputSrc, sigFmt, transFmt, pq_mode, value);
}

Return<int32_t> SystemControlHal::factoryGetPQMode_Saturation(int32_t inputSrc, int32_t sigFmt, int32_t transFmt, int32_t pq_mode) {
    return mSysControl->factoryGetPQMode_Saturation(inputSrc, sigFmt, transFmt, pq_mode);
}

Return<int32_t> SystemControlHal::factorySetPQMode_Hue(int32_t inputSrc, int32_t sigFmt, int32_t transFmt, int32_t pq_mode, int32_t value) {
    return mSysControl->factorySetPQMode_Hue(inputSrc, sigFmt, transFmt, pq_mode, value);
}

Return<int32_t> SystemControlHal::factoryGetPQMode_Hue(int32_t inputSrc, int32_t sigFmt, int32_t transFmt, int32_t pq_mode) {
    return mSysControl->factoryGetPQMode_Hue(inputSrc, sigFmt, transFmt, pq_mode);
}

Return<int32_t> SystemControlHal::factorySetPQMode_Sharpness(int32_t inputSrc, int32_t sigFmt, int32_t transFmt, int32_t pq_mode, int32_t value) {
    return mSysControl->factorySetPQMode_Sharpness(inputSrc, sigFmt, transFmt, pq_mode, value);
}

Return<int32_t> SystemControlHal::factoryGetPQMode_Sharpness(int32_t inputSrc, int32_t sigFmt, int32_t transFmt, int32_t pq_mode) {
    return mSysControl->factoryGetPQMode_Sharpness(inputSrc, sigFmt, transFmt, pq_mode);
}

Return<int32_t> SystemControlHal::factoryResetColorTemp(void) {
    return mSysControl->factoryResetColorTemp();
}

Return<int32_t> SystemControlHal::factorySetOverscan(int32_t inputSrc, int32_t sigFmt, int32_t transFmt, int32_t he_value, int32_t hs_value, int32_t ve_value, int32_t vs_value) {
    return mSysControl->factorySetOverscan(inputSrc, sigFmt, transFmt, he_value, hs_value, ve_value, vs_value);
}

Return<void> SystemControlHal::factoryGetOverscan(int32_t inputSrc, int32_t sigFmt, int32_t transFmt, factoryGetOverscan_cb _hidl_cb) {
    tvin_cutwin_t tempParam = mSysControl->factoryGetOverscan(inputSrc, sigFmt, transFmt);
    OverScanParam param;
    param.he = tempParam.he;
    param.hs = tempParam.hs;
    param.ve = tempParam.ve;
    param.vs = tempParam.vs;
    _hidl_cb(param);
    return Void();
}

Return<int32_t> SystemControlHal::factorySetNolineParams(int32_t inputSrc, int32_t sigFmt, int32_t transFmt, int32_t type, int32_t osd0_value, int32_t osd25_value,
                            int32_t osd50_value, int32_t osd75_value, int32_t osd100_value) {
    return mSysControl->factorySetNolineParams(inputSrc, sigFmt, transFmt, type, osd0_value, osd25_value,
                                               osd50_value, osd75_value, osd100_value);
}

Return<void> SystemControlHal::factoryGetNolineParams(int inputSrc, int32_t sigFmt, int32_t transFmt, int32_t type,
factoryGetNolineParams_cb _hidl_cb) {
    noline_params_t tempParam = mSysControl->factoryGetNolineParams(inputSrc, sigFmt, transFmt, type);
    NolineParam nolineParam;
    nolineParam.osd0 = tempParam.osd0;
    nolineParam.osd25 = tempParam.osd25;
    nolineParam.osd50 = tempParam.osd50;
    nolineParam.osd75 = tempParam.osd75;
    nolineParam.osd100 = tempParam.osd100;
    _hidl_cb(nolineParam);
    return Void();
}

Return<int32_t> SystemControlHal::factoryfactoryGetColorTemperatureParams(int32_t colorTemp_mode) {
    return mSysControl->factoryGetColorTemperatureParams(colorTemp_mode);
}

Return<int32_t> SystemControlHal::factorySetParamsDefault(void) {
    return mSysControl->factorySetParamsDefault();
}

Return<int32_t> SystemControlHal::factorySSMRestore(void) {
    return mSysControl->factorySSMRestore();
}

Return<int32_t> SystemControlHal::factoryResetNonlinear(void) {
    return mSysControl->factoryResetNonlinear();
}

Return<int32_t> SystemControlHal::factorySetGamma(int32_t gamma_r, int32_t gamma_g, int32_t gamma_b) {
    return mSysControl->factorySetGamma(gamma_r, gamma_g, gamma_b);
}

Return<int32_t> SystemControlHal::sysSSMReadNTypes(int32_t id, int32_t data_len, int32_t offset) {
    return mSysControl->sysSSMReadNTypes(id, data_len, offset);
}

Return<int32_t> SystemControlHal::sysSSMWriteNTypes(int32_t id, int32_t data_len, int32_t data_buf, int32_t offset) {
    return mSysControl->sysSSMWriteNTypes(id, data_len, data_buf, offset);
}

Return<int32_t> SystemControlHal::getActualAddr(int32_t id) {
    return mSysControl->getActualAddr(id);
}

Return<int32_t> SystemControlHal::getActualSize(int32_t id) {
    return mSysControl->getActualSize(id);
}

Return<int32_t> SystemControlHal::SSMRecovery(void) {
    return mSysControl->SSMRecovery();
}

Return<int32_t> SystemControlHal::setPLLValues(const SourceInputParam& srcInputParam) {
    source_input_param_t srcInput;
    memcpy(&srcInput, &srcInputParam, sizeof(SourceInputParam));
    return mSysControl->setPLLValues(srcInput);
}

Return<int32_t> SystemControlHal::setCVD2Values() {
    return mSysControl->setCVD2Values();
}

Return<int32_t> SystemControlHal::getSSMStatus(void) {
    return mSysControl->getSSMStatus();
}

Return<int32_t> SystemControlHal::setCurrentSourceInfo(int32_t sourceInput, int32_t sigFmt, int32_t transFmt) {
    return mSysControl->setCurrentSourceInfo(sourceInput, sigFmt, transFmt);
}

Return<void> SystemControlHal::getCurrentSourceInfo(getCurrentSourceInfo_cb _hidl_cb) {
    SourceInputParam srcInput;
    source_input_param_t tmpSrcInput = mSysControl->getCurrentSourceInfo();
    srcInput.sourceInput = tmpSrcInput.source_input;
    srcInput.sigFmt = tmpSrcInput.sig_fmt;
    srcInput.transFmt = tmpSrcInput.trans_fmt;
    _hidl_cb(Result::OK, srcInput);
    return Void();
}

Return<int32_t> SystemControlHal::setCurrentHdrInfo(int32_t hdrInfo) {
    return mSysControl->setCurrentHdrInfo(hdrInfo);
}

Return<int32_t> SystemControlHal::setCurrentAspectRatioInfo(int32_t aspectRatioInfo) {
    return mSysControl->setCurrentAspectRatioInfo(aspectRatioInfo);
}

Return<int32_t> SystemControlHal::setwhiteBalanceGainRed(int32_t inputSrc, int32_t sigFmt, int32_t transFmt, int32_t colortemp_mode, int32_t value) {
    return mSysControl->setwhiteBalanceGainRed(inputSrc, sigFmt, transFmt, colortemp_mode, value);
}

Return<int32_t> SystemControlHal::setwhiteBalanceGainGreen(int32_t inputSrc, int32_t sigFmt, int32_t transFmt, int32_t colortemp_mode, int32_t value) {
    return mSysControl->setwhiteBalanceGainGreen(inputSrc, sigFmt, transFmt, colortemp_mode, value);
}

Return<int32_t> SystemControlHal::setwhiteBalanceGainBlue(int32_t inputSrc, int32_t sigFmt, int32_t transFmt, int32_t colortemp_mode, int32_t value) {
    return mSysControl->setwhiteBalanceGainBlue(inputSrc, sigFmt, transFmt, colortemp_mode, value);
}

Return<int32_t> SystemControlHal::setwhiteBalanceOffsetRed(int32_t inputSrc, int32_t sigFmt, int32_t transFmt, int32_t colortemp_mode, int32_t value) {
    return mSysControl->setwhiteBalanceOffsetRed(inputSrc, sigFmt, transFmt, colortemp_mode, value);
}

Return<int32_t> SystemControlHal::setwhiteBalanceOffsetGreen(int32_t inputSrc, int32_t sigFmt, int32_t transFmt, int32_t colortemp_mode, int32_t value) {
    return mSysControl->setwhiteBalanceOffsetGreen(inputSrc, sigFmt, transFmt, colortemp_mode, value);
}

Return<int32_t> SystemControlHal::setwhiteBalanceOffsetBlue(int32_t inputSrc, int32_t sigFmt, int32_t transFmt, int32_t colortemp_mode, int32_t value) {
    return mSysControl->setwhiteBalanceOffsetBlue(inputSrc, sigFmt, transFmt, colortemp_mode, value);
}

Return<int32_t> SystemControlHal::getwhiteBalanceGainRed(int32_t inputSrc, int32_t sigFmt, int32_t transFmt, int32_t colortemp_mode) {
    return mSysControl->getwhiteBalanceGainRed(inputSrc, sigFmt, transFmt, colortemp_mode);
}

Return<int32_t> SystemControlHal::getwhiteBalanceGainGreen(int32_t inputSrc, int32_t sigFmt, int32_t transFmt, int32_t colortemp_mode) {
    return mSysControl->getwhiteBalanceGainGreen(inputSrc, sigFmt, transFmt, colortemp_mode);
}

Return<int32_t> SystemControlHal::getwhiteBalanceGainBlue(int32_t inputSrc, int32_t sigFmt, int32_t transFmt, int32_t colortemp_mode) {
   return mSysControl->getwhiteBalanceGainBlue(inputSrc, sigFmt, transFmt, colortemp_mode);
}

Return<int32_t> SystemControlHal::getwhiteBalanceOffsetRed(int32_t inputSrc, int32_t sigFmt, int32_t transFmt, int32_t colortemp_mode) {
    return mSysControl->getwhiteBalanceOffsetRed(inputSrc, sigFmt, transFmt, colortemp_mode);
}

Return<int32_t> SystemControlHal::getwhiteBalanceOffsetGreen(int32_t inputSrc, int32_t sigFmt, int32_t transFmt, int32_t colortemp_mode) {
    return mSysControl->getwhiteBalanceOffsetGreen(inputSrc, sigFmt, transFmt, colortemp_mode);
}

Return<int32_t> SystemControlHal::getwhiteBalanceOffsetBlue(int32_t inputSrc, int32_t sigFmt, int32_t transFmt, int32_t colortemp_mode) {
    return mSysControl->getwhiteBalanceOffsetBlue(inputSrc, sigFmt, transFmt, colortemp_mode);
}

Return<int32_t> SystemControlHal::saveWhiteBalancePara(int32_t inputSrc, int32_t sigFmt, int32_t transFmt, int32_t colorTemp_mode, int32_t r_gain, int32_t g_gain, int32_t b_gain, int32_t r_offset, int32_t g_offset, int32_t b_offset) {
    return mSysControl->saveWhiteBalancePara(inputSrc, sigFmt, transFmt, colorTemp_mode, r_gain, g_gain, b_gain, r_offset, g_offset, b_offset);
}

Return<int32_t> SystemControlHal::getRGBPattern() {
    return mSysControl->getRGBPattern();
}

Return<int32_t> SystemControlHal::setRGBPattern(int32_t r, int32_t g, int32_t b) {
    return mSysControl->setRGBPattern(r, g, b);
}

Return<int32_t> SystemControlHal::factorySetDDRSSC(int32_t step) {
    return mSysControl->factorySetDDRSSC(step);
}

Return<int32_t> SystemControlHal::factoryGetDDRSSC() {
    return mSysControl->factoryGetDDRSSC();
}

Return<int32_t> SystemControlHal::factorySetLVDSSSC(int32_t step) {
    return mSysControl->factorySetLVDSSSC(step);
}

Return<int32_t> SystemControlHal::factoryGetLVDSSSC() {
    return mSysControl->factoryGetLVDSSSC();
}

Return<int32_t> SystemControlHal::whiteBalanceGrayPatternClose() {
    return mSysControl->whiteBalanceGrayPatternClose();
}

Return<int32_t> SystemControlHal::whiteBalanceGrayPatternOpen() {
    return mSysControl->whiteBalanceGrayPatternOpen();
}

Return<int32_t> SystemControlHal::whiteBalanceGrayPatternSet(int value) {
    return mSysControl->whiteBalanceGrayPatternSet(value);
}

Return<int32_t> SystemControlHal::whiteBalanceGrayPatternGet() {
    return mSysControl->whiteBalanceGrayPatternGet();
}

Return<int32_t> SystemControlHal::factorySetHdrMode(int32_t mode) {
    return mSysControl->factorySetHdrMode(mode);
}

Return<int32_t> SystemControlHal::factoryGetHdrMode(void) {
    return mSysControl->factoryGetHdrMode();
}

Return<int32_t> SystemControlHal::setDnlpParams(int32_t inputSrc, int32_t sig_fmt, int32_t trans_fmt, int32_t level) {
    return mSysControl->setDnlpParams(inputSrc, sig_fmt, trans_fmt, level);
}

Return<int32_t> SystemControlHal::getDnlpParams(int32_t inputSrc, int32_t sig_fmt, int32_t trans_fmt) {
    return mSysControl->getDnlpParams(inputSrc, sig_fmt, trans_fmt);
}

Return<int32_t> SystemControlHal::factorySetDnlpParams(int32_t inputSrc, int32_t sig_fmt, int32_t trans_fmt, int32_t level, int32_t final_gain) {
    return mSysControl->factorySetDnlpParams(inputSrc, sig_fmt, trans_fmt, level, final_gain);
}

Return<int32_t> SystemControlHal::factoryGetDnlpParams(int32_t inputSrc, int32_t sig_fmt, int32_t trans_fmt, int32_t level) {
    return mSysControl->factoryGetDnlpParams(inputSrc, sig_fmt, trans_fmt, level);
}

Return<int32_t> SystemControlHal::factorySetBlackExtRegParams(int32_t inputSrc, int32_t sig_fmt, int32_t trans_fmt, int32_t val) {
    return mSysControl->factorySetBlackExtRegParams(inputSrc, sig_fmt, trans_fmt, val);
}

Return<int32_t> SystemControlHal::factoryGetBlackExtRegParams(int32_t inputSrc, int32_t sig_fmt, int32_t trans_fmt) {
    return mSysControl->factoryGetBlackExtRegParams(inputSrc, sig_fmt, trans_fmt);
}

Return<int32_t> SystemControlHal::factorySetColorParams(int32_t inputSrc, int32_t sig_fmt, int32_t trans_fmt, int32_t color_type, int32_t color_param, int32_t val) {
    return mSysControl->factorySetColorParams(inputSrc, sig_fmt, trans_fmt, color_type, color_param, val);
}

Return<int32_t> SystemControlHal::factoryGetColorParams(int32_t inputSrc, int32_t sig_fmt, int32_t trans_fmt, int32_t color_type, int32_t color_param) {
    return mSysControl->factoryGetColorParams(inputSrc, sig_fmt, trans_fmt, color_type, color_param);
}

Return<int32_t> SystemControlHal::factorySetNoiseReductionParams(int32_t inputSrc, int32_t sig_fmt, int32_t trans_fmt, int32_t nr_mode, int32_t param_type, int32_t val) {
    return mSysControl->factorySetNoiseReductionParams(inputSrc, sig_fmt, trans_fmt, nr_mode, param_type, val);
}

Return<int32_t> SystemControlHal::factoryGetNoiseReductionParams(int32_t inputSrc, int32_t sig_fmt, int32_t trans_fmt, int32_t nr_mode, int32_t param_type) {
    return mSysControl->factoryGetNoiseReductionParams(inputSrc, sig_fmt, trans_fmt, nr_mode, param_type);
}

Return<int32_t> SystemControlHal::factorySetCTIParams(int32_t inputSrc, int32_t sig_fmt, int32_t trans_fmt, int32_t param_type, int32_t val) {
    return mSysControl->factorySetCTIParams(inputSrc, sig_fmt, trans_fmt, param_type, val);
}

Return<int32_t> SystemControlHal::factoryGetCTIParams(int32_t inputSrc, int32_t sig_fmt, int32_t trans_fmt, int32_t param_type) {
    return mSysControl->factoryGetCTIParams(inputSrc, sig_fmt, trans_fmt, param_type);
}

Return<int32_t> SystemControlHal::factorySetDecodeLumaParams(int32_t inputSrc, int32_t sig_fmt, int32_t trans_fmt, int32_t param_type, int32_t val) {
    return mSysControl->factorySetDecodeLumaParams(inputSrc, sig_fmt, trans_fmt, param_type, val);
}

Return<int32_t> SystemControlHal::factoryGetDecodeLumaParams(int32_t inputSrc, int32_t sig_fmt, int32_t trans_fmt, int32_t param_type) {
    return mSysControl->factoryGetDecodeLumaParams(inputSrc, sig_fmt, trans_fmt, param_type);
}

Return<int32_t> SystemControlHal::factorySetSharpnessParams(int32_t inputSrc, int32_t sig_fmt, int32_t trans_fmt, int32_t isHD, int32_t param_type, int32_t val) {
    return mSysControl->factorySetSharpnessParams(inputSrc, sig_fmt, trans_fmt, isHD, param_type, val);
}

Return<int32_t> SystemControlHal::factoryGetSharpnessParams(int32_t inputSrc, int32_t sig_fmt, int32_t trans_fmt, int32_t isHD,int32_t param_type) {
    return mSysControl->factoryGetSharpnessParams(inputSrc, sig_fmt, trans_fmt, isHD, param_type);
}

Return<void> SystemControlHal::getChipVersionInfo(getChipVersionInfo_cb _hidl_cb) {
    std::string chipversion;
    mSysControl->getChipVersionInfo(chipversion);

    if (ENABLE_LOG_PRINT) ALOGI("getChipVersionInfo :%s", chipversion.c_str());
    _hidl_cb(Result::OK, chipversion);
    return Void();
}

Return<void> SystemControlHal::getPQDatabaseInfo(int32_t dataBaseName, getPQDatabaseInfo_cb _hidl_cb) {
    PQDatabaseInfo Info;
    tvpq_databaseinfo_t pq_databaseinfo = mSysControl->getPQDatabaseInfo(dataBaseName);
    Info.ToolVersion    = pq_databaseinfo.ToolVersion;
    Info.ProjectVersion = pq_databaseinfo.ProjectVersion;
    Info.GenerateTime   = pq_databaseinfo.GenerateTime;
    _hidl_cb(Info);
    return Void();
}

Return<int32_t> SystemControlHal::setDtvKitSourceEnable(int32_t isEnable) {
    return mSysControl->setDtvKitSourceEnable(isEnable);
}

Return<Result> SystemControlHal::setAipqEnable(bool on) {
    if (mSysControl->setAipqEnable(on)) {
        return Result::OK;
    }
    return Result::FAIL;
}

Return<Result> SystemControlHal::getAipqEnable() {
    if (mSysControl->getAipqEnable()) {
        return Result::OK;
    }
    return Result::FAIL;
}

Return<Result> SystemControlHal::hasAipqFunc() {
    if (mSysControl->hasAipqFunc()) {
        return Result::OK;
    }
    return Result::FAIL;
}

Return<int32_t> SystemControlHal::setColorGamutMode(int32_t isEnable, int32_t is_save) {
    return mSysControl->setColorGamutMode(isEnable, is_save);
}

Return<int32_t> SystemControlHal::getColorGamutMode(void) {
    return mSysControl->getColorGamutMode();
}
//PQ end
//static frame
Return<int32_t> SystemControlHal::setStaticFrameEnable(int enable, int isSave)
{
    return mSysControl->setStaticFrameEnable(enable, isSave);
}

Return<int32_t> SystemControlHal::getStaticFrameEnable()
{
    return mSysControl->getStaticFrameEnable();
}

//screen color
Return<int32_t> SystemControlHal::setScreenColorForSignalChange(int screenColor, int isSave)
{
    return mSysControl->setScreenColorForSignalChange(screenColor, isSave);
}

Return<int32_t> SystemControlHal::getScreenColorForSignalChange()
{
    return mSysControl->getScreenColorForSignalChange();
}

Return<int32_t> SystemControlHal::setVideoScreenColor(int color)
{
    return mSysControl->setVideoScreenColor(color);
}

//FBC
Return<int32_t> SystemControlHal::StartUpgradeFBC(const hidl_string& fileName, int32_t mode, int32_t upgrade_blk_size)
{
    return mSysControl->StartUpgradeFBC(fileName, mode, upgrade_blk_size);
}

Return<int32_t> SystemControlHal::UpdateFBCUpgradeStatus(int32_t state, int32_t param)
{
    return mSysControl->UpdateFBCUpgradeStatus(state, param);
}

Return<int32_t> SystemControlHal::setAudioParam(int32_t param1, int32_t param2, int32_t param3, int32_t param4) {
    return mSysControl->setAudioParam(param1, param2, param3, param4);
}

void SystemControlHal::handleServiceDeath(uint32_t cookie) {
    AutoMutex _l( mLock );
    if (mClients[cookie] != nullptr) {
        mClients[cookie]->unlinkToDeath(mDeathRecipient);
        mClients[cookie].clear();
    }
}

Return<void> SystemControlHal::debug(const hidl_handle& handle, const hidl_vec<hidl_string>& options) {
    if (handle != nullptr && handle->numFds >= 1) {
        int fd = handle->data[0];

        std::vector<std::string> args;
        for (size_t i = 0; i < options.size(); i++) {
            args.push_back(options[i]);
        }
        mSysControl->dump(fd, args);
        fsync(fd);
    }
    return Void();
}

Return<Result> SystemControlHal::frameRateDisplay(bool on, const ISystemControl::Rect& rect) {
    ALOGI("SystemControlHal frameRateDisplay %d,[%d %d %d %d]",on,rect.left, rect.top, rect.right, rect.bottom);
    droidlogic::Rect frect(rect.left, rect.top, rect.right, rect.bottom);
    /*here always on since is for testing*/
    return mSysControl->frameRateDisplay(on)?Result::OK:Result::FAIL;
    /*if (!on ||frect.inscreen()) {
        return mSysControl->frameRateDisplay(on)?Result::OK:Result::FAIL;
    } else {
        return Result::OK;
    }*/
}

SystemControlHal::DeathRecipient::DeathRecipient(sp<SystemControlHal> sch)
        : mSystemControlHal(sch) {}

void SystemControlHal::DeathRecipient::serviceDied(
        uint64_t cookie,
        const wp<::android::hidl::base::V1_0::IBase>& /*who*/) {
    ALOGE("systemcontrol daemon client died cookie:%d", (int)cookie);

    uint32_t type = static_cast<uint32_t>(cookie);
    mSystemControlHal->handleServiceDeath(type);
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace systemcontrol
}  // namespace hardware
}  // namespace android
}  // namespace vendor
