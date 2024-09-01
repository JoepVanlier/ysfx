// Copyright 2021 Jean Pierre Cimalando
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
//

#include "processor.h"
#include "editor.h"
#include "parameter.h"
#include "info.h"
#include "utility/audio_processor_suspender.h"
#include "utility/rt_semaphore.h"
#include "utility/sync_bitset.hpp"
#include "ysfx.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

struct YsfxProcessor::Impl : public juce::AudioProcessorListener {
    YsfxProcessor *m_self = nullptr;
    ysfx_u m_fx;
    ysfx_time_info_t m_timeInfo{};
    int m_sliderParamOffset = 0;
    ysfx::sync_bitset64 m_sliderParametersChanged[ysfx_max_slider_groups];
    YsfxInfo::Ptr m_info{new YsfxInfo};
    YsfxCurrentPresetInfo::Ptr m_currentPresetInfo{new YsfxCurrentPresetInfo};
    ysfx_bank_shared m_bank{nullptr};

    double m_sample_rate{44100.0};
    uint32_t m_block_size{256};

    //==========================================================================
    void processBlockGenerically(const void *inputs[], void *outputs[], uint32_t numIns, uint32_t numOuts, uint32_t numFrames, uint32_t processBits, juce::MidiBuffer &midiMessages);
    void processMidiInput(juce::MidiBuffer &midi);
    void processMidiOutput(juce::MidiBuffer &midi);
    void processSliderChanges();
    void processLatency();
    void updateTimeInfo();
    void syncParametersToSliders();
    void syncSlidersToParameters(bool notify);
    void syncParameterToSlider(int index);
    void syncSliderToParameter(int index, bool notify);
    static YsfxInfo::Ptr createNewFx(juce::CharPointer_UTF8 filePath, ysfx_state_t *initialState);
    void installNewFx(YsfxInfo::Ptr info, ysfx_bank_shared bank);
    ysfx_bank_shared loadDefaultBank(YsfxInfo::Ptr info);
    void loadNewPreset(const ysfx_preset_t &preset);

    //==========================================================================
    struct LoadRequest : public std::enable_shared_from_this<LoadRequest> {
        juce::String filePath;
        ysfx_state_u initialState;
        volatile bool completion = false;
        std::mutex completionMutex;
        std::condition_variable completionVariable;
        using Ptr = std::shared_ptr<LoadRequest>;
    };

    struct PresetRequest : public std::enable_shared_from_this<PresetRequest> {
        YsfxInfo::Ptr info;
        ysfx_bank_shared bank;
        uint32_t index = 0;
        volatile bool completion = false;
        std::mutex completionMutex;
        std::condition_variable completionVariable;
        using Ptr = std::shared_ptr<PresetRequest>;
    };

    LoadRequest::Ptr m_loadRequest;
    PresetRequest::Ptr m_presetRequest;
    ysfx::sync_bitset64 m_sliderParamsToNotify[ysfx_max_slider_groups];
    ysfx::sync_bitset64 m_sliderParamsTouching[ysfx_max_slider_groups];

    //==========================================================================
    class SliderNotificationUpdater : public juce::AsyncUpdater {
    public:
        explicit SliderNotificationUpdater(Impl *impl) : m_impl{impl} {}
        void addSlidersToNotify(uint64_t mask, int group) { m_sliderMask[group].fetch_or(mask); }
        void updateTouch(uint64_t mask, int group) { m_touchMask[group].exchange(mask); };

    protected:
        void handleAsyncUpdate() override;

    private:
        Impl *m_impl = nullptr;
        ysfx::sync_bitset64 m_sliderMask[ysfx_max_slider_groups];

        ysfx::sync_bitset64 m_touchMask[ysfx_max_slider_groups];
        uint64_t m_previousTouchMask[ysfx_max_slider_groups]{0};
    };

    std::unique_ptr<SliderNotificationUpdater> m_sliderNotificationUpdater;

    //==========================================================================
    class Background {
    public:
        explicit Background(Impl *impl);
        void shutdown();
        void wakeUp();
    private:
        void run();
        void processSliderNotifications(uint64_t sliderMask, uint64_t touchMask, int group);
        void processLoadRequest(LoadRequest &req);
        void processPresetRequest(PresetRequest &req);
        Impl *m_impl = nullptr;
        RTSemaphore m_sema;
        std::atomic<bool> m_running{};
        std::thread m_thread;
    };

