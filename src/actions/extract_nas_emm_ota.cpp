/* Copyright [2020] Zhiyao Ma */
#include "actions.hpp"
#include "global_states.hpp"
#include "in_order_executor.hpp"

/// This function extracts and prints tracking area update accept or reject
/// from LTE_NAS_EMM_OTA_Incoming_Packet packets. For update accept, it
/// looks for the pattern shown below.
/// <dm_log_packet>
///     ...
///         <field name="nas_eps.nas_msg_emm_type"
///             showname="NAS EPS Mobility Management Message Type:
///                       Tracking area update accept (0x49)" ... />
///     ...
/// </dm_log_packet>
void extract_nas_emm_ota_incoming_packet(pt::ptree &&tree, Job &&job) {
    std::string timestamp = get_packet_time_stamp(tree);

    bool tracking_area_update_accept = false;
    bool tracking_area_update_reject = false;
    auto &&nas_msg_emm_type_fields = locate_subtree_with_attribute(
        tree, "name", "nas_eps.nas_msg_emm_type"
    );
    for (auto ptr : nas_msg_emm_type_fields) {
        auto &&showname = ptr->get("<xmlattr>.showname", std::string());
        if (showname.find("Tracking area update accept") != std::string::npos) {
            tracking_area_update_accept = true;
            break;
        }
        if (showname.find("Tracking area update reject") != std::string::npos) {
            tracking_area_update_reject = true;
            break;
        }
    }

    // If tracking area update request is neither accecpted or rejected,
    // we have nothing to print. Simply do nothing.
    if (!tracking_area_update_accept && !tracking_area_update_reject) {
        insert_ordered_task(
            job.job_num, [] {}
        );
        return;
    }

    std::string message;
    message += timestamp + " $ LTE_NAS_EMM_OTA_Incoming_Packet $ "
               + "Tracking area update accept: ";
    if (tracking_area_update_accept) {
        message += "1";
    } else {
        message += "0";
    }
    message += ", Tracking area update reject: ";
    if (tracking_area_update_reject) {
        message += "1";
    } else {
        message += "0";
    }

    insert_ordered_task(
        job.job_num,
        [message = std::move(message)] {
            (*g_output) << message << std::endl;
        }
    );
}

/// This function extracts and prints tracking area update request
/// from LTE_NAS_EMM_OTA_Outgoing_Packet packets. It looks for the
/// pattern shown below.
/// <dm_log_packet>
///     ...
///         <field name="nas_eps.nas_msg_emm_type"
///             showname="NAS EPS Mobility Management Message Type:
///                       Tracking area update request (0x48)" ... />
///     ...
/// </dm_log_packet>
void extract_nas_emm_ota_outgoing_packet(
    pt::ptree &&tree, Job &&job) {
    std::string timestamp = get_packet_time_stamp(tree);

    bool tracking_area_update_request = false;
    auto &&nas_msg_emm_type_fields = locate_subtree_with_attribute(
        tree, "name", "nas_eps.nas_msg_emm_type"
    );
    for (auto ptr : nas_msg_emm_type_fields) {
        auto &&showname = ptr->get("<xmlattr>.showname", std::string());
        if (showname.find("Tracking area update request")
            != std::string::npos) {
            tracking_area_update_request = true;
            break;
        }
    }

    // If the packet does not contain tracking area update request,
    // we have nothing to print. Simply do nothing.
    if (!tracking_area_update_request) {
        insert_ordered_task(
            job.job_num, [] {}
        );
        return;
    }

    std::string message;
    message += timestamp + " $ LTE_NAS_EMM_OTA_Outgoing_Packet $ "
               + "Tracking area update request: ";
    if (tracking_area_update_request) {
        message += "1";
    } else {
        message += "0";
    }

    insert_ordered_task(
        job.job_num,
        [message = std::move(message)] {
            (*g_output) << message << std::endl;
        }
    );
}
