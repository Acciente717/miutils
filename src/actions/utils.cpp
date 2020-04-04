/* Copyright [2020] Zhiyao Ma */
#include "actions.hpp"
#include "exceptions.hpp"

/// Return the `type_id` field in the packet.
std::string get_packet_type(const pt::ptree &tree) {
    for (const auto &i : tree.get_child("dm_log_packet")) {
        if (i.first == "pair") {
            if (i.second.get("<xmlattr>.key", std::string()) == "type_id") {
                return i.second.data();
            }
        }
    }
    return "";
}

/// Return true if and only if the tree has the following structure:
/// <dm_log_packet>
///     ...
///     <pair key="type_id">$type_id</pair>
///     ...
/// <dm_log_packet>
bool is_packet_having_type(const pt::ptree &tree, const std::string type_id) {
    if (get_packet_type(tree) == type_id) {
        return true;
    }
    return false;
}

/// Find and return the timestamp locating at
/// <dm_log_packet>
///     ...
///     <pair key="timestamp"> TIMESTAMP </pair>
///     ...
/// </dm_log_packet>
std::string get_packet_time_stamp(const pt::ptree &tree) {
    std::string timestamp = "timestamp N/A";
    for (const auto &i : tree.get_child("dm_log_packet")) {
        if (i.first == "pair") {
            if (i.second.get<std::string>("<xmlattr>.key") == "timestamp") {
                timestamp = i.second.data();
                break;
            }
        }
    }
    return timestamp;
}

/// Convert the timestamp string to a long integer under
/// timezone UTC+8.
time_t timestamp_str2long(const std::string &timestamp) {
    tm s;
    memset(&s, 0, sizeof(s));
    auto cnt = sscanf(timestamp.c_str(), "%d-%d-%d %d:%d:%d.%*d",
                      &s.tm_year, &s.tm_mon, &s.tm_mday,
                      &s.tm_hour, &s.tm_min, &s.tm_sec);
    if (cnt != 6) {
        return static_cast<time_t>(-1);
    }
    s.tm_year -= 1900;
    s.tm_mon -= 1;
    return mktime(&s) + 28800;
}

/// Convert the timestamp string to a long integer under
/// timezone UTC+8. The returned value contains the microsecond part.
time_t timestamp_str2long_microsec_hack(const std::string &timestamp) {
    tm s;
    int mircosec;
    memset(&s, 0, sizeof(s));
    auto cnt = sscanf(timestamp.c_str(), "%d-%d-%d %d:%d:%d.%d",
                      &s.tm_year, &s.tm_mon, &s.tm_mday,
                      &s.tm_hour, &s.tm_min, &s.tm_sec, &mircosec);
    if (cnt == 6) {
        cnt = sscanf(timestamp.c_str(), "%d-%d-%d %d:%d:%d",
                     &s.tm_year, &s.tm_mon, &s.tm_mday,
                     &s.tm_hour, &s.tm_min, &s.tm_sec);
        mircosec = 0;
    } else if (cnt != 7) {
        return static_cast<time_t>(-1);
    }
    s.tm_year -= 1900;
    s.tm_mon -= 1;
    return ((mktime(&s) + 28800) * 1000000) + mircosec;
}

/// Check whether the tree has an attribute as its direct child.
bool is_tree_having_attribute(
    const pt::ptree &tree, const std::string &key, const std::string &val) {
    auto &&attributes = tree.get_child_optional("<xmlattr>");

    // If the root XML tree has no attribute.
    if (!attributes) {
        return false;
    }

    // Scan through the attributes to see if there is any key:val pair.
    for (auto attribute : *attributes) {
        if (attribute.first == key
            && attribute.second.get_value(std::string()) == val) {
            return true;
        }
    }

    return false;
}

