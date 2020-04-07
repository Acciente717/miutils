/* Copyright [2020] Zhiyao Ma */
#include "macros.hpp"
#include "actions.hpp"
#include "global_states.hpp"
#include "in_order_executor.hpp"

/// Extract the following field from an LTE_RRC_Serv_Cell_Info packet.
/// <dm_log_packet>
///     <pair key="type_id">LTE_RRC_Serv_Cell_Info</pair>
///     <pair key="timestamp">XXX</pair>
///     <pair key="Cell ID">XXX</pair>
///     <pair key="Downlink frequency">XXX</pair>
///     <pair key="Uplink frequency">XXX</pair>
///     <pair key="Downlink bandwidth">XXX MHz</pair>
///     <pair key="Uplink bandwidth">XXX MHz</pair>
///     <pair key="Cell Identity">XXX</pair>
///     <pair key="TAC">XXX</pair>
///     ...
/// </dm_log_packet>
void extract_rrc_serv_cell_info_packet(
    pt::ptree &&tree, Job &&job) {
    std::string timestamp = "timestamp N/A";
    std::string cell_id, dl_freq, ul_freq;
    std::string dl_bandwidth, ul_bandwidth;
    std::string cell_identity, tracking_area_code;

    for (const auto &i : tree.get_child("dm_log_packet")) {
        if (i.first != "pair") {
            continue;
        }
        if (i.second.get("<xmlattr>.key", std::string()) == "timestamp") {
            timestamp = i.second.data();
        } else if (i.second.get("<xmlattr>.key", std::string()) == "Cell ID") {
            cell_id = i.second.data();
        } else if (i.second.get("<xmlattr>.key", std::string())
                   == "Downlink frequency") {
            dl_freq = i.second.data();
        } else if (i.second.get("<xmlattr>.key", std::string())
                   == "Uplink frequency") {
            ul_freq = i.second.data();
        } else if (i.second.get("<xmlattr>.key", std::string())
                   == "Downlink bandwidth") {
            dl_bandwidth = i.second.data();
        } else if (i.second.get("<xmlattr>.key", std::string())
                   == "Uplink bandwidth") {
            ul_bandwidth = i.second.data();
        } else if (i.second.get("<xmlattr>.key", std::string())
                   == "Cell Identity") {
            cell_identity = i.second.data();
        } else if (i.second.get("<xmlattr>.key", std::string()) == "TAC") {
            tracking_area_code = i.second.data();
        }
    }

    std::string err_msg;
    if_unlikely (timestamp.empty() || cell_id.empty() || dl_freq.empty()
                 || ul_freq.empty() || dl_bandwidth.empty()
                 || ul_bandwidth.empty() || cell_identity.empty()
                 || tracking_area_code.empty()) {
        err_msg += "Warning (packet timestamp = " + timestamp + "): \n";
        err_msg += "The following field in the rrc_serv_cell_info_packet"
                   " is empty\n";
        if (timestamp.empty()) {
            err_msg += "timestamp, ";
        }
        if (cell_id.empty()) {
            err_msg += "Cell ID, ";
        }
        if (dl_freq.empty()) {
            err_msg += "Downlink frequency, ";
        }
        if (ul_freq.empty()) {
            err_msg += "Uplink frequency, ";
        }
        if (dl_bandwidth.empty()) {
            err_msg += "Downlink bandwidth, ";
        }
        if (ul_bandwidth.empty()) {
            err_msg += "Uplink bandwidth, ";
        }
        if (cell_identity.empty()) {
            err_msg += "Cell Identity, ";
        }
        if (tracking_area_code.empty()) {
            err_msg += "TAC, ";
        }
        err_msg += "\n";
        err_msg += "Input file " + job.file_name + " at line "
                   + std::to_string(job.start_line_number) + "-"
                   + std::to_string(job.end_line_number) + "\n";
    }

    insert_ordered_task(
        job.job_num,
        [timestamp = std::move(timestamp), cell_id = std::move(cell_id),
         dl_freq = std::move(dl_freq), ul_freq = std::move(ul_freq),
         dl_bandwidth = std::move(dl_bandwidth),
         ul_bandwidth = std::move(ul_bandwidth),
         cell_identity = std::move(cell_identity),
         tracking_area_code = std::move(tracking_area_code),
         err_msg = std::move(err_msg)] {
             std::cerr << err_msg;
             (*g_output) << timestamp << " $ LTE_RRC_Serv_Cell_Info $ "
                         << "Cell ID: " << cell_id << ", "
                         << "Downlink frequency: " << dl_freq << ", "
                         << "Uplink frequency: " << ul_freq << ", "
                         << "Downlink bandwidth: " << dl_bandwidth << ", "
                         << "Uplink bandwidth: " << ul_bandwidth << ", "
                         << "Cell Identity: " << cell_identity << ", "
                         << "TAC: " << tracking_area_code << std::endl;
        }
    );
}
