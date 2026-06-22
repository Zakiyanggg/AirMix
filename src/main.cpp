#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "audio/AudioBuffer.h"
#include "audio/AudioInput.h"
#include "audio/AudioOutput.h"
#include "audio/EffectProcessor.h"
#include "controls/ButtonInput.h"
#include "controls/ControlState.h"
#include "mapping/GestureToEffectMapper.h"
#include "motion/IMUInput.h"
#include "motion/MotionDetector.h"

using airmix::audio::AudioBuffer;
using airmix::audio::AudioConfig;
using airmix::audio::EffectParameters;
using airmix::audio::EffectProcessor;
using airmix::audio::SimulatedAudioInput;
using airmix::audio::SimulatedAudioOutput;
using airmix::controls::ButtonFrame;
using airmix::controls::SimulatedButtonInput;
using airmix::motion::GestureType;
using airmix::motion::IMUSample;
using airmix::motion::MotionDetector;
using airmix::motion::SimulatedIMUInput;

namespace {

std::string yesNo(bool value) {
    return value ? "yes" : "no";
}

void printFrame(
    int frame,
    GestureType gesture,
    const EffectParameters& parameters,
    float inputRms,
    const SimulatedAudioOutput& output) {
    std::cout << "frame=" << std::setw(2) << frame
              << " gesture=" << std::setw(10) << airmix::motion::toString(gesture)
              << " mode=" << std::setw(9) << airmix::controls::toString(parameters.activeMode)
              << " bypass=" << yesNo(parameters.bypass)
              << " hold=" << yesNo(parameters.hold)
              << " bpm=" << std::fixed << std::setprecision(1) << parameters.tempoBpm
              << " hp=" << std::setw(7) << std::setprecision(0) << parameters.highPassCutoffHz << "Hz"
              << " lp=" << std::setw(7) << parameters.lowPassCutoffHz << "Hz"
              << " gate=" << std::setprecision(2) << parameters.gateDepth
              << " delayMix=" << parameters.delayMix
              << " stutter=" << yesNo(parameters.stutterTrigger)
              << " rms " << std::setprecision(3) << inputRms << " -> " << output.lastRms()
              << " peak=" << output.lastPeak()
              << '\n';
}

}  // namespace

int main() {
    const AudioConfig config{
        .sampleRate = 48000,
        .blockSize = 256,
        .channels = 1,
    };

    SimulatedAudioInput audioInput(config);
    SimulatedAudioOutput audioOutput(config);
    EffectProcessor effectProcessor(config);
    MotionDetector motionDetector;
    airmix::mapping::GestureToEffectMapper mapper;

    std::vector<IMUSample> imuScript = {
        {},                                               // tap 1, no gesture
        {},                                               // tap 2, tempo becomes 120 BPM
        {.ay = 0.88f, .az = 0.72f},                       // tilt left -> high-pass sweep
        {.ay = -0.92f, .az = 0.70f},                      // tilt right -> low-pass sweep
        {.az = 1.00f, .gz = 420.0f},                      // rotate -> gate
        {.ax = 1.80f, .az = 0.75f},                       // punch -> delay throw
        {.ax = 1.55f, .ay = 1.00f, .az = 1.65f, .gx = 190.0f}, // shake -> stutter
        {.ay = 0.95f, .az = 0.70f},                       // hold freezes previous params
        {.az = 0.20f},                                    // bypass on, raw pass-through
        {.az = 1.70f},                                    // bypass off, lift
        {.az = 0.20f},                                    // drop -> duck + low-pass
        {},                                               // effect mode button demo
    };

    std::vector<ButtonFrame> buttonScript = {
        {.tapPressed = true},
        {.tapPressed = true},
        {},
        {.effectModePressed = true},
        {.effectModePressed = true},
        {},
        {},
        {.holdDown = true},
        {.bypassPressed = true},
        {.bypassPressed = true},
        {},
        {.effectModePressed = true},
    };

    SimulatedIMUInput imuInput(imuScript);
    SimulatedButtonInput buttonInput(buttonScript);

    std::cout << "AirMix MVP simulated demo\n";
    std::cout << "---------------------------------------------\n";

    for (int frame = 0; frame < static_cast<int>(imuScript.size()); ++frame) {
        IMUSample imu;
        ButtonFrame buttons;
        imuInput.read(imu);
        buttonInput.read(buttons);

        AudioBuffer buffer;
        audioInput.read(buffer);
        const float inputRms = airmix::audio::bufferRms(buffer);

        const GestureType gesture = motionDetector.detect(imu);
        const double timestampSec = static_cast<double>(frame) * 0.5;
        const EffectParameters parameters = mapper.update(gesture, imu, buttons, timestampSec);

        effectProcessor.process(buffer, parameters);
        audioOutput.write(buffer);

        printFrame(frame, gesture, parameters, inputRms, audioOutput);
    }

    std::cout << "---------------------------------------------\n";
    std::cout << "Demo complete. Hardware drivers are still mocked behind interfaces.\n";
    return 0;
}

