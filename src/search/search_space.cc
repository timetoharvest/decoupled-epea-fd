#include "search_space.h"

#include "factoring.h"
#include "globals.h"
#include "operator.h"
#include "compliant_paths/cpg_storage.h"
#include "compliant_paths/path_price_tag.h"
#include "search_node_info.h"
#include "state.h"
#include "symmetries/graph_creator.h"
#include "symmetries/permutation.h"

#include <cassert>
#include <limits>

using namespace std;


SearchNode::SearchNode(StateID state_id_, SearchNodeInfo &info_,
                       OperatorCost cost_type_)
    : state_id(state_id_), info(info_), cost_type(cost_type_) {
    assert(state_id != StateID::no_state);
}

State SearchNode::get_state() const {
    return g_state_registry->lookup_state(state_id);
}

bool SearchNode::is_open() const {
    return info.status == SearchNodeInfo::OPEN;
}

bool SearchNode::is_closed() const {
    return info.status == SearchNodeInfo::CLOSED;
}

bool SearchNode::is_dead_end() const {
    return info.status == SearchNodeInfo::DEAD_END;
}

bool SearchNode::is_new() const {
    return info.status == SearchNodeInfo::NEW;
}

int SearchNode::get_g() const {
    return info.g;
}

int SearchNode::get_real_g() const {
    return info.real_g;
}

int SearchNode::get_leaf_part_g() const {
    return info.leaf_part_g;
}

int SearchNode::get_h() const {
    return info.h;
}

StateID SearchNode::get_parent_state_id() const {
    return info.parent_state_id;
}

bool SearchNode::is_h_dirty() const {
    return info.h_is_dirty;
}

void SearchNode::set_h_dirty() {
    info.h_is_dirty = true;
}

void SearchNode::clear_h_dirty() {
    info.h_is_dirty = false;
}

void SearchNode::open_initial(int h) {
    assert(info.status == SearchNodeInfo::NEW);
    info.status = SearchNodeInfo::OPEN;
    info.g = 0;
    info.leaf_part_g = 0;
    info.real_g = 0;
    info.h = h;
    info.parent_state_id = StateID::no_state;
    info.creating_operator = 0;
}

void SearchNode::open(int h, const SearchNode &parent_node,
                      const Operator *parent_op) {
    assert(info.status == SearchNodeInfo::NEW);
    info.status = SearchNodeInfo::OPEN;
    info.g = parent_node.info.g + get_adjusted_action_cost(*parent_op, cost_type) + g_inc_g_by;
    info.leaf_part_g = parent_node.info.leaf_part_g + g_inc_g_by;
    info.real_g = parent_node.info.real_g + parent_op->get_cost() + g_inc_g_by;// HACK
    info.h = h;
    info.parent_state_id = parent_node.get_state_id();
    info.creating_operator = parent_op;
    g_inc_g_by = 0;
}

void SearchNode::reopen(const SearchNode &parent_node,
                        const Operator *parent_op) {
    assert(info.status == SearchNodeInfo::OPEN ||
           info.status == SearchNodeInfo::CLOSED);

    // The latter possibility is for inconsistent heuristics, which
    // may require reopening closed nodes.
    info.status = SearchNodeInfo::OPEN;
    info.g = parent_node.info.g + get_adjusted_action_cost(*parent_op, cost_type) + g_inc_g_by;
    info.leaf_part_g = parent_node.info.leaf_part_g + g_inc_g_by;
    info.real_g = parent_node.info.real_g + parent_op->get_cost() + g_inc_g_by;// HACK
    info.parent_state_id = parent_node.get_state_id();
    info.creating_operator = parent_op;
    g_inc_g_by = 0;
}