    std::unique_ptr<Background> m_background;

    //==========================================================================
    void audioProcessorParameterChanged(AudioProcessor *processor, int parameterIndex, float newValue) override;
    void audioProcessorChanged(AudioProcessor *processor, const ChangeDetails &details) override;

    //==========================================================================
    std::atomic<RetryState> m_failedLoad{RetryState::ok};
    juce::CriticalSection m_loadLock;
    juce::String m_lastLoadPath{""};
    ysfx_state_u m_failedLoadState{nullptr};  // Holds the state of a failed load
};

//==============================================================================
YsfxProcessor::YsfxProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    m_impl(new Impl)
{
    m_impl->m_self = this;

    ysfx_config_u config{ysfx_config_new()};
    ysfx_register_builtin_audio_formats(config.get());

    ysfx_t *fx = ysfx_new(config.get());
    m_impl->m_fx.reset(fx);
    YsfxInfo::Ptr info{new YsfxInfo};
    info->effect.reset(fx);
    ysfx_add_ref(fx);  // why?
    std::atomic_store(&m_impl->m_info, info);

    ///
    ysfx_time_info_t &timeInfo = m_impl->m_timeInfo;
    timeInfo.tempo = 120;
    timeInfo.playback_state = ysfx_playback_paused;
    timeInfo.time_position = 0;
    timeInfo.beat_position = 0;
    timeInfo.time_signature[0] = 4;
    timeInfo.time_signature[1] = 4;

    ///
    m_impl->m_sliderParamOffset = getParameters().size();
    for (int i = 0; i < ysfx_max_sliders; ++i)
        addParameter(new YsfxParameter(fx, i));

    ///
    m_impl->m_sliderNotificationUpdater.reset(new Impl::SliderNotificationUpdater{m_impl.get()});

    ///
    m_impl->m_background.reset(new Impl::Background(m_impl.get()));

    ///
    addListener(m_impl.get());
}

juce::String YsfxProcessor::lastLoadPath()
{
    const juce::ScopedLock sl(m_impl->m_loadLock);
    return m_impl->m_lastLoadPath;
}

RetryState YsfxProcessor::retryLoad()
{
    RetryState state = m_impl->m_failedLoad.load();

    if (state == RetryState::mustRetry) {
        m_impl->m_failedLoad.store(RetryState::retrying);
    }

    return state;
}

YsfxProcessor::~YsfxProcessor()
{
    removeListener(m_impl.get());
    m_impl->m_background->shutdown();
}

YsfxParameter *YsfxProcessor::getYsfxParameter(int sliderIndex)
{
    if (sliderIndex < 0 || sliderIndex >= ysfx_max_sliders)
        return nullptr;

    int paramIndex = sliderIndex + m_impl->m_sliderParamOffset;
    return static_cast<YsfxParameter *>(getParameters()[paramIndex]);
}

void YsfxProcessor::loadJsfxFile(const juce::String &filePath, ysfx_state_t *initialState, bool async)
{
    Impl::LoadRequest::Ptr loadRequest{new Impl::LoadRequest};
    loadRequest->filePath = filePath;

    if (m_impl->m_failedLoad.load() == RetryState::retrying) {
        {
            const juce::ScopedLock sl(m_impl->m_loadLock);
            loadRequest->initialState.reset(ysfx_state_dup(m_impl->m_failedLoadState.get()));
        }
    } else {
        loadRequest->initialState.reset(ysfx_state_dup(initialState));
    };
    std::atomic_store(&m_impl->m_loadRequest, loadRequest);
    m_impl->m_background->wakeUp();
    if (!async) {
        std::unique_lock<std::mutex> lock(loadRequest->completionMutex);
        loadRequest->completionVariable.wait(lock, [&]() { return loadRequest->completion; });
    }
}

