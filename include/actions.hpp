/* Copyright [2020] Zhiyao Ma */
#ifndef ACTIONS_HPP_
#define ACTIONS_HPP_
#include <boost/property_tree/ptree.hpp>
#include "extractor.hpp"

namespace pt = boost::property_tree;

extern std::string get_packet_type(const pt::ptree &tree);

extern bool is_packet_having_type(
    const pt::ptree &tree, const std::string type_id);

extern std::string get_packet_time_stamp(const pt::ptree &tree);

extern time_t timestamp_str2long(const std::string &timestamp);

extern time_t timestamp_str2long_microsec_hack(const std::string &timestamp);

extern bool is_tree_having_attribute(
    const pt::ptree &tree, const std::string &key, const std::string &val);

extern bool recursive_find_mobility_control_info(const pt::ptree &tree);

extern void print_time_of_mobility_control_info(pt::ptree &&tree, Job &&job);

extern void print_timestamp(pt::ptree &&tree, long seq_num);

extern std::vector<const pt::ptree*> locate_subtree_with_attribute(
    const pt::ptree &tree,
    const std::string &attribute_name,
    const std::string &attribute_value
);

extern std::vector<const pt::ptree*> locate_disjoint_subtree_with_attribute(
    const pt::ptree &tree,
    const std::string &attribute_name,
    const std::string &attribute_value
);

extern bool is_subtree_with_attribute_present(
    const pt::ptree &tree,
    const std::string &attribute_name,
    const std::string &attribute_value
);

extern void throw_vector_size_unequal(
    const std::string vec1_name, const std::string vec2_name,
    std::size_t vec1_size, std::size_t vec2_size, const Job &job);

extern std::string generate_vector_size_unexpected_message(
    const std::string &timestamp,
    const std::string &vec_name,
    std::size_t vec_size,
    std::size_t lower_limit,
    std::size_t upper_limit,
    const Job &job);

extern void extract_rrc_ota_packet(pt::ptree &&tree, Job &&job);

extern void extract_rrc_serv_cell_info_packet(pt::ptree &&tree, Job &&job);

extern void update_pdcp_cipher_data_pdu_packet_timestamp(
    pt::ptree &&tree, Job &&job);

extern void extract_pdcp_cipher_data_pdu_packet(pt::ptree &&tree, Job &&job);

extern void extract_nas_emm_ota_incoming_packet(pt::ptree &&tree, Job &&job);

extern void extract_nas_emm_ota_outgoing_packet(pt::ptree &&tree, Job &&job);

extern void extract_mac_rach_attempt_packet(pt::ptree &&tree, Job &&job);

extern void extract_lte_mac_rach_trigger_packet(pt::ptree &&tree, Job &&job);

extern void extract_phy_pdsch_stat_packet(pt::ptree &&tree, Job &&job);

extern void extract_phy_pdsch_packet(pt::ptree &&tree, Job &&job);

extern void extract_phy_serv_cell_measurement(pt::ptree &&tree, Job &&job);

extern void echo_packet_within_time_range(pt::ptree &&tree, Job &&job);

extern void extract_packet_type(pt::ptree &&tree, Job &&job);

extern void extract_rlc_dl_am_all_pdu(pt::ptree &&tree, Job &&job);

extern void extract_rlc_ul_am_all_pdu(pt::ptree &&tree, Job &&job);

extern void echo_packet_if_new(pt::ptree &&tree, Job &&job);

extern void update_reorder_window(pt::ptree &&tree, Job &&job);

extern void echo_packet_if_match(pt::ptree &&tree, Job &&job);

extern void extract_rlc_dl_config_log_packet(pt::ptree &&tree, Job &&job);

extern void extract_rlc_ul_config_log_packet(pt::ptree &&tree, Job &&job);

#endif  // ACTIONS_HPP_