// like reopen, except doesn't change status
void SearchNode::update_parent(const SearchNode &parent_node,
                               const Operator *parent_op) {
    assert(info.status == SearchNodeInfo::OPEN ||
           info.status == SearchNodeInfo::CLOSED);
    // The latter possibility is for inconsistent heuristics, which
    // may require reopening closed nodes.
    info.g = parent_node.info.g + get_adjusted_action_cost(*parent_op, cost_type) + g_inc_g_by;
    info.leaf_part_g = parent_node.info.leaf_part_g + g_inc_g_by;
    info.real_g = parent_node.info.real_g + parent_op->get_cost() + g_inc_g_by;// HACK
    info.parent_state_id = parent_node.get_state_id();
    info.creating_operator = parent_op;
    g_inc_g_by = 0;
}

void SearchNode::increase_h(int h) {
    assert(h >= info.h);
    info.h = h;
}

void SearchNode::close() {
    assert(info.status == SearchNodeInfo::OPEN);
    info.status = SearchNodeInfo::CLOSED;
}

void SearchNode::mark_as_dead_end() {
    info.status = SearchNodeInfo::DEAD_END;
}

void SearchNode::dump() const {
    cout << state_id << ": ";
    cout << "g = " << info.g << " h = " << info.h << endl;
    g_state_registry->lookup_state(state_id).dump_pddl();
    if (info.creating_operator) {
        cout << " created by " << info.creating_operator->get_name()
             << " from " << info.parent_state_id << endl;
    } else {
        cout << " no parent" << endl;
    }
}

SearchSpace::SearchSpace(OperatorCost cost_type_)
    : cost_type(cost_type_) {
    if (g_use_decoupled_search){
        g_factoring->set_search_space(this);
    }
}

bool SearchSpace::is_equal(const State& s1, const CompliantPathGraph& cpg1, const State& s2, const CompliantPathGraph& cpg2) const {
    for (int var : g_center_vars){
        if (s1[var] !=  s2[var]){
            return false;
        }
    }
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        for (size_t id = 0; id < g_state_registry->size(factor); ++id){
            bool has_state = cpg1.has_leaf_state(id, factor);
            if (has_state ^ cpg2.has_leaf_state(id, factor)){
                return false;
            }
            if (has_state && cpg1.get_cost_of_state(id, factor) != cpg2.get_cost_of_state(id, factor)){
                return false;
            }
        }
    }
    
    return true;
}

SearchNode SearchSpace::get_node(const State &state) {
    return SearchNode(state.get_id(), search_node_infos[state], cost_type);
}

