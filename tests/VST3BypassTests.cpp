#include <juce_audio_processors/juce_audio_processors.h>
#include <pluginterfaces/base/ipluginbase.h>
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivsteditcontroller.h>
#include <pluginterfaces/vst/ivstmessage.h>
#include <pluginterfaces/vst/ivstparameterchanges.h>
#include <pluginterfaces/vst/ivstprocesscontext.h>

#include <iostream>

namespace
{
using namespace Steinberg;
using namespace Steinberg::Vst;
juce::String pluginPath;

template <typename Interface>
Interface* query (FUnknown* object, const TUID id)
{
    void* result = nullptr;
    return object->queryInterface (id, &result) == kResultOk ? static_cast<Interface*> (result) : nullptr;
}

class ParameterQueue : public IParamValueQueue
{
public:
    ParamID id = 0;
    ParamValue value = 0.0;
    tresult PLUGIN_API queryInterface (const TUID, void** result) override { *result = nullptr; return kNoInterface; }
    uint32 PLUGIN_API addRef() override { return 1; }
    uint32 PLUGIN_API release() override { return 1; }
    ParamID PLUGIN_API getParameterId() override { return id; }
    int32 PLUGIN_API getPointCount() override { return 1; }
    tresult PLUGIN_API getPoint (int32 index, int32& offset, ParamValue& result) override
    {
        offset = 0;
        result = value;
        return index == 0 ? kResultOk : kInvalidArgument;
    }
    tresult PLUGIN_API addPoint (int32, ParamValue, int32&) override { return kNotImplemented; }
};

class ParameterChanges : public IParameterChanges
{
public:
    ParameterQueue queue;
    tresult PLUGIN_API queryInterface (const TUID, void** result) override { *result = nullptr; return kNoInterface; }
    uint32 PLUGIN_API addRef() override { return 1; }
    uint32 PLUGIN_API release() override { return 1; }
    int32 PLUGIN_API getParameterCount() override { return 1; }
    IParamValueQueue* PLUGIN_API getParameterData (int32 index) override { return index == 0 ? &queue : nullptr; }
    IParamValueQueue* PLUGIN_API addParameterData (const ParamID&, int32&) override { return nullptr; }
};

class VST3BypassTests : public juce::UnitTest
{
public:
    VST3BypassTests() : UnitTest ("Built VST3 bypass", "VST3") {}

