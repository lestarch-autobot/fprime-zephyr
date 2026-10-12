// ======================================================================
// \title  LoRa.cpp
// \author lestarch
// \brief  cpp file for LoRa component implementation class
// ======================================================================

#include "fprime-zephyr/Drv/LoRa/LoRa.hpp"
#include "zephyr-config/LoRaCfg.hpp"
#include <Fw/Logger/Logger.hpp>
#include <zephyr/kernel.h>
#include <errno.h>
#if defined(CONFIG_HAS_SEMTECH_SX1276)
#include <radio.h>
#include <sx1276/sx1276Regs-LoRa.h>
#define ZEPHYR_LORA_HAS_RSSI 1
#elif defined(CONFIG_HAS_SEMTECH_SX1272)
#include <radio.h>
#include <sx1272/sx1272Regs-LoRa.h>
#define ZEPHYR_LORA_HAS_RSSI 1
#endif
namespace Zephyr {

namespace {
#if defined(CONFIG_HAS_SEMTECH_SX1276)
// SX1276 RegRssiValue offsets for the high-frequency (> 525 MHz) and low-frequency ports
constexpr I16 SX127X_RSSI_OFFSET_HF = -157;
constexpr I16 SX127X_RSSI_OFFSET_LF = -164;
constexpr U32 SX127X_MID_BAND_HZ = 525000000;
#elif defined(CONFIG_HAS_SEMTECH_SX1272)
constexpr I16 SX127X_RSSI_OFFSET_HF = -139;
constexpr I16 SX127X_RSSI_OFFSET_LF = -139;
constexpr U32 SX127X_MID_BAND_HZ = 0;
#endif
}  // namespace

// Margin past a continuous wave's duration for the driver to release the modem
static constexpr U32 CW_TEARDOWN_MARGIN_US = 250000;

static_assert((0 < LoRaConfig::MIN_FREQUENCY) && (LoRaConfig::MIN_FREQUENCY <= LoRaConfig::FREQUENCY) &&
                  (LoRaConfig::FREQUENCY <= LoRaConfig::MAX_FREQUENCY),
              "LoRaConfig: FREQUENCY must lie within a non-zero MIN_FREQUENCY to MAX_FREQUENCY");

// LoRaBandwidth is cast straight into Zephyr's lora_signal_bandwidth, so the encodings must agree
static_assert(static_cast<U16>(LoRaBandwidth::BW_125_KHZ) == ::BW_125_KHZ,
              "LoRaBandwidth::BW_125_KHZ must match Zephyr's lora_signal_bandwidth");
static_assert(static_cast<U16>(LoRaBandwidth::BW_250_KHZ) == ::BW_250_KHZ,
              "LoRaBandwidth::BW_250_KHZ must match Zephyr's lora_signal_bandwidth");
static_assert(static_cast<U16>(LoRaBandwidth::BW_500_KHZ) == ::BW_500_KHZ,
              "LoRaBandwidth::BW_500_KHZ must match Zephyr's lora_signal_bandwidth");

// Active LoRa modem configuration; frequency is the current carrier set at boot or by SET_FREQ
struct lora_modem_config BASE_CONFIG = {
    .frequency = LoRaConfig::FREQUENCY,
    .bandwidth = BW_125_KHZ,
    .datarate = SF_8,
    .coding_rate = CR_4_5,
    .preamble_len = LoRaConfig::PREAMBLE_LENGTH,
    .tx_power = LoRaConfig::TX_POWER,
    .tx = false,
    .iq_inverted = false,
    .public_network = false,
};

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

void LoRa::receiveCallback(const struct device* dev, U8* data, U16 size, I16 rssi, I8 snr, void* user_data) {
    Zephyr::LoRa* lora_component = static_cast<Zephyr::LoRa*>(user_data);
    FW_ASSERT(lora_component != nullptr);
    lora_component->receive(data, size, rssi, snr);
}

LoRa ::LoRa(const char* const compName)
    : LoRaComponentBase(compName), m_lora_device(nullptr), m_transmit_enabled(TransmitState::DISABLED) {}

LoRa ::~LoRa() {}

LoRa::Status LoRa ::start(const struct device* lora_device, const TransmitState& transmit_enabled) {
    this->m_lora_device = lora_device;
    FW_ASSERT(lora_device != nullptr);
    if (!device_is_ready(lora_device)) {
        return NOT_READY;
    }

    LoRa::Status config_status = this->enableRx(true);
    if (config_status != Status::SUCCESS) {
        this->log_WARNING_HI_ConfigurationFailed(LoRaMode::Receive);
        return LoRa::Status::ERROR;
    }
    // On start, if the transmit is enabled then start the com status ping-pong transmit protocol
    this->setTransmitState(transmit_enabled);

    return Status::SUCCESS;
}

LoRa::Status LoRa ::enableTx() {
    Fw::ParamValid isValid = Fw::ParamValid::INVALID;
    const LoRaDataRate data_rate = this->paramGet_DATA_RATE(isValid);
    FW_ASSERT((isValid == Fw::ParamValid::VALID) || (isValid == Fw::ParamValid::DEFAULT), static_cast<FwAssertArgType>(isValid));
    const LoRaCodingRate coding_rate = this->paramGet_CODING_RATE(isValid);
    FW_ASSERT((isValid == Fw::ParamValid::VALID) || (isValid == Fw::ParamValid::DEFAULT), static_cast<FwAssertArgType>(isValid));
    const LoRaBandwidth bandwidth = this->paramGet_BANDWIDTH_TX(isValid);
    FW_ASSERT((isValid == Fw::ParamValid::VALID) || (isValid == Fw::ParamValid::DEFAULT), static_cast<FwAssertArgType>(isValid));

    // Release the idle RxContinuous listen so the driver grants the modem to TX
    int status = lora_recv_async(this->m_lora_device, nullptr, nullptr);
    if (status == 0) {
        // Update BASE_CONFIG in-place to save on stack space
        BASE_CONFIG.tx = true;
        BASE_CONFIG.datarate = static_cast<enum lora_datarate>(data_rate.e);
        BASE_CONFIG.coding_rate = static_cast<enum lora_coding_rate>(coding_rate.e);
        BASE_CONFIG.bandwidth = static_cast<enum lora_signal_bandwidth>(bandwidth.e);

        status = lora_config(this->m_lora_device, &BASE_CONFIG);
    }
    return (status < 0) ? Status::ERROR : Status::SUCCESS;
}

bool LoRa ::channelEnergyDetected() {
#if defined(ZEPHYR_LORA_HAS_RSSI)
    // Instantaneous in-band RSSI: the HAL leaves Radio.Rssi unset, so read the register with its band offset
    const I16 offset = (BASE_CONFIG.frequency > SX127X_MID_BAND_HZ) ? SX127X_RSSI_OFFSET_HF : SX127X_RSSI_OFFSET_LF;
    const I16 rssi = static_cast<I16>(offset + static_cast<I16>(Radio.Read(REG_LR_RSSIVALUE)));
    this->tlmWrite_ChannelRssi(rssi);
    return rssi > LoRaConfig::RSSI_BUSY_THRESHOLD_DBM;
#else
    return false;
#endif
}

LoRa::Status LoRa ::enableRx(bool initial) {
    Fw::ParamValid isValid = Fw::ParamValid::INVALID;
    const LoRaDataRate data_rate = this->paramGet_DATA_RATE(isValid);
    FW_ASSERT((isValid == Fw::ParamValid::VALID) || (isValid == Fw::ParamValid::DEFAULT), static_cast<FwAssertArgType>(isValid));
    const LoRaCodingRate coding_rate = this->paramGet_CODING_RATE(isValid);
    FW_ASSERT((isValid == Fw::ParamValid::VALID) || (isValid == Fw::ParamValid::DEFAULT), static_cast<FwAssertArgType>(isValid));
    const LoRaBandwidth bandwidth = this->paramGet_BANDWIDTH_RX(isValid);
    FW_ASSERT((isValid == Fw::ParamValid::VALID) || (isValid == Fw::ParamValid::DEFAULT), static_cast<FwAssertArgType>(isValid));

    // Update BASE_CONFIG in-place to save on stack space
    BASE_CONFIG.tx = false;
    BASE_CONFIG.datarate = static_cast<enum lora_datarate>(data_rate.e);
    BASE_CONFIG.coding_rate = static_cast<enum lora_coding_rate>(coding_rate.e);
    BASE_CONFIG.bandwidth = static_cast<enum lora_signal_bandwidth>(bandwidth.e);

    // On the initial configuration log the config parameters
    if (initial) {
        Fw::Logger::log("[LoRa] Frequency: %" PRI_U32 "\n", BASE_CONFIG.frequency);
        Fw::Logger::log("[LoRa] Bandwidth: %" PRI_U32 "\n", static_cast<U32>(BASE_CONFIG.bandwidth));
        Fw::Logger::log("[LoRa] Data Rate (Spreading Factor): %" PRI_U8 "\n", static_cast<U8>(BASE_CONFIG.datarate));
        Fw::Logger::log("[LoRa] Coding Rate: %" PRI_U8 "\n", static_cast<U8>(BASE_CONFIG.coding_rate));
        Fw::Logger::log("[LoRa] Preamble Length: %" PRI_U16 "\n", BASE_CONFIG.preamble_len);
        Fw::Logger::log("[LoRa] TX Power: %" PRI_I8 "\n", BASE_CONFIG.tx_power);
    }
    int status = lora_config(this->m_lora_device, &BASE_CONFIG);
    if (status == 0) {
        status = lora_recv_async(this->m_lora_device, LoRa::receiveCallback, this);
    }
    return (status < 0) ? Status::ERROR : Status::SUCCESS;
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void LoRa ::dataIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    Os::ScopeLock lock(this->m_mutex);
    FW_ASSERT(data.getSize() + sizeof(LoRaConfig::HEADER) <= LoRa::MAX_PACKET_SIZE, data.getSize());
    FW_ASSERT(this->m_lora_device != nullptr);
    FW_ASSERT(device_is_ready(this->m_lora_device));
    Fw::Success returnStatus = Fw::Success::FAILURE;
    bool deferred = false;
    U32 busy_ms = 0;
    if (this->m_transmit_enabled == TransmitState::ENABLED) {
        // Channel energy defers the transmit, but only for MAX_TX_DEFER_MS: energy that outlasts any packet is
        // noise or jamming, and waiting on it would silence this radio for as long as it persists
        const bool energy = this->channelEnergyDetected();
        Os::ScopeLock recoveryLock(this->m_recovery_mutex);
        if (energy) {
            const U32 now = k_uptime_get_32();
            if (!this->m_busy_wait_active) {
                this->m_busy_wait_active = true;
                this->m_busy_since_ms = now;
            }
            busy_ms = now - this->m_busy_since_ms;
        }
        deferred = energy && (busy_ms < LoRaConfig::MAX_TX_DEFER_MS);
    }
    if (deferred) {
        // Defer rather than abort the receive; run_handler emits the recovery SUCCESS owed for this FAILURE
        this->m_transmits_deferred++;
        this->tlmWrite_TransmitsDeferred(this->m_transmits_deferred);
    } else if ((this->m_transmit_enabled == TransmitState::ENABLED) && !this->updateContinuousWave()) {
        {
            Os::ScopeLock recoveryLock(this->m_recovery_mutex);
            this->m_busy_wait_active = false;
        }
        if (busy_ms >= LoRaConfig::MAX_TX_DEFER_MS) {
            this->m_transmits_forced++;
            this->tlmWrite_TransmitsForced(this->m_transmits_forced);
            this->log_WARNING_LO_ChannelBusyTimeout(busy_ms);
        } else {
            this->log_WARNING_LO_ChannelBusyTimeout_ThrottleClear();
        }
        Status status = this->enableTx();
        if (status == Status::SUCCESS) {
            (void)::memcpy(this->m_send_buffer, LoRaConfig::HEADER, sizeof(LoRaConfig::HEADER));
            (void)::memcpy(this->m_send_buffer + sizeof(LoRaConfig::HEADER), data.getData(), data.getSize());
            int send_status =
                lora_send(this->m_lora_device, this->m_send_buffer, sizeof(LoRaConfig::HEADER) + data.getSize());
            if (send_status != 0) {
                this->log_WARNING_HI_SendFailed(static_cast<I32>(send_status));
                returnStatus = Fw::Success::FAILURE;
            } else {
                this->m_bytes_sent += data.getSize();
                this->tlmWrite_BytesSent(this->m_bytes_sent);
                returnStatus = Fw::Success::SUCCESS;
                this->log_WARNING_HI_ConfigurationFailed_ThrottleClear();
                this->log_WARNING_HI_SendFailed_ThrottleClear();
            }
        } else {
            this->log_WARNING_HI_ConfigurationFailed(LoRaMode::Transmit);
            returnStatus = Fw::Success::FAILURE;
        }
        // Enable RX mode after all TXes
        status = this->enableRx();
        if (status != Status::SUCCESS) {
            this->log_WARNING_HI_ConfigurationFailed(LoRaMode::Receive);
        }
    } else if (this->m_transmit_enabled == TransmitState::DISABLING) {
        this->m_transmit_enabled = TransmitState::DISABLED;
    }
    this->dataReturnOut_out(0, data, context);
    this->comStatusOut_out(0, returnStatus);
    // Arm only after the FAILURE is delivered so the recovery SUCCESS can never overtake it
    if (deferred) {
        Os::ScopeLock recoveryLock(this->m_recovery_mutex);
        this->m_recovery_pending = true;
        this->m_deferred_at_ms = k_uptime_get_32();
    }
}

void LoRa ::dataReturnIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    this->deallocate_out(0, data);
}

