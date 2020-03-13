/* Copyright [2019] Zhiyao Ma */
#ifndef GLOBAL_STATES_HPP_
#define GLOBAL_STATES_HPP_

#include <exception>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <functional>
#include <iostream>
#include <vector>
#include <ctime>

/// The states for the state machine in the main function.
enum class MainState {
    /// Initializing action list and starting sub threads.
    Initializing,
    /// All sub threads are running without exceptions.
    AllRunning,
    /// The splitter has finished, while the extractors and the in-order
    /// executor are still running.
    SplitterFinished,
    /// The splitter and all the extractors have finished, while the
    /// in-order executor is still running.
    ExtractorFinished,
    /// All sub threads have finished.
    InOrderExecutorFinished,
    /// Exceptions are thrown in some sub threads.
    Error
};

/// The direction of the last seen PDCP packet.
enum class PDCPDirection {
    // We have not seen any pdcp data packet.
    Unknown,
    // The last pdcp log contains an uplink packet.
    Uplink,
    // The last pdcp log contains a downlink packet.
    Downlink
};

/// Enum for disruption events. These events are possible subfields
/// in RRC OTA packets.
enum class DisruptionEventEnum {
    RRCConnectionReconfiguration,
    RRCConnectionReconfigurationComplete,
    RRCConnectionReestablishmentRequest,
    RRCConnectionReestablishmentComplete,
    RRCConnectionRequest,
    RRCConnectionSetup,
    NumberOfDisruptions
};

/// Name of the disruption events, ordered in the same sequence
/// as in the corresponding enum.
constexpr const char *DisruptionEventNames[] = {
    "RRCConnectionReconfiguration",
    "RRCConnectionReconfigurationComplete",
    "RRCConnectionReestablishmentRequest",
    "RRCConnectionReestablishmentComplete",
    "RRCConnectionRequest",
    "RRCConnectionSetup",
    "NumberOfDisruptions"
};

/// This structure records on going disruption events.
struct DisruptionEvents {
    /// If there is any ongoing disruption.
    bool is_being_disrupted;
    /// Disruption vector corresponding to events in the enum.
    bool disruptions[
        static_cast<int>(DisruptionEventEnum::NumberOfDisruptions)
    ];
};

/// Parameter: the number of extractor thread.
extern int g_thread_num;

/// Parameter: input file names.
extern std::vector<std::string> g_input_file_names;

/// Parameter: input file streams.
extern std::vector< std::unique_ptr<std::istream,
                    std::function<void(std::istream*)>> > g_inputs;

/// Parameter: output file stream.
extern std::unique_ptr<std::ostream,
                       std::function<void(std::ostream*)> > g_output;

/// The global exception pointer.
extern std::exception_ptr g_pexcept;

/// The mutex protecting the main state and the global exception pointer.
extern std::mutex g_main_state_mtx;

/// The enum representing the main state.
extern MainState g_main_state;

/// The condition variable used to notify the change of main state.
extern std::condition_variable g_main_state_change_cv;

/// The timestamp of the transmission of the pdcp packets contained in the
/// last LTE_PDCP_UL_Cipher_Data_PDU or LTE_PDCP_DL_Cipher_Data_PDU packet.
extern std::string g_last_pdcp_packet_timestamp;

/// The direction of the transmission of the pdcp packets contained in the
/// last LTE_PDCP_UL_Cipher_Data_PDU or LTE_PDCP_DL_Cipher_Data_PDU packet.
extern PDCPDirection g_last_pdcp_packet_direction;

/// Mark if we have just completed an handover.
extern bool g_first_pdcp_packet_after_handover;

/// Disruption events, see definition of the structure for details.
extern DisruptionEvents g_distuption_events;

/// Valid ranges of timestamps, provided by --range argument.
extern std::vector<std::pair<time_t, time_t>> g_valid_time_range;

/// Enabled extractor names.
extern std::vector<std::string> g_enabled_extractors;

/// The largest timestamp we have ever seen in the packets.
extern time_t g_latest_seen_timestamp;

/// The largest timestamp in string representation
/// that we have ever seen in the packets.
extern std::string g_latest_seen_ts_string;

/// Sub threads call this function to propagate caught exception to the
/// main thread. It changes the main state to Error and set the exception
/// pointer.
extern void propagate_exeption_to_main();

#endif  // GLOBAL_STATES_HPP_