    void runTest() override
    {
        beginTest ("The compiled VST3 exports one standard bypass parameter and accepts host bypass events");
        juce::DynamicLibrary library;
        if (! require (library.open (pluginPath), "Cannot load " + pluginPath))
            return;
        using Entry = bool (*)();
        auto init = reinterpret_cast<Entry> (library.getFunction ("InitDll"));
        auto exit = reinterpret_cast<Entry> (library.getFunction ("ExitDll"));
        if (init != nullptr)
            init();
        using FactoryFunction = IPluginFactory* (PLUGIN_API*)();
        auto factoryFunction = reinterpret_cast<FactoryFunction> (library.getFunction ("GetPluginFactory"));
        if (! require (factoryFunction != nullptr))
            return;
        auto* factory = factoryFunction();
        IComponent* component = nullptr;
        for (int32 i = 0; i < factory->countClasses(); ++i)
        {
            PClassInfo info;
            factory->getClassInfo (i, &info);
            if (std::strcmp (info.category, kVstAudioEffectClass) == 0)
            {
                void* object = nullptr;
                factory->createInstance (info.cid, IComponent_iid, &object);
                component = static_cast<IComponent*> (object);
                break;
            }
        }
        if (! require (component != nullptr))
            return;
        component->initialize (nullptr);
        TUID controllerId {};
        component->getControllerClassId (controllerId);
        void* controllerObject = nullptr;
        factory->createInstance (controllerId, IEditController_iid, &controllerObject);
        auto* controller = static_cast<IEditController*> (controllerObject);
        if (! require (controller != nullptr))
            return;
        controller->initialize (nullptr);
        auto* componentConnection = query<IConnectionPoint> (component, IConnectionPoint_iid);
        auto* controllerConnection = query<IConnectionPoint> (controller, IConnectionPoint_iid);
        componentConnection->connect (controllerConnection);
        controllerConnection->connect (componentConnection);

        ParamID bypassId = 0;
        ParamID lookaheadId = 0;
        int bypassCount = 0;
        for (int32 i = 0; i < controller->getParameterCount(); ++i)
        {
            ParameterInfo info {};
            controller->getParameterInfo (i, info);
            std::cout << "Parameter " << info.id << ", flags=" << info.flags << '\n';
            if ((info.flags & ParameterInfo::kIsBypass) != 0)
            {
                ++bypassCount;
                bypassId = info.id;
            }
            else
                lookaheadId = info.id;
        }
        expectEquals (bypassCount, 1, "The host must see exactly one kIsBypass parameter");
        controller->setParamNormalized (lookaheadId, 0.0002); // 1 ms out of 5000 ms
        auto* processor = query<IAudioProcessor> (component, IAudioProcessor_iid);
        component->activateBus (kAudio, kInput, 0, true);
        component->activateBus (kAudio, kOutput, 0, true);
        ProcessSetup setup { kRealtime, kSample32, 64, 48000.0 };
        processor->setupProcessing (setup);
        component->setActive (true);
        processor->setProcessing (true);
        expectEquals ((int) processor->getLatencySamples(), 48);

        float left[64] {};
        float right[64] {};
        float inputLeft[64] {};
        float inputRight[64] {};
        float* channels[] { left, right };
        float* inputChannels[] { inputLeft, inputRight };
        AudioBusBuffers inputBus {};
        inputBus.numChannels = 2;
        inputBus.channelBuffers32 = inputChannels;
        AudioBusBuffers outputBus {};
        outputBus.numChannels = 2;
        outputBus.channelBuffers32 = channels;
        ParameterChanges changes;
        changes.queue.id = bypassId;
        ProcessContext context {};
        context.sampleRate = 48000.0;
        context.state = ProcessContext::kPlaying;
        ProcessData data {};
        data.processMode = kRealtime;
        data.symbolicSampleSize = kSample32;
        data.numSamples = 64;
        data.numInputs = data.numOutputs = 1;
        data.inputs = &inputBus;
        data.outputs = &outputBus;
        data.inputParameterChanges = &changes;
        data.processContext = &context;
        for (bool bypassed : { true, false, true, false })
        {
            changes.queue.value = bypassed ? 1.0 : 0.0;
            controller->setParamNormalized (bypassId, changes.queue.value);
            std::fill_n (inputLeft, 64, 0.25f);
            std::fill_n (inputRight, 64, -0.25f);
            std::fill_n (left, 64, -999.0f);
            std::fill_n (right, 64, -999.0f);
            expectEquals ((int) processor->process (data), (int) kResultOk);
            expectEquals ((int) processor->getLatencySamples(), bypassed ? 0 : 48);
            expectEquals (controller->getParamNormalized (bypassId), bypassed ? 1.0 : 0.0);
            if (bypassed)
            {
                expectEquals (left[0], 0.25f);
                expectEquals (right[0], -0.25f);
            }
            else
            {
                expectEquals (left[0], 0.0f);
                expectEquals (left[48], 0.25f);
            }
        }

        processor->setProcessing (false);
        component->setActive (false);
        processor->release();
        controllerConnection->disconnect (componentConnection);
        componentConnection->disconnect (controllerConnection);
        controllerConnection->release();
        componentConnection->release();
        controller->terminate();
        controller->release();
        component->terminate();
        component->release();
        factory->release();
        if (exit != nullptr)
            exit();
    }

private:
    bool require (bool condition, const juce::String& message = {})
    {
        expect (condition, message);
        return condition;
    }
};
VST3BypassTests vst3BypassTests;
}

void setVST3TestPath (const juce::String& path) { pluginPath = path; }