void LoRa ::enableTransmit_handler(FwIndexType portNum) {
    this->setTransmitState(TransmitState::ENABLED);
}

void LoRa ::disableTransmit_handler(FwIndexType portNum) {
    this->setTransmitState(TransmitState::DISABLING);
}

void LoRa ::run_handler(FwIndexType portNum, U32 context) {
    {
        Os::ScopeLock lock(this->m_mutex);
        (void)this->updateContinuousWave();
    }
    bool recover = false;
    {
        // Separate from m_mutex so the rate group is never blocked behind a transmit in progress
        Os::ScopeLock recoveryLock(this->m_recovery_mutex);
        const U32 now = k_uptime_get_32();
        const bool busy = this->channelEnergyDetected();
        if (this->m_recovery_pending) {
            // Also recover once the channel-energy wait expires, so the retry's resend goes out at the bound
            recover = !busy || ((now - this->m_deferred_at_ms) >= LoRaConfig::DEFERRED_TX_RECOVERY_MS) ||
                      (this->m_busy_wait_active && ((now - this->m_busy_since_ms) >= LoRaConfig::MAX_TX_DEFER_MS));
            this->m_recovery_pending = !recover;
        } else if (!busy) {
            // Nothing is waiting and the channel is clear: the busy episode is over, the next starts fresh
            this->m_busy_wait_active = false;
        }
    }
    // Exactly one recovery SUCCESS per deferral, emitted outside the lock so the retry may resend immediately
    if (recover) {
        Fw::Success status = Fw::Success::SUCCESS;
        this->comStatusOut_out(0, status);
    }
}

