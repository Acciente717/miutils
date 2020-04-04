/* Copyright [2019] Zhiyao Ma */
#ifndef SPLITTER_HPP_
#define SPLITTER_HPP_

#include <string>

/// Provide the input file name and start running the lexical splitter.
extern void start_splitter();

/// Join the thread running the lexical splitter.
extern void join_splitter();

/// Prematurely stop the lexical splitter. It does NOT join the thread.
/// One should call join_splitter() after calling this function.
extern void kill_splitter();

/// Get the next subtree in the opened XML file. The returned string is a
/// slice of the input XML file in the form like
/// "<$top_level_tag> ... </$top_level_tag>". Note that since the splitter
/// is only running on the lexical level, rather than the grammar level,
/// it assumes that the input file is in valid XML format. It defers the
/// validation of the format to the following modules, where an exception
/// will be raised if the returned string is malformated because the input
/// is not a valid XML file.
extern std::string next_ptree_string();

#endif  // SPLITTER_HPP_