void SearchSpace::trace_path(const State &goal_state,
                             vector<const Operator *> &path) const {
                                 
    State current_state = goal_state;
    
    vector<Permutation> permutations; // needed for orbit search
    vector<StateID> states; // needed for decoupled search
    
    assert(path.empty());
    
    for (;;) {          // backtrace solution path
        const SearchNodeInfo &info = search_node_infos[current_state];
        const Operator *op = info.creating_operator;
        
        if (g_use_decoupled_search || g_symmetry_graph){
            states.push_back(current_state.get_id());
        }
        
        if (g_symmetry_graph && !g_use_decoupled_search){
            State parent_state = g_initial_state();
            State new_state = g_initial_state();
            if (op != 0) {
                parent_state = g_state_registry->lookup_state(info.parent_state_id);
                new_state = g_state_registry->get_successor_state(parent_state, *op, false);
            } else {
                new_state = g_state_registry->get_state(g_initial_state_data);;
            }
            Permutation p;
            if (new_state.get_id() != current_state.get_id()){
                p = g_symmetry_graph->create_permutation_from_state_to_state(current_state, new_state);
            }
            permutations.push_back(p);
        }

        if (op == 0) {  // reached initial state => done
            assert(info.parent_state_id == StateID::no_state);
            break;
        }

        if (!g_symmetry_graph || g_use_decoupled_search){
            path.push_back(op);
        }
        current_state = g_state_registry->lookup_state(info.parent_state_id);
    }
    
    reverse(path.begin(), path.end());
    
    if (g_symmetry_graph && !g_use_decoupled_search){
        vector<Permutation> reverse_permutations;
        Permutation temp_p, p;
        while (permutations.begin() != permutations.end()) {
            p = permutations.back();
            temp_p = Permutation(p, temp_p);
            reverse_permutations.push_back(temp_p);
            permutations.pop_back();
        }
        for(size_t i = 0; i < states.size(); ++i){
            Permutation &permutation = reverse_permutations[states.size() - i - 1];
            states[i] = g_state_registry->get_state_permutation(g_state_registry->lookup_state(states[i]), permutation).get_id();
        }
        for (size_t i = states.size() - 1; i > 0; i--) {
            vector<const Operator *> applicable_ops;
            g_successor_generator->generate_applicable_ops(g_state_registry->lookup_state(states[i]), applicable_ops);
            bool found = false;
            int min_cost_op = 0;
            int min_cost = numeric_limits<int>::max();

            for (size_t o = 0; o < applicable_ops.size(); ++o) {
                const Operator *op = applicable_ops[o];
                State succ_state = g_state_registry->get_successor_state(g_state_registry->lookup_state(states[i]), *op, false);
                if (succ_state.get_id() == states[i-1]) {
                    found = true;
                    if (op->get_cost() < min_cost) {
                        min_cost = op->get_cost();
                        min_cost_op = o;
                    }
                }
            }
            if (!found) {
                cout << "No operator is found!!!" << endl
                    << "Cannot reach the state " << endl;
                g_state_registry->lookup_state(states[i-1]).dump_pddl();
                cout << endl << "From the state" << endl;
                g_state_registry->lookup_state(states[i]).dump_pddl();
                exit_with(EXIT_CRITICAL_ERROR);
            }
            path.push_back(applicable_ops[min_cost_op]);
        }
        return;
    }
    
    if (g_use_decoupled_search){
#ifdef DEBUG_PLAN_EXTRACTION
        cout << "##################### STARTING TRACE_PATH" << endl;
#endif
        
        reverse(states.begin(), states.end());
        
        vector<PathPriceTag> price_tags(states.size(), PathPriceTag(0));
        
        if (g_symmetry_graph){
            
            if (g_factoring->get_profile() != FORK){
                // TODO need to handle center actions that touch leaves correctly!
                return;
            }
            
            vector<int> init_center(g_variable_domain.size(), -1);
            for (int var : g_center_vars){
                init_center[var] = g_initial_state_data[var];
            }
            State real_state = g_state_registry->get_center_state(init_center);
            states[0] = real_state.get_id();
            
            for (size_t factor = 0; factor < g_factors.size(); ++factor){
                price_tags[0].add_state(LeafStateID(0, factor), 0, 0, LeafStateID::no_state);
            }
            price_tags[0].update_price_tags(real_state);
            
            State permuted_state = g_state_registry->get_initial_state();
            PathPriceTag permuted_cpg(&price_tags[0]);
            
            unique_ptr<DecoupledPermutation> overall_permutation(new DecoupledPermutation());
            for (size_t i = 0; i < states.size(); ++i){
                if (i > 0){
                    real_state = g_state_registry->get_center_successor(permuted_state, *path[i-1]);
                    permuted_state = g_state_registry->lookup_state(states[i]);
                    permuted_cpg.update_price_tags(permuted_state);
                }
                
                PathPriceTag tmp(&permuted_cpg);
                const vector<int> &permuted_center_facts = g_symmetry_graph->get_canonical_decoupled_state(real_state, tmp);
                const State permuted_center = g_state_registry->get_center_state(permuted_center_facts);
                
#ifndef NDEBUG
//                 permuted_center.dump_pddl();
//                 permuted_state.dump_pddl();
                for (int var : g_center_vars){
                    assert(permuted_center[var] == permuted_state[var]);
                }
#endif
                
                if (!is_equal(permuted_center, tmp, real_state, permuted_cpg)){
                    DecoupledPermutation p(g_symmetry_graph->create_permutation_from_state_to_state(permuted_center, tmp, real_state, permuted_cpg));
                    overall_permutation.reset(new DecoupledPermutation(*overall_permutation, p));
                    
                    if (i > 0){
                        states[i] = g_state_registry->get_state_permutation(g_state_registry->lookup_state(states[i]), p).get_id();
                        price_tags[i] = PathPriceTag(&price_tags[i-1]);
                        price_tags[i].update_price_tags(g_state_registry->lookup_state(states[i]));
                    }
                } else {
                    if (i > 0){
                        price_tags[i] = PathPriceTag(&price_tags[i-1]);
                        price_tags[i].update_price_tags(g_state_registry->lookup_state(states[i]));
                    }
                }
                
//                 cout << "_____________________________________________" << endl;
//                 g_state_registry->lookup_state(states[i]).dump_pddl();
//                 price_tags[i].dump();
//                 cout << "_____________________________________________" << endl;
                
                if (i + 1 < states.size()){
                    states[i+1] = g_state_registry->get_decoupled_state_permutation(g_state_registry->lookup_state(states[i+1]), permuted_cpg, *overall_permutation).get_id();
                }
                
                if (i > 0){
                    State predecessor = g_state_registry->lookup_state(states[i-1]);
                    State current = g_state_registry->lookup_state(states[i]);
                    
                    vector<const Operator*> applicable_ops;
                    g_successor_generator->generate_applicable_ops(predecessor, applicable_ops, price_tags[i-1]);
                    bool found = false;
                    int min_cost_op = 0;
                    int min_cost = numeric_limits<int>::max();

                    for (size_t o = 0; o < applicable_ops.size(); ++o) {
                        const Operator *op = applicable_ops[o];
                        State succ_state = g_state_registry->get_center_successor(predecessor, *op);
                        PathPriceTag succ_cpg(&price_tags[i-1]);
                        succ_cpg.update_price_tags(succ_state);
                        if (is_equal(succ_state, succ_cpg, current, price_tags[i])) {
                            found = true;
                            if (op->get_cost() < min_cost) {
                                min_cost = op->get_cost();
                                min_cost_op = o;
                            }
                        }
                    }
                    if (!found) {
                        cout << "No operator is found!!!" << endl
                            << "Cannot reach the state " << endl;
                        current.dump_pddl();
                        cout << endl << "From the state" << endl;
                        predecessor.dump_pddl();
                        exit_with(EXIT_CRITICAL_ERROR);
                    }
                    path[i-1] = applicable_ops[min_cost_op];
                }
            }
        } else {
        
            // reconstruct leaf paths
            
            for (size_t factor = 0; factor < g_factors.size(); ++factor){
                price_tags[0].add_state(LeafStateID(0, factor), 0, 0, LeafStateID::no_state);
            }
            price_tags[0].update_price_tags(g_state_registry->lookup_state(states[0]));
            
            for (size_t i = 1; i < states.size(); ++i){
                PathPriceTag &tag = price_tags[i];
                
                if (g_factoring->get_profile() != FORK){
                    tag.apply_center_op_to_leaves(price_tags[i - 1], *path[i - 1]);
                } else {
                    tag = PathPriceTag(&price_tags[i - 1]);
                }
                tag.update_price_tags(g_state_registry->lookup_state(states[i]));
            }
        }

#ifdef DEBUG_PLAN_EXTRACTION
        cout << "done reconstructing leaf paths" << endl;
#endif
        
        // start from goal state
        reverse(path.begin(), path.end());
        reverse(price_tags.begin(), price_tags.end());
        
#ifdef DEBUG_PLAN_EXTRACTION
        reverse(states.begin(), states.end());
#endif        
        
        vector<set<int> > marked_leaf_states(g_factors.size());       
        
        // mark leaf goal states
        for (size_t factor = 0; factor < g_factors.size(); ++factor){
            if (!g_goals_per_factor[factor].empty()){
                int min_cost = numeric_limits<int>::max();
                int best_leaf_state_id = -1;
                
                size_t number_states = price_tags[0].get_number_states(factor);
                size_t id = 0;
                while (number_states > 0) {
                    if (price_tags[0].has_leaf_state(id, factor)){
                        --number_states;
                        if (CompliantPathGraph::is_leaf_goal_state(LeafStateID(id, factor))){
                            int new_cost = price_tags[0].get_cost_of_state(id, factor);
                            if (min_cost > new_cost){
                                min_cost = new_cost;
                                best_leaf_state_id = id;
                                if (g_factoring->get_search_type() == SAT || g_factoring->get_search_type() == UNSAT){
                                    break;
                                }
                            }
                        }
                    }
                    ++id;
                }
                assert(best_leaf_state_id != -1);
                marked_leaf_states[factor].insert(best_leaf_state_id);

#ifdef DEBUG_PLAN_EXTRACTION
                cout << "marked goal fact: " << best_leaf_state_id << " for cost " << min_cost << endl;
                g_state_registry->lookup_leaf_state(LeafStateID(best_leaf_state_id, factor)).dump_pddl();
#endif
            }
        }
        
#ifdef DEBUG_PLAN_EXTRACTION        
        cout << "start backtracing plan" << endl;
#endif
        
        vector<const Operator*> decoupled_plan;
        
        vector<set<int> > center_preconditions(g_factors.size());
        vector<int> best_center_supporter(g_factors.size());
        
        // go through center path and fill in leaf actions
        for (size_t step = 0; step < price_tags.size(); step++){
        
            const Operator *op = 0;
            if (step < path.size()){
                op = path[step];
            }
            
            // collect leaf preconditions of center op
            vector<vector<pair<int, int> > > leaf_pre_by_factor;
            if (g_factoring->get_profile() != FORK && step < path.size()){
                leaf_pre_by_factor.resize(g_factors.size());
                for (size_t pre = 0; pre < op->get_preconditions().size(); pre++){
                    const Condition &prev = op->get_preconditions()[pre];
                    int factor = g_belongs_to_factor[prev.var];
                    if (factor != -1){
                        leaf_pre_by_factor[factor].push_back(make_pair(prev.var, prev.val));
#ifdef DEBUG_PLAN_EXTRACTION
                        cout << "added center precondition " << g_fact_names[prev.var][prev.val] << endl;
#endif
                    }
                }
            }
            
            
#ifdef DEBUG_PLAN_EXTRACTION
            cout << "current root state: " << states[step] << endl;
            g_state_registry->lookup_state(states[step]).dump_pddl();
            for (size_t factor = 0; factor < g_factors.size(); ++factor){
                for (size_t i = 0; i < g_state_registry->size(factor); ++i){
                    if (price_tags[step].has_leaf_state(i, factor)){
                        if (price_tags[step].get_path_info_for_fact(i, factor).is_new){
                            cout << "is new:" << endl;
                        }
                        g_state_registry->lookup_leaf_state(LeafStateID(i, factor)).dump_pddl();
                    }
                }
            }
            cout << endl;
#endif      
            
            // iterate over leaf factors
            for (size_t factor = 0; factor < g_factors.size(); ++factor){
                
                bool change = true;
                while (change){
                    change = false;

                    size_t number_states = price_tags[step].get_number_states(factor);
                    size_t id = 0;
                    while (number_states > 0){          // iterate over all new leaf states
                        if (price_tags[step].has_leaf_state(id, factor)){
                            --number_states;
                            if (price_tags[step].get_path_info_for_fact(id, factor).is_new){
                        
                                const PathPriceTagInfo &fact = price_tags[step].get_path_info_for_fact(id, factor);

                                if (marked_leaf_states[factor].erase(id) > 0){
                                    if (center_preconditions[factor].find(id) != center_preconditions[factor].end()){
                                        // if a leaf precondition fulfills the center precondition => center pre is achieved
                                        center_preconditions[factor] = set<int>();
#ifdef DEBUG_PLAN_EXTRACTION
                                        cout << "center precondition fulfilled" << endl;
#endif
                                    }
#ifdef DEBUG_PLAN_EXTRACTION
                                    cout << "handling fact " << id << ":" << endl;
                                    g_state_registry->lookup_leaf_state(LeafStateID(id, factor)).dump_pddl();
                                    cout << "added op to plan: ";
                                    if (g_factoring->get_profile() == FORK){
                                        if (fact.best_supporter != 0){
                                            cout << fact.best_supporter->get_name() << endl;
                                            cout << "marked predecessor: " << fact.predecessor << endl;
                                            g_state_registry->lookup_leaf_state(fact.predecessor).dump_pddl();
                                            cout << "predecessor in this pricetag? => " << (price_tags[step].get_cost_of_state(fact.predecessor.hash(), factor) != -1) << endl;
                                        } else {
                                            cout << "is init state" << endl;
                                        }
                                    } else if (fact.best_supporter == 0) {
                                        if (fact.predecessor != LeafStateID::no_state){
                                            cout << "no-op (affects center)" << endl;
                                            cout << "marked predecessor: " << fact.predecessor << " in predecessor pricetag" << endl;
                                            g_state_registry->lookup_leaf_state(fact.predecessor).dump_pddl();
                                            cout << "predecessor in this pricetag? => " << (price_tags[step].get_cost_of_state(fact.predecessor.hash(), factor) != -1) << endl;
                                            cout << "predecessor in predecessor pricetag? => " << (price_tags[step+1].get_cost_of_state(fact.predecessor.hash(), factor) != -1) << endl;
                                        }
                                    } else {
                                        cout << fact.best_supporter->get_name() << endl;
                                        cout << "marked predecessor: " << fact.predecessor << endl;
                                        g_state_registry->lookup_leaf_state(fact.predecessor).dump_pddl();
                                        cout << "predecessor in this pricetag? => " << (price_tags[step].get_cost_of_state(fact.predecessor.hash(), factor) != -1) << endl;
                                    }
#endif

                                    if (fact.best_supporter != 0){
                                        marked_leaf_states[factor].insert(fact.predecessor.hash());
                                        decoupled_plan.push_back(fact.best_supporter);
                                        if (center_preconditions[factor].find(fact.predecessor.hash()) != center_preconditions[factor].end()){
                                            // if a leaf precondition fulfills the center precondition => center pre is achieved
                                            center_preconditions[factor] = set<int>();
#ifdef DEBUG_PLAN_EXTRACTION
                                            cout << "center precondition fulfilled" << endl;
#endif
                                        }
                                        change = true;
                                    } else if (fact.predecessor != LeafStateID::no_state){
                                        // can happen for center factoring when center op changed leaf
                                        marked_leaf_states[factor].insert(fact.predecessor.hash());
                                    }
                                }
                            }
                        }
                        id++;
                    }
                    
                    if (!change && !center_preconditions[factor].empty()){
                        // all necessary leaf preconditions marked, but none of them fulfills center precondition
                        // => needs to be handled separately
                        marked_leaf_states[factor].insert(best_center_supporter[factor]);
#ifdef DEBUG_PLAN_EXTRACTION
                        cout << "marked predecessor " << best_center_supporter[factor] << " for center op" << endl;
                        g_state_registry->lookup_leaf_state(LeafStateID(best_center_supporter[factor], factor)).dump_pddl();;
#endif
                        change = true;
                        center_preconditions[factor] = set<int>();
                    }
                    
                }
                
                if (!leaf_pre_by_factor.empty() && !leaf_pre_by_factor[factor].empty()){       // mark leaf preconditions of center op
                    
                    int best_price = numeric_limits<int>::max();
                    LeafStateID best_predecessor = LeafStateID::no_state;
                    
                    size_t number_states = price_tags[step + 1].get_number_states(factor);
                    size_t id = 0;
                    while (number_states > 0) {
                        if (price_tags[step + 1].has_leaf_state(id, factor)){
                            --number_states;
                            bool fulfills_pre = true;
                            LeafState leaf_state = g_state_registry->lookup_leaf_state(LeafStateID(id, factor));
                            for (size_t pre = 0; pre < leaf_pre_by_factor[factor].size(); pre++){
                                int var = leaf_pre_by_factor[factor][pre].first;
                                int val = leaf_pre_by_factor[factor][pre].second;
                                if (leaf_state[var] != val){
                                    fulfills_pre = false;
                                    break;
                                }
                            }
                            if (fulfills_pre){
                                center_preconditions[factor].insert(leaf_state.get_id().hash());
                                int cost = price_tags[step + 1].get_cost_of_state(leaf_state.get_id().hash(), factor);
                                if (cost < best_price){
                                    best_price = cost;
                                    best_predecessor = leaf_state.get_id();
                                }
                            }
                        }
                        ++id;
                    }
                    
                    assert(best_predecessor != LeafStateID::no_state);
                    best_center_supporter[factor] = best_predecessor.hash();
                }
            }
            
            if (step < path.size()){
                decoupled_plan.push_back(path[step]);
#ifdef DEBUG_PLAN_EXTRACTION
                cout << "added root op: " << decoupled_plan.back()->get_name() << endl;
#endif
            }
            
        }
        
#ifndef NDEBUG
        for (size_t i = 0; i < g_factors.size(); ++i){
            assert(marked_leaf_states[i].empty());
        }
#endif
        
        path.swap(decoupled_plan);
        reverse(path.begin(), path.end());
        
    }
}