/// Start from the root `tree`, recursively find the following subtrees:
/// <some_tag attribute_name=attribute_value ... >
///     ...
/// </some_tag>
/// Return the pointers to the roots of the subtrees, i.e. all the subtrees
/// starting at `some_tag` with the attribute name `attribute_name` and
/// attribute value `attribute_value`.
///
/// This function *does not* guarantee that all returned trees are disjoint,
/// i.e. a returned node might be the decendent of another returned node.
std::vector<const pt::ptree*> locate_subtree_with_attribute(
    const pt::ptree &tree,
    const std::string &attribute_name,
    const std::string &attribute_value
) {
    std::vector<const pt::ptree*> subtrees;
    for (auto &i : tree) {
        if (i.first == "<xmlattr>") {
            for (auto &j : i.second) {
                if (j.first == attribute_name
                    && j.second.data() == attribute_value) {
                    subtrees.push_back(&tree);
                }
            }
        } else {
            auto &&ret = locate_subtree_with_attribute(
                i.second, attribute_name, attribute_value
            );
            subtrees.insert(subtrees.end(), ret.begin(), ret.end());
        }
    }
    return subtrees;
}

/// Start from the root `tree`, recursively find the following subtrees:
/// <some_tag attribute_name=attribute_value ... >
///     ...
/// </some_tag>
/// Return the pointers to the roots of the subtrees, i.e. all the subtrees
/// starting at `some_tag` with the attribute name `attribute_name` and
/// attribute value `attribute_value`.
///
/// This function guarantees that all returned trees are disjoint, i.e.
/// no node is the decendent of any other returned nodes.
std::vector<const pt::ptree*> locate_disjoint_subtree_with_attribute(
    const pt::ptree &tree,
    const std::string &attribute_name,
    const std::string &attribute_value
) {
    std::vector<const pt::ptree*> subtrees;
    bool hit = false;
    for (auto &i : tree) {
        if (i.first == "<xmlattr>") {
            for (auto &j : i.second) {
                if (j.first == attribute_name
                    && j.second.data() == attribute_value) {
                    subtrees.push_back(&tree);
                    hit = true;
                    break;
                }
            }
            if (hit) break;
        }
    }
    if (!hit) {
        for (auto &i : tree) {
            auto &&ret = locate_disjoint_subtree_with_attribute(
                i.second, attribute_name, attribute_value);
            subtrees.insert(subtrees.end(), ret.begin(), ret.end());
        }
    }
    return subtrees;
}

/// Start from the root `tree`, recursively find the following subtrees:
/// <some_tag attribute_name=attribute_value ... >
///     ...
/// </some_tag>
/// Return true if at least one such subtree exists, otherwise false.
bool is_subtree_with_attribute_present(
    const pt::ptree &tree,
    const std::string &attribute_name,
    const std::string &attribute_value
) {
    for (auto &i : tree) {
        if (i.first == "<xmlattr>") {
            for (auto &j : i.second) {
                if (j.first == attribute_name
                    && j.second.data() == attribute_value) {
                    return true;
                }
            }
        } else {
            if (is_subtree_with_attribute_present(
                i.second, attribute_name, attribute_value
            )) {
                return true;
            }
        }
    }
    return false;
}

void throw_vector_size_unequal(
    const std::string vec1_name,
    const std::string vec2_name,
    std::size_t vec1_size,
    std::size_t vec2_size,
    const Job &job) {
    throw ProgramBug(
        vec1_name + " and " + vec2_name + " have unequal size.\n"
        + vec1_name + " has size " + std::to_string(vec1_size)
        + ", whlie " + vec2_name + " has size " + std::to_string(vec2_size)
        + ".\nInput file \"" + job.file_name + "\" at line "
        + std::to_string(job.start_line_number)
        + "-" + std::to_string(job.end_line_number)
    );
}

std::string generate_vector_size_unexpected_message(
    const std::string &timestamp,
    const std::string &vec_name,
    std::size_t vec_size,
    std::size_t lower_limit,
    std::size_t upper_limit,
    const Job &job) {
    return
        "Warning (packet timestamp = " + timestamp + "): \n"
        + vec_name + " has unexpected size " + std::to_string(vec_size)
        + "\nExpected range: [" + std::to_string(lower_limit)
        + "," + std::to_string(upper_limit) + "] (inclusive)"
        + ".\nInput file \"" + job.file_name + "\" at line "
        + std::to_string(job.start_line_number)
        + "-" + std::to_string(job.end_line_number) + "\n";
}