void LoRa ::receive(U8* data, U16 size, I16 rssi, I8 snr) {
    FW_ASSERT(data != nullptr);
    const FwSizeType payload_size = static_cast<FwSizeType>(size - sizeof(LoRaConfig::HEADER));
    Fw::Buffer buffer = this->allocate_out(0, payload_size);
    if (buffer.isValid()) {
        (void)::memcpy(buffer.getData(), data + sizeof(LoRaConfig::HEADER), payload_size);
        ComCfg::FrameContext frameContext;
        this->m_bytes_received += payload_size;
        this->tlmWrite_BytesReceived(this->m_bytes_received);
        this->dataOut_out(0, buffer, frameContext);
    } else {
        this->log_WARNING_HI_AllocationFailed(payload_size);
    }
    this->tlmWrite_LastRssi(rssi);
    this->tlmWrite_LastSnr(snr);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void LoRa ::CONTINUOUS_WAVE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U16 seconds) {
    Os::ScopeLock lock(this->m_mutex);
    Fw::CmdResponse response = Fw::CmdResponse::OK;
    if (this->updateContinuousWave()) {
        response = Fw::CmdResponse::BUSY;
    } else {
        Status status = this->enableTx();
        if (status == Status::SUCCESS) {
            // lora_test_cw() returns immediately and holds the modem until the wave ends, so RX is re-armed later
            const int cw_status =
                lora_test_cw(this->m_lora_device, BASE_CONFIG.frequency, LoRaConfig::TX_POWER, seconds);
            if (cw_status == 0) {
                const Fw::Time now = this->getTime();
                this->m_cw_end = Fw::Time::add(now, Fw::Time(now.getTimeBase(), seconds, CW_TEARDOWN_MARGIN_US));
                this->m_cw_active = true;
            } else {
                this->log_WARNING_HI_SendFailed(static_cast<I32>(cw_status));
                status = Status::ERROR;
            }
        }
        if (status != Status::SUCCESS) {
            if (this->enableRx() != Status::SUCCESS) {
                this->log_WARNING_HI_ConfigurationFailed(LoRaMode::Receive);
            }
            response = Fw::CmdResponse::EXECUTION_ERROR;
        }
    }
    this->cmdResponse_out(opCode, cmdSeq, response);
}