void SearchSpace::dump() const {
    for (PerStateInformation<SearchNodeInfo>::const_iterator it =
            search_node_infos.begin(g_state_registry);
         it != search_node_infos.end(g_state_registry); ++it) {
        StateID id = *it;
        State s = g_state_registry->lookup_state(id);
        const SearchNodeInfo &node_info = search_node_infos[s];
        cout << id << ": ";
        s.dump_fdr();
        if (node_info.creating_operator && node_info.parent_state_id != StateID::no_state) {
            cout << " created by " << node_info.creating_operator->get_name()
                 << " from " << node_info.parent_state_id << endl;
        } else {
            cout << "has no parent" << endl;
        }
    }
}

void SearchSpace::statistics() const {
    cout << "Number of registered states: " << g_state_registry->size() << endl;
    
    if (g_use_decoupled_search){
        size_t min_leaf_factor_size = numeric_limits<int>::max();
        double avg_leaf_factor_size = 0;
        size_t max_leaf_factor_size = 0;
        size_t tmp;
        bool precomputed_state_spaces = true;
        for (size_t factor = 0; factor < g_factors.size(); ++factor){
            tmp = g_state_registry->size(factor);
            min_leaf_factor_size = min(min_leaf_factor_size, tmp);
            avg_leaf_factor_size += tmp;
            max_leaf_factor_size = max(max_leaf_factor_size, tmp);
            if (!CompliantPathGraph::precompute_leaf_state_space(factor)){
                precomputed_state_spaces = false;
            }
            cout << "Number of registered leaf states in factor " << factor << ": " << tmp << endl;
        }
        if (!precomputed_state_spaces){
            cout << "min reachable leaf factor size "  << min_leaf_factor_size << endl;
            cout << "avg reachable leaf factor size "  << (int) (avg_leaf_factor_size/g_factors.size()) << endl;
            cout << "max reachable leaf factor size "  << max_leaf_factor_size << endl;
        }
        g_state_registry->output_decoupled_state_space_size();
        cout << "maximum duplicate counter " << g_state_registry->get_max_number_duplicates() << endl;
    }
}
