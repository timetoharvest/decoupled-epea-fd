#include "lexicographic_ordering.h" 

#include "../compliant_paths/compliant_path_graph.h"
#include "decoupled_permutation.h"

#include "../option_parser.h"


LexicographicOrdering::LexicographicOrdering(const Options & opts) : 
    apply_goal_cost (opts.get<bool>("lex_goal_cost")), 
    apply_num_states (opts.get<bool>("lex_num_states")), 
    apply_prices (opts.get<bool>("lex_prices")) {
}

bool LexicographicOrdering::produces_less(const LeavesPermutation & per, 
					  const CompliantPathGraph & p) const { 
    if(apply_goal_cost) {
	cmp_t cmp = per.cmp_goal_cost(p);
	if(cmp == cmp_t::LESS) return true;
	else if(cmp == cmp_t::GREATER) return false;
    }

    if(apply_num_states) {
	cmp_t cmp = per.cmp_num_states(p);
	if(cmp == cmp_t::LESS) return true;
	else if(cmp == cmp_t::GREATER) return false;
    }

    if(apply_prices) {
	cmp_t cmp = per.cmp_prices(p);
	if(cmp == cmp_t::LESS) return true;
	else if(cmp == cmp_t::GREATER) return false;
    }

    return false;
} 

void LexicographicOrdering::add_options_to_parser(OptionParser &parser) {
    parser.add_option<bool>("lex_goal_cost",
			    "Apply lexicographic leaves ordering based on goal cost",
			    "true");
    parser.add_option<bool>("lex_num_states",
			    "Apply lexicographic leaves ordering based on number of reached states",
			    "true");
    parser.add_option<bool>("lex_prices",
			    "Apply lexicographic leaves ordering based on prices",
			    "false");
}