void YsfxProcessor::loadJsfxPreset(YsfxInfo::Ptr info, ysfx_bank_shared bank, uint32_t index, bool async)
{
    Impl::PresetRequest::Ptr presetRequest{new Impl::PresetRequest};
    presetRequest->info = info;
    presetRequest->bank = bank;
    presetRequest->index = index;
    std::atomic_store(&m_impl->m_presetRequest, presetRequest);
    m_impl->m_background->wakeUp();
    if (!async) {
        std::unique_lock<std::mutex> lock(presetRequest->completionMutex);
        presetRequest->completionVariable.wait(lock, [&]() { return presetRequest->completion; });
    }
}

static juce::File getCustomBankLocation(ysfx_t *fx) {
    std::string pathString{ysfx_get_bank_path(fx)};
    if (pathString.empty()) {
        // No bank exists yet
        pathString = std::string{ysfx_get_file_path(fx)} + ".rpl";
    }
    juce::File customBankPath{pathString};
    customBankPath = juce::File(customBankPath.getParentDirectory().getFullPathName()).getChildFile(customBankPath.getFileNameWithoutExtension() + "-ysfx.rpl");
    return customBankPath;
}

bool YsfxProcessor::presetExists(const char* presetName)
{
    auto sourceBank = m_impl->m_bank;
    return ysfx_preset_exists(sourceBank.get(), presetName) > 0;
}

void YsfxProcessor::saveCurrentPreset(const char* preset_name)
{
    ysfx_t *fx = m_impl->m_fx.get();
    if (!fx) return;

    // Make a backup copy before we write in case there's trouble
    juce::File bankLocation = getCustomBankLocation(fx);
    juce::File bankCopy(bankLocation.getFullPathName() + "-bak");
    bankLocation.copyFileTo(bankCopy);

    ysfx_bank_shared bank = m_impl->m_bank;  // Make sure we keep it alive while we are operating on it

    ysfx_bank_shared newBank;
    if (!bank) {
        ysfx_bank_u emptyBank{ysfx_create_empty_bank(m_impl->m_info->m_name.toUTF8())};
        newBank.reset(ysfx_add_preset_to_bank(emptyBank.get(), preset_name, ysfx_save_state(fx)));
    } else {
        newBank.reset(ysfx_add_preset_to_bank(bank.get(), preset_name, ysfx_save_state(fx)));
    }

    ysfx_save_bank(bankLocation.getFullPathName().toStdString().c_str(), newBank.get());
    loadJsfxPreset(m_impl->m_info, newBank, ysfx_preset_exists(newBank.get(), preset_name) - 1, true);
}

YsfxInfo::Ptr YsfxProcessor::getCurrentInfo()
{
    return std::atomic_load(&m_impl->m_info);
}

YsfxCurrentPresetInfo::Ptr YsfxProcessor::getCurrentPresetInfo()
{
    return std::atomic_load(&m_impl->m_currentPresetInfo);
}

ysfx_bank_shared YsfxProcessor::getCurrentBank()
{
    return std::atomic_load(&m_impl->m_bank);
}

//==============================================================================
void YsfxProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    AudioProcessorSuspender sus(*this);
    sus.lockCallbacks();

    ysfx_t *fx = m_impl->m_fx.get();
    m_impl->m_sample_rate = sampleRate;
    m_impl->m_block_size = static_cast<uint32_t>(samplesPerBlock);

    ysfx_set_sample_rate(fx, sampleRate);
    ysfx_set_block_size(fx, (uint32_t)samplesPerBlock);

    ysfx_init(fx);

    m_impl->processLatency();
}

void YsfxProcessor::releaseResources()
{
}

void YsfxProcessor::Impl::processBlockGenerically(const void *inputs[], void *outputs[], uint32_t numIns, uint32_t numOuts, uint32_t numFrames, uint32_t processBits, juce::MidiBuffer &midiMessages)
{
    ysfx_t *fx = m_fx.get();

    for (uint8_t group = 0; group < ysfx_max_slider_groups; group++) {
        uint64_t sliderParametersChanged = m_sliderParametersChanged[group].exchange(0);
    
        if (sliderParametersChanged) {
            for (int slider = 0; slider < ysfx_max_sliders; ++slider) {
                if (sliderParametersChanged & ysfx_slider_mask((uint32_t) slider, group)) {
                    syncParameterToSlider(slider);
                }
            }
        }
    }

    updateTimeInfo();
    ysfx_set_time_info(fx, &m_timeInfo);

    processMidiInput(midiMessages);

    switch (processBits) {
    case 32:
        ysfx_process_float(fx, (const float **)inputs, (float **)outputs, numIns, numOuts, numFrames);
        break;
    case 64:
        ysfx_process_double(fx, (const double **)inputs, (double **)outputs, numIns, numOuts, numFrames);
        break;
    default:
        jassertfalse;
    }

    processMidiOutput(midiMessages);
    processSliderChanges();
    processLatency();
}

void YsfxProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{
    m_impl->processBlockGenerically(
        (const void **)buffer.getArrayOfReadPointers(),
        (void **)buffer.getArrayOfWritePointers(),
        (uint32_t)getTotalNumInputChannels(),
        (uint32_t)getTotalNumOutputChannels(),
        (uint32_t)buffer.getNumSamples(),
        8 * sizeof(buffer.getSample(0, 0)),
        midiMessages);
}

void YsfxProcessor::processBlock(juce::AudioBuffer<double> &buffer, juce::MidiBuffer &midiMessages)
{
    m_impl->processBlockGenerically(
        (const void **)buffer.getArrayOfReadPointers(),
        (void **)buffer.getArrayOfWritePointers(),
        (uint32_t)getTotalNumInputChannels(),
        (uint32_t)getTotalNumOutputChannels(),
        (uint32_t)buffer.getNumSamples(),
        8 * sizeof(buffer.getSample(0, 0)),
        midiMessages);
}

bool YsfxProcessor::supportsDoublePrecisionProcessing() const
{
    return true;
}

//==============================================================================
juce::AudioProcessorEditor *YsfxProcessor::createEditor()
{
    return new YsfxEditor(*this);
}

bool YsfxProcessor::hasEditor() const
{
    return true;
}

//==============================================================================
const juce::String YsfxProcessor::getName() const
{
    return JucePlugin_Name;
}

bool YsfxProcessor::acceptsMidi() const
{
    return true;
}

bool YsfxProcessor::producesMidi() const
{
    return true;
}

bool YsfxProcessor::isMidiEffect() const
{
    return false;
}

double YsfxProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

//==============================================================================
int YsfxProcessor::getNumPrograms()
{
    return 1;
}

int YsfxProcessor::getCurrentProgram()
{
    return 0;
}

void YsfxProcessor::setCurrentProgram(int index)
{
    (void)index;
}

const juce::String YsfxProcessor::getProgramName(int index)
{
    (void)index;
    return {};
}

void YsfxProcessor::changeProgramName(int index, const juce::String &newName)
{
    (void)index;
    (void)newName;
}

//==============================================================================
void YsfxProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    juce::File path;
    ysfx_state_u state;

    {
        AudioProcessorSuspender sus(*this);
        sus.lockCallbacks();
        ysfx_t *fx = m_impl->m_fx.get();
        path = juce::CharPointer_UTF8(ysfx_get_file_path(fx));
        state.reset(ysfx_save_state(fx));
    }

    juce::ValueTree root("ysfx");
    root.setProperty("version", 1, nullptr);
    root.setProperty("path", path.getFullPathName(), nullptr);

    if (state) {
        juce::ValueTree stateTree("state");

        juce::ValueTree sliderTree("sliders");
        for (uint32_t i = 0; i < state->slider_count; ++i)
            sliderTree.setProperty(juce::String(state->sliders[i].index), state->sliders[i].value, nullptr);
        stateTree.addChild(sliderTree, -1, nullptr);

        stateTree.setProperty("data", juce::Base64::toBase64(state->data, state->data_size), nullptr);

        root.addChild(stateTree, -1, nullptr);
    }

    juce::MemoryOutputStream stream(destData, false);
    root.writeToStream(stream);
}

void YsfxProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    juce::File path;

    juce::MemoryInputStream stream(data, (size_t)sizeInBytes, false);
    juce::ValueTree root = juce::ValueTree::readFromStream(stream);

    if (root.getType().getCharPointer().compare(juce::CharPointer_UTF8("ysfx")) != 0)
        return;
    if ((int)root.getProperty("version") != 1)
        return;

    path = root.getProperty("path").toString();

    juce::ValueTree stateTree = root.getChildWithName("state");
    if (stateTree != juce::ValueTree{}) {
        ysfx_state_t state{};
        juce::Array<ysfx_state_slider_t> sliders;
        juce::MemoryBlock dataBlock;

        {
            juce::ValueTree sliderTree = stateTree.getChildWithName("sliders");
            for (uint32_t i = 0; i < ysfx_max_sliders; ++i) {
                if (const juce::var *v = sliderTree.getPropertyPointer(juce::String(i))) {
                    ysfx_state_slider_t item{};
                    item.index = i;
                    item.value = (double)*v;
                    sliders.add(item);
                }
            }
        }
        {
            juce::MemoryOutputStream base64Result(dataBlock, false);
            juce::Base64::convertFromBase64(base64Result, stateTree.getProperty("data").toString());
        }

        state.sliders = sliders.data();
        state.slider_count = (uint32_t)sliders.size();
        state.data = (uint8_t *)dataBlock.getData();
        state.data_size = dataBlock.getSize();
        loadJsfxFile(path.getFullPathName(), &state, false);
    }
    else {
        loadJsfxFile(path.getFullPathName(), nullptr, false);
    }
}

//==============================================================================
bool YsfxProcessor::isBusesLayoutSupported(const BusesLayout &layout) const
{
    int numInputs = layout.getMainInputChannels();
    int numOutputs = layout.getMainOutputChannels();

    if (numInputs > ysfx_max_channels || numOutputs > ysfx_max_channels)
        return false;

    return true;
}

//==============================================================================
void YsfxProcessor::Impl::processMidiInput(juce::MidiBuffer &midi)
{
    ysfx_t *fx = m_fx.get();

    for (juce::MidiMessageMetadata md : midi) {
        ysfx_midi_event_t event{};
        event.offset = (uint32_t)md.samplePosition;
        event.size = (uint32_t)md.numBytes;
        event.data = md.data;
        ysfx_send_midi(fx, &event);
    }
}

void YsfxProcessor::Impl::processMidiOutput(juce::MidiBuffer &midi)
{
    midi.clear();

    ysfx_midi_event_t event;
    ysfx_t *fx = m_fx.get();
    while (ysfx_receive_midi(fx, &event))
        midi.addEvent(event.data, (int)event.size, (int)event.offset);
}

void YsfxProcessor::Impl::processSliderChanges()
{
    ysfx_t *fx = m_fx.get();

    for (int i = 0; i < ysfx_max_sliders; ++i) {
        YsfxParameter *param = m_self->getYsfxParameter(i);
        if (param->existsAsSlider()) {
            float normValue = param->convertFromYsfxValue(ysfx_slider_get_value(fx, (uint32_t)i));
            if (param->getValue() != normValue) {
                param->setValueNoNotify(normValue);  // This should not trigger @slider
            }
        }
    }

    bool notify = false;
    for (uint8_t i = 0; i < ysfx_max_slider_groups; ++i) {
        uint64_t automated = ysfx_fetch_slider_automations(fx, i);
        m_sliderParamsTouching[i].exchange(ysfx_fetch_slider_touches(fx, i));
        m_sliderParamsToNotify[i].fetch_or(automated);

        notify = automated ? true : notify;
    };

    // this will sync parameters later (on message thread)
    if (notify) m_background->wakeUp();

    //TODO: visibility changes
}

void YsfxProcessor::Impl::processLatency()
{
    ysfx_t *fx = m_fx.get();
    ysfx_real latency = ysfx_get_pdc_delay(fx);

    // NOTE: ignore pdc_bot_ch and pdc_top_ch

    int samples = juce::roundToInt(latency);
    m_self->setLatencySamples(samples);
}

void YsfxProcessor::Impl::updateTimeInfo()
{
    juce::AudioPlayHead *playHead = m_self->getPlayHead();

    juce::Optional<juce::AudioPlayHead::PositionInfo> cpi = playHead->getPosition();
    if (!cpi)
        return;

    if (cpi->getIsRecording())
        m_timeInfo.playback_state = ysfx_playback_recording;
    else if (cpi->getIsPlaying())
        m_timeInfo.playback_state = ysfx_playback_playing;
    else
        m_timeInfo.playback_state = ysfx_playback_paused;

    if (juce::Optional<double> bpm = cpi->getBpm())
        m_timeInfo.tempo = *bpm;
    if (juce::Optional<double> timeInSeconds = cpi->getTimeInSeconds())
        m_timeInfo.time_position = *timeInSeconds;
    if (juce::Optional<double> ppqPosition = cpi->getPpqPosition())
        m_timeInfo.beat_position = *ppqPosition;
    if (juce::Optional<juce::AudioPlayHead::TimeSignature> timeSignature = cpi->getTimeSignature()) {
        m_timeInfo.time_signature[0] = (uint32_t)timeSignature->numerator;
        m_timeInfo.time_signature[1] = (uint32_t)timeSignature->denominator;
    }
}