bool LoRa ::updateContinuousWave() {
    if (this->m_cw_active && (Fw::Time::compare(this->getTime(), this->m_cw_end) != Fw::TimeComparison::LT)) {
        this->m_cw_active = false;
        if (this->enableRx() != Status::SUCCESS) {
            this->log_WARNING_HI_ConfigurationFailed(LoRaMode::Receive);
        }
    }
    return this->m_cw_active;
}

void LoRa ::SET_FREQ_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 freq_hz) {
    Os::ScopeLock lock(this->m_mutex);
    FW_ASSERT(this->m_lora_device != nullptr);
    Fw::CmdResponse response = Fw::CmdResponse::OK;
    if ((freq_hz < LoRaConfig::MIN_FREQUENCY) || (freq_hz > LoRaConfig::MAX_FREQUENCY)) {
        this->log_WARNING_LO_FrequencyOutOfRange(freq_hz, LoRaConfig::MIN_FREQUENCY, LoRaConfig::MAX_FREQUENCY);
        response = Fw::CmdResponse::VALIDATION_ERROR;
    } else if (this->updateContinuousWave()) {
        response = Fw::CmdResponse::BUSY;
    } else {
        // -EINVAL means receive was not armed; retuning re-arms it, so only other errors stop the retune
        const int cancel_status = lora_recv_async(this->m_lora_device, nullptr, nullptr);
        if ((cancel_status != 0) && (cancel_status != -EINVAL)) {
            this->log_WARNING_HI_ConfigurationFailed(LoRaMode::Receive);
            response = Fw::CmdResponse::EXECUTION_ERROR;
        }
    }
    if (response == Fw::CmdResponse::OK) {
        const U32 previous_freq = BASE_CONFIG.frequency;
        BASE_CONFIG.frequency = freq_hz;
        if (this->enableRx() == Status::SUCCESS) {
            this->log_ACTIVITY_HI_FrequencySet(freq_hz);
        } else {
            BASE_CONFIG.frequency = previous_freq;
            if (this->enableRx() != Status::SUCCESS) {
                this->log_WARNING_HI_ConfigurationFailed(LoRaMode::Receive);
            }
            response = Fw::CmdResponse::EXECUTION_ERROR;
        }
    }
    this->cmdResponse_out(opCode, cmdSeq, response);
}

void LoRa ::setTransmitState(TransmitState state) {
    Os::ScopeLock lock(this->m_mutex);
    // Want to enable
    if (state == TransmitState::ENABLED) {
        // Start the ping-pong protocol if we are disabled
        const bool was_disabled = (this->m_transmit_enabled == TransmitState::DISABLED);
        // Must transition to ENABLED **BEFORE** calling comStatusOut
        this->m_transmit_enabled = TransmitState::ENABLED;
        if (was_disabled) {
            Fw::Success comStatus = Fw::Success::SUCCESS;
            this->comStatusOut_out(0, comStatus);
        }
        if (!this->m_lora_ever_on) {
            this->m_lora_ever_on = true;
            if (this->isConnected_loraFirstStart_OutputPort(0)) {
                this->loraFirstStart_out(0);
            }
        }
    }
    // Want to disable
    else {
        // If not already disabled, then the ping-pong protocol should be stopped and thus we go to DISABLING state
        if (this->m_transmit_enabled != TransmitState::DISABLED) {
            this->m_transmit_enabled = TransmitState::DISABLING;
        }
    }
}

void LoRa ::TRANSMIT_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, const TransmitState& enabled) {
    this->setTransmitState(enabled);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}
}  // namespace Zephyr
