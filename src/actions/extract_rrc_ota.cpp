/* Copyright [2020] Zhiyao Ma */
#include "macros.hpp"
#include "actions.hpp"
#include "global_states.hpp"
#include "in_order_executor.hpp"

/// This function extracts several kinds of information from RRC_OTA
/// packets. Currently 14 kinds of information are extracted.
/// 1. adding mapping between measurement event types to report config IDs
/// 2. removing mapping between measurement event types to report config IDs
/// 3. adding mapping between report config IDs to measurement IDs
/// 4. removing mapping betwee report config IDs to measurement IDs
/// 5. sending measurement report with triggering measurement ID
/// 6. sending RRC connection reestablishment request
/// 7. receiving RRC connection reestablishment complete
/// 8. receiving RRC connection reestablishment reject
/// 9. sending RRC connection reconfiguration
/// 10. sending RRC connection reconfiguration complete
/// 11. sending RRC connection release
/// 12. sending RRC connection request
/// 13. receiving RRC connection setup
/// 14. receiving RRC connection reject
void extract_rrc_ota_packet(pt::ptree &&tree, Job &&job) {
    // Warning message to be printed to stderr.
    std::string warning_message;

    // Extract the timestamp.
    std::string timestamp = get_packet_time_stamp(tree);

    // Extract new mapping between measurement event types to report config IDs.
    // <field name="lte-rrc.ReportConfigToAddMod_element" ... >
    //     ...
    //         ... <field name="lte-rrc.reportConfigId"
    //                    showname="reportConfigId: X" .../>
    //         ... <field name="lte-rrc.eventId"
    //                    showname="eventId: eventAX (X)" ... >
    //                 ...
    //             </field>
    //     ...
    // </field>
    std::vector<std::string> added_config_ids, added_event_types;
    {
        auto &&report_config_nodes = locate_subtree_with_attribute(
            tree, "name", "lte-rrc.ReportConfigToAddMod_element"
        );
        std::vector<const pt::ptree*> report_config_id_nodes, event_id_nodes;
        for (auto ptr : report_config_nodes) {
            auto &&ret = locate_subtree_with_attribute(
                *ptr, "name", "lte-rrc.reportConfigId"
            );
            if_likely (ret.size() == 1) {
                for (auto iter = ret.begin(); iter != ret.end(); ++iter) {
                    auto &&subret = locate_subtree_with_attribute(
                        *ptr, "name", "lte-rrc.eventId"
                    );
                    if_likely (subret.size() == 1) {
                        event_id_nodes.push_back(*iter);
                        report_config_id_nodes.push_back(subret[0]);
                    } else {
                        warning_message +=
                            generate_vector_size_unexpected_message(
                                timestamp,
                                "vector containing lte-rrc.eventId",
                                subret.size(),
                                1, 1, job
                            );
                    }
                }
            } else {
                warning_message +=
                    generate_vector_size_unexpected_message(
                        timestamp,
                        "vector containing lte-rrc.reportConfigId",
                        ret.size(),
                        1, 1, job
                    );
            }
        }

        if_unlikely (report_config_id_nodes.size() != event_id_nodes.size()) {
            throw_vector_size_unequal(
                "report_config_nodes", "event_id_nodes",
                report_config_id_nodes.size(),
                event_id_nodes.size(),
                job
            );
        }
        for (auto i = 0; i < report_config_id_nodes.size(); ++i) {
            added_event_types.push_back(
                event_id_nodes[i]->get<std::string>("<xmlattr>.showname")
            );
            added_config_ids.push_back(
                report_config_id_nodes[i]
                    ->get<std::string>("<xmlattr>.showname")
            );
        }
    }

    // Extract removal of mapping between measurement event types to
    // report config IDs.
    // <field name="lte-rrc.reportConfigToRemoveList" ... >
    //     ...
    //         ... <field name="lte-rrc.ReportConfigId"
    //                    showname="ReportConfigId: X" ... />
    //         ...
    //     ...
    // </field>
    std::vector<std::string> removed_config_ids;
    {
        auto &&report_config_remove_nodes = locate_subtree_with_attribute(
            tree, "name", "lte-rrc.reportConfigToRemoveList"
        );
        std::vector<const pt::ptree*> remove_config_id_nodes;
        for (auto ptr : report_config_remove_nodes) {
            auto &&ret = locate_subtree_with_attribute(
                *ptr, "name", "lte-rrc.ReportConfigId"
            );
            remove_config_id_nodes.insert(
                remove_config_id_nodes.end(), ret.begin(), ret.end()
            );
        }
        for (auto i = 0; i < remove_config_id_nodes.size(); ++i) {
            removed_config_ids.push_back(
                remove_config_id_nodes[i]
                    ->get<std::string>("<xmlattr>.showname")
            );
        }
    }

    // Extract new mapping between report config IDs to measurement IDs.
    // <field name="lte-rrc.MeasIdToAddMod_element" ... >
    //     ...
    //         ... <field name="lte-rrc.reportConfigId"
    //                    showname="reportConfigId: X" .../>
    //         ... <field name="lte-rrc.measId"
    //                    showname="measId: 5" .../>
    //     ...
    // </field>
    std::vector<std::string> added_measure_ids, report_to_measure_ids;
    {
        auto &&measure_id_to_add_nodes = locate_subtree_with_attribute(
            tree, "name", "lte-rrc.MeasIdToAddMod_element"
        );
        std::vector<const pt::ptree*> added_measure_id_nodes, measure_id_nodes;
        for (auto ptr : measure_id_to_add_nodes) {
            auto &&ret = locate_subtree_with_attribute(
                *ptr, "name", "lte-rrc.reportConfigId"
            );
            if_likely (ret.size() == 1) {
                for (auto iter = ret.begin(); iter != ret.end(); ++iter) {
                    auto &&subret = locate_subtree_with_attribute(
                        *ptr, "name", "lte-rrc.measId"
                    );
                    if_likely (subret.size() == 1) {
                        added_measure_id_nodes.push_back(*iter);
                        measure_id_nodes.push_back(subret[0]);
                    } else {
                        warning_message +=
                            generate_vector_size_unexpected_message(
                                timestamp,
                                "vector containing lte-rrc.measId",
                                subret.size(),
                                1, 1, job
                            );
                    }
                }
            } else {
                warning_message +=
                    generate_vector_size_unexpected_message(
                        timestamp,
                        "vector containing lte-rrc.reportConfigId",
                        ret.size(),
                        1, 1, job
                    );
            }
        }

        if_unlikely (added_measure_id_nodes.size() != measure_id_nodes.size()) {
            throw_vector_size_unequal(
                "added_measure_id_nodes", "measure_id_nodes",
                added_measure_id_nodes.size(),
                measure_id_nodes.size(),
                job
            );
        }
        for (auto i = 0; i < added_measure_id_nodes.size(); ++i) {
            added_measure_ids.push_back(
                measure_id_nodes[i]->get<std::string>("<xmlattr>.showname")
            );
            report_to_measure_ids.push_back(
                added_measure_id_nodes[i]
                    ->get<std::string>("<xmlattr>.showname")
            );
        }
    }

    // Extract removal of mapping betwee report config IDs to measurement IDs.
    // <field name="lte-rrc.measIdToRemoveList" ... >
    //     ...
    //         ... <field name="lte-rrc.MeasId"
    //                    showname="MeasId: X" ... />
    //         ...
    //     ...
    // </field>
    std::vector<std::string> removed_measure_ids;
    {
        auto &&measure_id_to_remove_nodes = locate_subtree_with_attribute(
            tree, "name", "lte-rrc.measIdToRemoveList"
        );
        std::vector<const pt::ptree*> removed_measure_id_nodes;
        for (auto ptr : measure_id_to_remove_nodes) {
            auto &&ret = locate_subtree_with_attribute(
                *ptr, "name", "lte-rrc.MeasId"
            );
            removed_measure_id_nodes.insert(
                removed_measure_id_nodes.end(), ret.begin(), ret.end()
            );
        }
        for (auto i = 0; i < removed_measure_id_nodes.size(); ++i) {
            removed_measure_ids.push_back(
                removed_measure_id_nodes[i]
                    ->get<std::string>("<xmlattr>.showname")
            );
        }
    }

    // Extract the triggering measurement ID of the measurement report.
    // <field name="lte-rrc.measResults_element" ... >
    //     <field name="lte-rrc.measId"
    //            showname="measId: X" ... />
    //     ...
    // </field>
    std::vector<std::string> measurement_reports;
    {
        auto &&measurement_result_nodes = locate_subtree_with_attribute(
            tree, "name", "lte-rrc.measResults_element"
        );
        std::vector<const pt::ptree*> measurement_id_nodes;
        for (auto ptr : measurement_result_nodes) {
            auto &&ret = locate_subtree_with_attribute(
                *ptr, "name", "lte-rrc.measId"
            );
            measurement_id_nodes.insert(
                measurement_id_nodes.end(), ret.begin(), ret.end()
            );
        }
        for (auto ptr : measurement_id_nodes) {
            measurement_reports.push_back(
                ptr->get<std::string>("<xmlattr>.showname")
            );
        }
    }

    bool rrc_connection_reestablishment_request_present
        = is_subtree_with_attribute_present(
            tree, "showname", "rrcConnectionReestablishmentRequest"
    );

    bool rrc_connection_reestablishment_complete_present
        = is_subtree_with_attribute_present(
            tree, "showname", "rrcConnectionReestablishmentComplete"
    );
    std::string connection_reestablishment_cause;
    {
        auto &&causes = locate_subtree_with_attribute(
            tree, "name", "lte-rrc.reestablishmentCause"
        );
        for (auto p_cause : causes) {
            if (!connection_reestablishment_cause.empty()) {
                connection_reestablishment_cause += ", ";
            }
            connection_reestablishment_cause +=
                p_cause->get("<xmlattr>.showname", std::string());
        }
    }

    bool rrc_connection_reestablishment_reject_present
        = is_subtree_with_attribute_present(
            tree, "showname", "rrcConnectionReestablishmentReject"
    );

    bool rrc_connection_reconfiguration_present = false;
    bool mobility_control_info_present = false;
    std::string target_cells;
    {
        auto &&reconf_nodes = locate_subtree_with_attribute(
            tree, "showname", "rrcConnectionReconfiguration"
        );
        if (!reconf_nodes.empty()) {
            rrc_connection_reconfiguration_present = true;
            for (auto p_reconf_node : reconf_nodes) {
                if (is_subtree_with_attribute_present(
                    *p_reconf_node, "showname", "mobilityControlInfo"
                )) {
                    mobility_control_info_present = true;
                    break;
                }
            }
        }
    }
    if (mobility_control_info_present) {
        auto &&target_physic_cells = locate_subtree_with_attribute(
            tree, "name", "lte-rrc.targetPhysCellId"
        );
        for (auto ptr : target_physic_cells) {
            if (!target_cells.empty()) {
                target_cells += ", ";
            }
            target_cells += ptr->get<std::string>("<xmlattr>.showname");
        }
    }

    bool rrc_connection_reconfiguration_complete_present
        = is_subtree_with_attribute_present(
            tree, "showname", "rrcConnectionReconfigurationComplete"
    );

    bool rrc_connection_release_present
        = is_subtree_with_attribute_present(
            tree, "showname", "rrcConnectionRelease"
    );

    bool rrc_connection_request_present
        = is_subtree_with_attribute_present(
            tree, "showname", "rrcConnectionRequest"
    );

    bool rrc_connection_setup_present
        = is_subtree_with_attribute_present(
            tree, "showname", "rrcConnectionSetup"
    );

    bool rrc_connection_reject_present
        = is_subtree_with_attribute_present(
            tree, "showname", "rrcConnectionReject"
    );

    // Send the closure to ordered task executor to print extracted
    // information out.
    insert_ordered_task(
        job.job_num,
        [timestamp = std::move(timestamp),
         added_config_ids = std::move(added_config_ids),
         added_event_types = std::move(added_event_types),
         removed_config_ids = std::move(removed_config_ids),
         added_measure_ids = std::move(added_measure_ids),
         report_to_measure_ids = std::move(report_to_measure_ids),
         removed_measure_ids = std::move(removed_measure_ids),
         measurement_reports = std::move(measurement_reports),
         warning_message = std::move(warning_message),
         rrc_connection_reestablishment_request_present,
         rrc_connection_reestablishment_complete_present,
         rrc_connection_reestablishment_reject_present,
         rrc_connection_reconfiguration_present,
         mobility_control_info_present,
         target_cells = std::move(target_cells),
         connection_reestablishment_cause
            = std::move(connection_reestablishment_cause),
         rrc_connection_reconfiguration_complete_present,
         rrc_connection_release_present,
         rrc_connection_request_present,
         rrc_connection_setup_present,
         rrc_connection_reject_present] {
            auto print_last_data_pdcp_packet_timestamp = [] {
            (*g_output) << "LastPDCPPacketTimestamp: "
                        << g_last_pdcp_packet_timestamp
                        << ", Direction: ";
                if (g_last_pdcp_packet_direction == PDCPDirection::Downlink) {
                    (*g_output) << "downlink";
                } else if (g_last_pdcp_packet_direction
                            == PDCPDirection::Uplink) {
                    (*g_output) << "uplink";
                } else {
                    (*g_output) << "unknown";
                }
            };

            auto set_connection_disruption =
                [] (DisruptionEventEnum event_type) {
                g_distuption_events.is_being_disrupted = true;
                g_distuption_events.disruptions[
                    static_cast<int>(event_type)
                ] = true;
            };

            std::cerr << warning_message;
            for (auto &i : removed_config_ids) {
                (*g_output) << timestamp << " $ reportConfigToRemoveList $ "
                            << i << std::endl;
            }
            for (auto &i : removed_measure_ids) {
                (*g_output) << timestamp << " $ measIdToRemoveList $ "
                            << i << std::endl;
            }
            for (auto i = 0; i < added_config_ids.size(); ++i) {
                (*g_output) << timestamp << " $ ReportConfigToAddMod $ "
                            << added_config_ids[i]
                            << ", " << added_event_types[i] << std::endl;
            }
            for (auto i = 0; i < added_measure_ids.size(); ++i) {
                (*g_output) << timestamp << " $ MeasIdToAddMod $ "
                            << added_measure_ids[i] << ", "
                            << report_to_measure_ids[i] << std::endl;
            }
            for (auto &i : measurement_reports) {
                (*g_output) << timestamp << " $ measResults $ "
                            << i << std::endl;
            }
            if (rrc_connection_reestablishment_request_present) {
                (*g_output) << timestamp
                            << " $ rrcConnectionReestablishmentRequest $ ";
                print_last_data_pdcp_packet_timestamp();
                set_connection_disruption(
                    DisruptionEventEnum::RRCConnectionReestablishmentRequest
                );;
                if (!connection_reestablishment_cause.empty()) {
                    (*g_output) << ", " << connection_reestablishment_cause;
                }
                (*g_output) << std::endl;
            }
            if (rrc_connection_reestablishment_complete_present) {
                (*g_output) << timestamp
                            << " $ rrcConnectionReestablishmentComplete $"
                            << std::endl;
                set_connection_disruption(
                    DisruptionEventEnum::RRCConnectionReestablishmentComplete
                );
            }
            if (rrc_connection_reestablishment_reject_present) {
                (*g_output) << timestamp
                            << " $ rrcConnectionReestablishmentReject $"
                            << std::endl;
            }
            if (rrc_connection_reconfiguration_present) {
                (*g_output) << timestamp << " $ rrcConnectionReconfiguration $"
                            << " mobilityControlInfo: ";
                if (mobility_control_info_present) {
                    (*g_output) << "1, "
                                << target_cells;
                } else {
                    (*g_output) << '0';
                }
                (*g_output) << ", ";
                print_last_data_pdcp_packet_timestamp();
                set_connection_disruption(
                    DisruptionEventEnum::RRCConnectionReconfiguration
                );
                (*g_output) << std::endl;
            }
            if (rrc_connection_reconfiguration_complete_present) {
                (*g_output) << timestamp
                            << " $ rrcConnectionReconfigurationComplete $"
                            << std::endl;
                set_connection_disruption(
                    DisruptionEventEnum::RRCConnectionReconfigurationComplete
                );
            }
            if (rrc_connection_release_present) {
                (*g_output) << timestamp << " $ rrcConnectionRelease $"
                            << std::endl;
            }
            if (rrc_connection_request_present) {
                (*g_output) << timestamp << " $ rrcConnectionRequest $ ";
                print_last_data_pdcp_packet_timestamp();
                set_connection_disruption(
                    DisruptionEventEnum::RRCConnectionRequest
                );
                (*g_output) << std::endl;
            }
            if (rrc_connection_setup_present) {
                (*g_output) << timestamp << " $ rrcConnectionSetup $"
                            << std::endl;
                set_connection_disruption(
                    DisruptionEventEnum::RRCConnectionSetup
                );
            }
            if (rrc_connection_reject_present) {
                (*g_output) << timestamp << " $ rrcConnectionReject $"
                            << std::endl;
            }
    });
}