void YsfxProcessor::Impl::syncParametersToSliders()
{
    for (int i = 0; i < ysfx_max_sliders; ++i)
        syncParameterToSlider(i);
}

void YsfxProcessor::Impl::syncSlidersToParameters(bool notify)
{
    for (int i = 0; i < ysfx_max_sliders; ++i)
        syncSliderToParameter(i, notify);
}

void YsfxProcessor::Impl::syncParameterToSlider(int index)
{
    if (index < 0 || index >= ysfx_max_sliders)
        return;

    YsfxParameter *param = m_self->getYsfxParameter(index);

    if (param->existsAsSlider()) {
        ysfx_real actualValue = param->convertToYsfxValue(param->getValue());

        // NOTE: Unfortunately, things have to map to 0-1 so you lose some precision 
        // coming back (and can't rely on integer floats being exact anymore).
        ysfx_real rounded = juce::roundToInt(actualValue);
        if (std::abs(rounded - actualValue) < 0.00001) {
            actualValue = rounded > -0.1 ? abs(rounded) : rounded;
        }

        ysfx_slider_set_value(m_fx.get(), (uint32_t)index, actualValue, param->wasUpdatedByHost());
    }
}

void YsfxProcessor::Impl::syncSliderToParameter(int index, bool notify)
{
    if (notify) {
        jassert(
            juce::MessageManager::getInstanceWithoutCreating() &&
            juce::MessageManager::getInstanceWithoutCreating()->isThisTheMessageThread());
    }

    if (index < 0 || index >= ysfx_max_sliders)
        return;

    YsfxParameter *param = m_self->getYsfxParameter(index);
    if (param->existsAsSlider()) {
        float normValue = param->convertFromYsfxValue(ysfx_slider_get_value(m_fx.get(), (uint32_t)index));
        
        if (notify)
            param->setValueNotifyingHost(normValue);
        else {
            param->setValue(normValue);

            uint8_t group = ysfx_fetch_slider_group_index((uint32_t) index);
            m_sliderParamsToNotify[group].fetch_or(ysfx_slider_mask((uint32_t) index, group));
        }
    }
}

YsfxInfo::Ptr YsfxProcessor::Impl::createNewFx(juce::CharPointer_UTF8 filePath, ysfx_state_t *initialState)
{
    YsfxInfo::Ptr info{new YsfxInfo};

    info->timeStamp = juce::Time::getCurrentTime();

    ///
    ysfx_config_u config{ysfx_config_new()};
    ysfx_register_builtin_audio_formats(config.get());
    ysfx_guess_file_roots(config.get(), filePath);

    ///
    auto logfn = [](intptr_t userdata, ysfx_log_level level, const char *message) {
        YsfxInfo &data = *(YsfxInfo *)userdata;
        if (level == ysfx_log_error)
            data.errors.add(juce::CharPointer_UTF8(message));
        else if (level == ysfx_log_warning)
            data.warnings.add(juce::CharPointer_UTF8(message));
    };

    ysfx_set_log_reporter(config.get(), +logfn);
    ysfx_set_user_data(config.get(), (intptr_t)info.get());

    ///
    ysfx_t *fx = ysfx_new(config.get());
    info->effect.reset(fx);

    uint32_t loadopts = 0;
    uint32_t compileopts = 0;
    ysfx_load_file(fx, filePath, loadopts);
    ysfx_compile(fx, compileopts);

    info->m_name = juce::File{filePath}.getFileNameWithoutExtension();

    if (initialState)
        ysfx_load_state(fx, initialState);

    return info;
}

