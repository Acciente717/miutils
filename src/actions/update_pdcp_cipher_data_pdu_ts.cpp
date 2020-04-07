/* Copyright [2020] Zhiyao Ma */
#include "actions.hpp"
#include "exceptions.hpp"
#include "global_states.hpp"
#include "in_order_executor.hpp"

/// This function extracts and update the global string containing the
/// timestamp of the last LTE_PDCP_UL_Cipher_Data_PDU or
/// LTE_PDCP_DL_Cipher_Data_PDU packet.
/// Note that this function itself does NOT check whether the packet is
/// one of the two above. It MUST be used together with the predicate
/// function.
/// Note that the update is done by the in-order executor.
void update_pdcp_cipher_data_pdu_packet_timestamp(
    pt::ptree &&tree, Job &&job) {
    std::string timestamp = get_packet_time_stamp(tree);

    // Get the direction of the PDCP packets, should be uplink or downlink.
    PDCPDirection direction = PDCPDirection::Unknown;
    for (const auto &i : tree.get_child("dm_log_packet")) {
        if (i.first == "pair") {
            if (i.second.get("<xmlattr>.key", std::string()) == "type_id") {
                auto &&packet_type = i.second.data();
                if (packet_type == "LTE_PDCP_UL_Cipher_Data_PDU") {
                    direction = PDCPDirection::Uplink;
                } else if (packet_type == "LTE_PDCP_DL_Cipher_Data_PDU") {
                    direction = PDCPDirection::Downlink;
                }
                break;
            }
        }
    }

    auto print_timestamp_of_first_pdcp_packet_after_disruption =
        [timestamp, direction] {
        if (g_distuption_events.is_being_disrupted) {
            for (int i = 0;
                i < static_cast<int>(DisruptionEventEnum::NumberOfDisruptions);
                ++i) {
                if (g_distuption_events.disruptions[i]) {
                    (*g_output) << timestamp
                                << " $ FirstPDCPPacketAfterDisruption $ "
                                << "Disruption Type: "
                                << DisruptionEventNames[i]
                                << ", Direction: ";
                    switch (direction) {
                    case PDCPDirection::Unknown:
                        (*g_output) << "unknown";
                        break;
                    case PDCPDirection::Uplink:
                        (*g_output) << "uplink";
                        break;
                    case PDCPDirection::Downlink:
                        (*g_output) << "downlink";
                        break;
                    }
                    (*g_output) << std::endl;
                    g_distuption_events.disruptions[i] = false;
                }
            }
            g_distuption_events.is_being_disrupted = false;
        }
    };

    switch (direction) {
    case PDCPDirection::Unknown:
        throw ProgramBug(
            "Function `update_pdcp_cipher_data_pdu_packet_timestamp`"
            " was invoked with a packet of type neither"
            " LTE_PDCP_UL_Cipher_Data_PDU nor"
            " LTE_PDCP_DL_Cipher_Data_PDU."
        );
    case PDCPDirection::Uplink:
        // Search if there is any uplink pdcp data packets.
        // Note that we only treat packets with size=1412 as data packets,
        // since upper TCP connection is sending at full speed.
        {
            bool uplink_pdcp_data_packet_present = false;
            auto &&pdu_packet_list = locate_subtree_with_attribute(
                tree, "key", "PDCPUL CIPH DATA"
            );
            for (auto pdu_packets : pdu_packet_list) {
                auto &&sizes = locate_subtree_with_attribute(
                    *pdu_packets, "key", "PDU Size"
                );
                for (auto size : sizes) {
                    if (size->data() == "1412") {
                        uplink_pdcp_data_packet_present = true;
                        break;
                    }
                }
                if (uplink_pdcp_data_packet_present) {
                    break;
                }
            }
            if (uplink_pdcp_data_packet_present) {
                insert_ordered_task(
                    job.job_num,
                    [timestamp = std::move(timestamp),
                     print_timestamp_of_first_pdcp_packet_after_disruption] {
                        print_timestamp_of_first_pdcp_packet_after_disruption();
                        g_last_pdcp_packet_timestamp = std::move(timestamp);
                        g_last_pdcp_packet_direction = PDCPDirection::Uplink;
                    }
                );
                return;
            }
        }
        break;
    case PDCPDirection::Downlink:
        // Search if there is any downlink pdcp data packets.
        // Note that we only treat packets with size=1412 as data packets,
        // since upper TCP connection is sending at full speed.
        {
            bool downlink_pdcp_data_packet_present = false;
            auto &&pdu_packet_list = locate_subtree_with_attribute(
                tree, "key", "PDCPDL CIPH DATA"
            );
            for (auto pdu_packets : pdu_packet_list) {
                auto &&sizes = locate_subtree_with_attribute(
                    *pdu_packets, "key", "PDU Size"
                );
                for (auto size : sizes) {
                    if (size->data() == "1412") {
                        downlink_pdcp_data_packet_present = true;
                        break;
                    }
                }
                if (downlink_pdcp_data_packet_present) {
                    break;
                }
            }
            if (downlink_pdcp_data_packet_present) {
                insert_ordered_task(
                    job.job_num,
                    [timestamp = std::move(timestamp),
                     print_timestamp_of_first_pdcp_packet_after_disruption] {
                        print_timestamp_of_first_pdcp_packet_after_disruption();
                        g_last_pdcp_packet_timestamp = std::move(timestamp);
                        g_last_pdcp_packet_direction = PDCPDirection::Downlink;
                    }
                );
                return;
            }
        }
        break;
    default:
        throw ProgramBug(
            "PDCPDirection should be one of the following: "
            "Unknown, Uplink, Downlink."
        );
    }

    insert_ordered_task(
        job.job_num,
        [] {}
    );
}
