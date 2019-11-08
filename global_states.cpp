/* Copyright [2019] Zhiyao Ma */
#include "global_states.hpp"

/// Parameter: the number of extractor thread
int g_thread_num = 4;

/// Parameter: input file names.
std::vector<std::string> g_input_file_names;

/// Parameter: input file streams.
std::vector< std::unique_ptr<std::istream,
             std::function<void(std::istream*)>> > g_inputs;

/// Parameter: output file stream.
std::unique_ptr<std::ostream,
                std::function<void(std::ostream*)> > g_output;

/// The global exception pointer.
std::exception_ptr g_pexcept = nullptr;

/// The mutex protecting the main state and the global exception pointer.
std::mutex g_main_state_mtx;

/// The enum representing the main state.
MainState g_main_state = MainState::Initializing;

/// The condition variable used to notify the change of main state.
std::condition_variable g_main_state_change_cv;

/// The timestamp of the transmission of the pdcp packets contained in the
/// last LTE_PDCP_UL_Cipher_Data_PDU or LTE_PDCP_DL_Cipher_Data_PDU packet.
std::string g_last_pdcp_packet_timestamp = "unknown";

/// The direction of the transmission of the pdcp packets contained in the
/// last LTE_PDCP_UL_Cipher_Data_PDU or LTE_PDCP_DL_Cipher_Data_PDU packet.
PDCPDirection g_last_pdcp_packet_direction = PDCPDirection::Unknown;

DisruptionEvents g_distuption_events = {
    .is_being_disrupted = false
};

/// Sub threads call this function to propagate caught exception to the
/// main thread. It changes the main state to Error and set the exception
/// pointer.
void propagate_exeption_to_main() {
    std::lock_guard<std::mutex> guard(g_main_state_mtx);
    if (g_main_state != MainState::Error) {
        g_pexcept = std::current_exception();
        g_main_state = MainState::Error;
        g_main_state_change_cv.notify_one();
    }
}