ysfx_bank_shared YsfxProcessor::Impl::loadDefaultBank(YsfxInfo::Ptr info)
{
    // Check if we have a customized bank
    const char *bankpath = ysfx_get_bank_path(info->effect.get());
    juce::File customBankPath = getCustomBankLocation(info->effect.get());

    ysfx_bank_shared bank;
    if (customBankPath.existsAsFile()) {
        bank.reset(ysfx_load_bank(customBankPath.getFullPathName().toStdString().c_str()));
    } else {
        bank.reset(ysfx_load_bank(bankpath));
    }

    return bank;
}

void YsfxProcessor::Impl::installNewFx(YsfxInfo::Ptr info, ysfx_bank_shared bank)
{
    AudioProcessorSuspender sus{*m_self};
    sus.lockCallbacks();

    ysfx_t *fx = info->effect.get();
    m_fx.reset(fx);
    ysfx_add_ref(fx);

    ysfx_set_sample_rate(fx, m_sample_rate);
    ysfx_set_block_size(fx, m_block_size);
    ysfx_init(fx);

    for (uint32_t i = 0; i < ysfx_max_sliders; ++i) {
        YsfxParameter *param = m_self->getYsfxParameter((int)i);
        param->setEffect(fx);
    }

    bool notify = false;
    syncSlidersToParameters(notify);

    m_self->updateHostDisplay();

    // notify parameters later, on the message thread
    for (int i=0; i < ysfx_max_slider_groups; i++) {
        m_sliderParamsToNotify[i].store(~(uint64_t)0);
        m_sliderParamsTouching[i].store((uint64_t)0);
    }

    YsfxCurrentPresetInfo::Ptr presetInfo{new YsfxCurrentPresetInfo()};
    std::atomic_store(&m_currentPresetInfo, presetInfo);
    std::atomic_store(&m_bank, bank);
    std::atomic_store(&m_info, info);
    m_background->wakeUp();
}

void YsfxProcessor::Impl::loadNewPreset(const ysfx_preset_t &preset)
{
    AudioProcessorSuspender sus{*m_self};
    sus.lockCallbacks();

    ysfx_t *fx = m_fx.get();
    ysfx_load_state(fx, preset.state);

    bool notify = false;
    syncSlidersToParameters(notify);

    YsfxCurrentPresetInfo::Ptr presetInfo{new YsfxCurrentPresetInfo()};
    presetInfo->m_lastChosenPreset = juce::String::fromUTF8(preset.name);

    // notify parameters later, on the message thread
    for (int i=0; i < ysfx_max_slider_groups; i++) {
        m_sliderParamsToNotify[i].store(~(uint64_t)0);
        m_sliderParamsTouching[i].store((uint64_t)0);
    }

    std::atomic_store(&m_currentPresetInfo, presetInfo);
    m_background->wakeUp();
}

//==============================================================================
void YsfxProcessor::Impl::SliderNotificationUpdater::handleAsyncUpdate()
{
    int group_offset = 0;
    for (uint8_t group = 0; group < ysfx_max_slider_groups; group++) {
        uint64_t sliderMask = m_sliderMask[group].exchange(0);
        uint64_t currentTouchMask = m_touchMask[group].load();

        uint64_t startMask = ~(m_previousTouchMask[group]) & currentTouchMask;
        uint64_t endMask = m_previousTouchMask[group] & ~currentTouchMask;
        m_previousTouchMask[group] = currentTouchMask;

        for (int i = 0; i < 64; ++i) {
            if (startMask & (uint64_t{1} << i)) {
                YsfxParameter *param = m_impl->m_self->getYsfxParameter(i + group_offset);
                param->beginChangeGesture();
            }
        }
        for (int i = 0; i < 64; ++i) {
            if (sliderMask & (uint64_t{1} << i)) {
                YsfxParameter *param = m_impl->m_self->getYsfxParameter(i + group_offset);
                param->sendValueChangedMessageToListeners(param->getValue());
            }
        }
        for (int i = 0; i < 64; ++i) {
            if (endMask & (uint64_t{1} << i)) {
                YsfxParameter *param = m_impl->m_self->getYsfxParameter(i + group_offset);
                param->endChangeGesture();
            }
        }

        group_offset += 64;
    }
}

//==============================================================================
YsfxProcessor::Impl::Background::Background(Impl *impl)
    : m_impl(impl)
{
    m_running.store(true, std::memory_order_relaxed);
    m_thread = std::thread([this]() { run(); });
}

void YsfxProcessor::Impl::Background::shutdown()
{
    m_running.store(false, std::memory_order_relaxed);
    m_sema.post();
    m_thread.join();
}

void YsfxProcessor::Impl::Background::wakeUp()
{
    m_sema.post();
}

void YsfxProcessor::Impl::Background::run()
{
    while (m_sema.wait(), m_running.load(std::memory_order_relaxed)) {
        for (uint8_t group = 0; group < ysfx_max_slider_groups; group++) {
            if (uint64_t sliderMask = m_impl->m_sliderParamsToNotify[group].exchange(0)) {
                uint64_t touchMask = m_impl->m_sliderParamsTouching[group].load();
                processSliderNotifications(sliderMask, touchMask, group);
            }
        }
        if (LoadRequest::Ptr loadRequest = std::atomic_exchange(&m_impl->m_loadRequest, LoadRequest::Ptr{}))
            processLoadRequest(*loadRequest);
        if (PresetRequest::Ptr presetRequest = std::atomic_exchange(&m_impl->m_presetRequest, PresetRequest::Ptr{}))
            processPresetRequest(*presetRequest);
    }
}

void YsfxProcessor::Impl::Background::processSliderNotifications(uint64_t sliderMask, uint64_t touchMask, int group)
{
    Impl *impl = this->m_impl;
    Impl::SliderNotificationUpdater *updater = impl->m_sliderNotificationUpdater.get();

    updater->addSlidersToNotify(sliderMask, group);
    updater->updateTouch(touchMask, group);
    updater->triggerAsyncUpdate();
}

void YsfxProcessor::Impl::Background::processLoadRequest(LoadRequest &req)
{
    YsfxInfo::Ptr info = createNewFx(req.filePath.toUTF8(), req.initialState.get());
    ysfx_bank_shared bank = m_impl->loadDefaultBank(info);
    m_impl->installNewFx(info, bank);

    {
        const juce::ScopedLock sl(m_impl->m_loadLock);

        m_impl->m_lastLoadPath = req.filePath;
        if (!ysfx_is_compiled(m_impl->m_fx.get())) {
            if (req.initialState) {
                m_impl->m_failedLoadState.reset(ysfx_state_dup(req.initialState.get()));
                m_impl->m_failedLoad.store(RetryState::mustRetry);  // aww
            }
        } else {
            // Successful compile this time. We can let it go.
            m_impl->m_failedLoadState.reset(nullptr);
            m_impl->m_failedLoad.store(RetryState::ok);
        }
    }

    std::lock_guard<std::mutex> lock(req.completionMutex);
    req.completion = true;
    req.completionVariable.notify_one();
}

void YsfxProcessor::Impl::Background::processPresetRequest(PresetRequest &req)
{
    // Verify that we still have the same plugin loaded
    if (m_impl->m_info != req.info)
        return;

    if (m_impl->m_bank != req.bank)
        std::atomic_store(&m_impl->m_bank, req.bank);

    ysfx_bank_t *bank = req.bank.get();
    if (!bank || req.index >= bank->preset_count)
        return;

    const ysfx_preset_t &preset = bank->presets[req.index];
    m_impl->loadNewPreset(preset);

    std::lock_guard<std::mutex> lock(req.completionMutex);
    req.completion = true;
    req.completionVariable.notify_one();
}

//==============================================================================
void YsfxProcessor::Impl::audioProcessorParameterChanged(AudioProcessor *processor, int parameterIndex, float newValue)
{
    (void)processor;
    (void)newValue;

    int sliderIndex = parameterIndex - m_sliderParamOffset;
    if (sliderIndex >= 0 && sliderIndex < ysfx_max_sliders) {
        uint8_t group = ysfx_fetch_slider_group_index((uint32_t) sliderIndex);
        m_sliderParametersChanged[group].fetch_or(ysfx_slider_mask((uint32_t) sliderIndex, group));
    }
}

void YsfxProcessor::Impl::audioProcessorChanged(AudioProcessor *processor, const ChangeDetails &details)
{
    (void)processor;
    (void)details;
}

//==============================================================================
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new YsfxProcessor;
}
