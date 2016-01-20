#include "simulation_relation.h"

#include "../price_tag.h"
#include "../operator.h"
#include "../factoring.h"
#include "../state_registry.h"
#include "../priority_queue.h"


using namespace std;

void SimulationRelation::init () {
    cout  << "Initializing simulation relation" << endl;
    compute_label_dominance();
    relation.resize(g_factors.size());
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
	compute_simulation( factor);
    }
}

void SimulationRelation::perform_leaf_irrelevance_pruning(bool only_remove_states) {
    reachable.resize(g_factors.size());
    vector<vector<vector<pair<const Operator*, size_t> > > > tmp;
    if (only_remove_states){
        tmp = g_leaf_state_successors;        
    }
    
    boost::dynamic_bitset<> dead_ops(g_operators.size(), false);
    int num_transitions_before = 0, num_transitions_after = 0;
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        for (size_t s = 0; s  < g_leaf_state_successors[factor].size(); ++s){
            auto  & trs_s = g_leaf_state_successors[factor][s];
	    for(auto tr : trs_s) {
		dead_ops[tr.first->get_index()] = true;
	    }
            num_transitions_before += trs_s.size();
            trs_s.erase(std::remove_copy_if(begin(trs_s),
                                            end(trs_s), begin(trs_s),
                                            [&](std::pair<const Operator*, size_t> & tr){
                                                if (simulates(factor, s, tr.second)) return true;
                                                return std::find_if(begin(trs_s),
                                                                    end(trs_s),
                                                                    [&](std::pair<const Operator*, size_t> & tr2){
                                                                        return simulates(factor, tr2.second, tr.second) && 
                                                                            op_dominated_by[tr.first->get_index()] [tr2.first->get_index()]
                                                                            && (!(simulates(factor, tr.second, tr2.second) && 
                                                                                  op_dominated_by[tr2.first->get_index()] [tr.first->get_index()])
                                                                                || tr.second < tr2.second || 
                                                                                (tr.second == tr2.second &&
                                                                                 tr.first->get_index() < tr2.first->get_index()));
                                                                    }) != end(trs_s);
                                            }),
                        end(trs_s));
        }

        // reachability analysis        
        // forward analysis
        reachable[factor].resize(g_leaf_state_successors[factor].size(), false);
        
        vector<size_t> current(1, 0);

        while (!current.empty()){
            vector<size_t> next;
            for (auto id : current){
                if (reachable[factor][id]){
                    continue;
                }
                reachable[factor][id] = true;
                for (auto transition : g_leaf_state_successors[factor][id]){
                    int t = transition.second;
                    if (!reachable[factor][t]) {
                        next.push_back(t);
                    }
                }
            }
            next.swap(current);
        }
        
        // backward analysis: disable because it does not make any sense. Don't ask why is here then....
//          for (size_t factor = 0; factor < g_factors.size(); ++factor){
//             for (size_t s = 0; s  < g_leaf_state_predecessors[factor].size(); ++s){
//                 std::vector<std::pair<const Operator*, size_t> >().swap(g_leaf_state_predecessors[factor][s]);
//             }
//             
//             for (size_t s = 0; s  < g_leaf_state_successors[factor].size(); ++s){
//                 for (auto tr : g_leaf_state_successors[factor][s]){
//                     g_leaf_state_predecessors[factor][tr.second].push_back(make_pair(tr.first, s));
//                 }
//             }
//         }
// 
        
//         boost::dynamic_bitset<> reachable_backwards(g_leaf_state_successors[factor].size(), false) ;
//         
//         current = g_leaf_goal_states[factor];
// 
//         while (!current.empty()){
//             vector<size_t> next;
//             for (auto id : current){
//                 if (reachable_backwards[id]){
//                     continue;
//                 }
//                 reachable_backwards[id] = true;
//                 for (auto transition : g_leaf_state_predecessors[factor][id]){
//                     int t = transition.second;
//                     if (!reachable_backwards[t]) {
//                         next.push_back(t);
//                     }
//                 }
//             }
//             next.swap(current);
//         }
//         
//         reachable[factor] &= reachable_backwards;
        
        
        if (only_remove_states){
            g_leaf_state_successors[factor].swap(tmp[factor]);
        }
        
        // remove irrelevant states 
        for (size_t id = 0; id < reachable[factor].size(); ++id){
            if (!reachable[factor][id]){
                vector<pair<const Operator*, size_t> >().swap(g_leaf_state_successors[factor][id]);
            }
        }
        
        // (re-)remove transitions entering removed states
        for (size_t s = 0; s  < g_leaf_state_successors[factor].size(); ++s){
            auto &trs_s = g_leaf_state_successors[factor][s];
            
            trs_s.erase(std::remove_if(begin(trs_s), end(trs_s),
                [&](std::pair<const Operator*, size_t> & tr){
                    return !reachable[factor][tr.second];
                }), end(trs_s));

	    for(auto tr : trs_s) {
		dead_ops[tr.first->get_index()] = false;
	    }
            
            num_transitions_after += trs_s.size();
        }
    }
    
    if (g_compute_leaf_backward_graph) {
	for (size_t factor = 0; factor < g_factors.size(); ++factor){
	    for (size_t s = 0; s  < g_leaf_state_predecessors[factor].size(); ++s){
		std::vector<std::pair<const Operator*, size_t> >().swap(g_leaf_state_predecessors[factor][s]);
	    }
	    
	    for (size_t s = 0; s  < g_leaf_state_successors[factor].size(); ++s){
		for (auto tr : g_leaf_state_successors[factor][s]){
		    g_leaf_state_predecessors[factor][tr.second].push_back(make_pair(tr.first, s));
		}
	    }
	}
    }

    for (size_t op = 0; op < g_operators.size(); ++op) {
	if (dead_ops[op]){
	    g_operators[op].set_dead();
	}
    }
    
    cout << "Irrelevance pruning: " << num_transitions_before << " => " << num_transitions_after  << endl;
}

bool SimulationRelation::precondition_dominance(const std::vector<const Condition*> & pre, 
						const std::vector<const Condition*> & pre2) const{
    for (const auto & p : pre){
	if (std::find_if(begin(pre2), end(pre2), [p] (const Condition * p2){
		    return *p2 == *p;
		}
		) == end(pre2)){
	    return false;
	}
    }
    return true;
}

void SimulationRelation::compute_label_dominance() {
    //cout  << "Label dominance" << endl;
    op_dominated_by.resize(g_operators.size());
    for (auto & elem : op_dominated_by)
	elem.resize(g_operators.size(), false);

    for (auto op = begin(g_operators); op != end(g_operators); ++op) {
	int factor = op->get_affected_factor();
	if (factor == -1) continue; //Skip center actions
	int cost = get_adjusted_action_cost(*op, g_factoring->get_cost_type());
	const auto & pre = op->get_preconditions(-1);
	
	for (auto op2 = op; ++op2 != end(g_operators); ) {
	    if(factor != op2->get_affected_factor()) continue; //Skip actions of other factors
	    int cost2 = get_adjusted_action_cost(*op2, g_factoring->get_cost_type());
	    const auto & pre2 = op2->get_preconditions(-1);
	    

	    if(cost <= cost2 && precondition_dominance(pre, pre2)) {
		//cout << op->get_name() << " is dominated by " << op2->get_name() << endl;
		op_dominated_by[op2->get_index()][op->get_index()] = true;
	    }

	    if (cost2 <= cost && precondition_dominance(pre2, pre)) {
		//cout << op2->get_name() << " is dominated by " << op->get_name() << endl;
		op_dominated_by[op->get_index()][op2->get_index()] = true;
	    }
	}
    }
}

void SimulationRelation::compute_simulation (int factor) {
    //cout  << "Compute simulation: " << factor << endl;
    //Init goal respecting
    size_t num_states = g_state_registry->size(factor);
    vector<int> goal_distances(num_states, numeric_limits<int>::max()); 
    BucketQueue<int> open;
	
    for (size_t s : g_leaf_goal_states[factor]) {
	goal_distances[s] = 0;
	open.push(0, s);
    }

    while (!open.empty()) {
	pair<int, int> entry = open.pop();
	int state = entry.second;
	int value = entry.first;
	//cout << "s: " << state << endl;
	if (goal_distances [state] < value) continue;
	for (auto transition :  g_leaf_state_predecessors[factor][state]) {
	    int t = transition.second;

	    if (value + 1 < goal_distances[t]) {
		goal_distances[t] = value + 1;
		open.push(value + 1, t);
	    }
	}
    }

    relation[factor].resize(num_states);
    for(size_t i = 0; i < num_states; i++){
        relation[factor][i].resize(num_states, true);
        if(!g_is_leaf_goal_state[factor][i]){
            for (size_t j = 0; j < num_states; j++){
                if (g_is_leaf_goal_state[factor][j] || goal_distances[i] > goal_distances[j]){
                    relation[factor][i][j] = false;
                }
            }
        }
    }

    bool changes = true;
    while (changes) {
        changes = false;
        for (size_t s = 0; s < g_state_registry->size(factor); s++) {
            for (size_t t = 0; t < g_state_registry->size(factor); t++) { //for each pair of states t, s
		if (s != t && simulates(factor, t, s)) {
                    //Check if really t simulates s
                    //for each transition s--l->s':
                    // a) with noop t >= s' and l dominated by noop?
                    // b) exist t--l'-->t', t' >= s' and l dominated by l'?
		    for (auto trs  : g_leaf_state_successors[factor][s]){
			int trs_target = trs.second;
			int trs_label = trs.first->get_index();
			
			if(simulates (factor, t, trs_target)
                           /* && op_dominated_by_noop(trs_label)*/) {
			    continue;
			}
			bool found = false;
			for (auto trt  : g_leaf_state_successors[factor][t]) {
			    int trt_target = trt.second;
			    int trt_label = trt.first->get_index();

			    if(op_dominated_by[trs_label][trt_label] &&
			       simulates(factor, trt_target, trs_target)) {
				found = true;
				break;
			    }
			}
			if(!found) {
			    changes = true;
			    remove(factor, t, s);
			}
		    }
		}
	    }
	}
    }
}


int SimulationRelation::num_equivalences(int factor) const{
    int num = 0;
    std::vector<bool> counted (relation[factor].size(), false);
    for(size_t i = 0; i < counted.size(); i++){
        if(!counted[i]){
            for(size_t j = i + 1; j < relation[factor].size(); j++){
                if(similar(factor, i, j)){
                    counted [j] = true;
                }
            }
        }else{
            num++;
        }
    }
    return num;
}

int SimulationRelation::num_simulations(int factor, bool ignore_equivalences) const{
    int res = 0;
    if(ignore_equivalences){
        std::vector<bool> counted (relation[factor].size(), false);
        for(size_t i = 0; i < relation[factor].size(); ++i){
            if(!counted[i]){
                for(size_t j = i+1; j < relation[factor].size(); ++j){
                    if(similar(factor, i, j)){
                        counted[j] = true;
                    }
                }
            }
        }
        for(size_t i = 0; i < relation[factor].size(); ++i){
            if(!counted[i]){
                for(size_t j = i+1; j < relation[factor].size(); ++j){
                    if(!counted[j]){
                        if(!similar(factor, i, j) && (simulates(factor, i, j) || simulates(factor, j, i))){
                            res ++;
                        }
                    }
                }
            }
        }
    }else {
        for(size_t i = 0; i < relation[factor].size(); ++i)
            for(size_t j = 0; j < relation[factor].size(); ++j)
                if(simulates(factor, i, j))
                    res++;
    }
    return res;
}

void SimulationRelation::set_list_dominated_states() const {
    assert(PriceTag::dominated_states.empty());
    
    PriceTag::dominated_states.resize(g_factors.size());

    for (size_t factor = 0; factor < g_factors.size(); ++factor){    
        
	PriceTag::dominated_states[factor].resize(relation[factor].size());
	for(size_t s = 0; s < relation[factor].size(); ++s){
            if(!reachable.empty() && !reachable[factor][s]) continue;
	    for(size_t t = 0; t < relation[factor].size(); ++t) {
                if(!reachable.empty() && !reachable[factor][t]) continue;
		if (s != t && simulates(factor, t, s)){
		    PriceTag::dominated_states[factor][t].push_back(s);
		}
	    }
	}
    }
}

void SimulationRelation::statistics() const{
    cout << "Simulation Relation Finished. " << endl;
    for (size_t factor = 0; factor < g_factors.size(); ++factor){    
	cout << "Factor " <<  factor << " has " <<
	    num_equivalences(factor) << " equivalences and " << 
	    num_simulations(factor, true) << " simulations " << endl;

// 	dump(factor);
    }    
}

void SimulationRelation::dump(int factor) const{ 
    cout << "SIMREL:" << endl;

    for(size_t j = 0; j < relation[factor].size(); ++j){
        for(size_t i = 0; i < relation[factor][i].size(); ++i){
            if(simulates(factor, j, i) && i != j){
                if(simulates(factor, i, j)){
                    if (j < i){
                        cout << get_name(factor, i) << " <=> " << get_name(factor, j) << endl;
                    }
                }else{
                    cout << get_name(factor, i) << " <= " << get_name(factor, j) << endl;
                }
            }
        }
    }


    cout << "Reasons: " << endl;
    for (size_t s = 0; s < g_state_registry->size(factor); s++) {
	for (size_t t = 0; t < g_state_registry->size(factor); t++) { //for each pair of states t, s
	    if (s != t && simulates(factor, t, s)) {
		for (auto trs  : g_leaf_state_successors[factor][s]){
		    int trs_target = trs.second;
		    int trs_label = trs.first->get_index();
		    
		    if(simulates (factor, t, trs_target)) {
			cout << get_name(factor, s) << " -> " << get_name(factor, trs_target) << " is simulated by " << 
			    get_name(factor, t) << " noop " << endl;
		        continue;
		    }
		    for (auto trt  : g_leaf_state_successors[factor][t]) {
			int trt_target = trt.second;
			int trt_label = trt.first->get_index();
			
			if(op_dominated_by[trs_label][trt_label] &&
			   simulates(factor, trt_target, trs_target)) {
			    cout << get_name(factor, s) << " -> " << get_name(factor, trs_target) << " is simulated by " << 
				get_name(factor, t) << " -> " << get_name(factor, trt_target) << endl;
			    break;
			}
		    }
		}
	    }
	}
    }
}


const string & SimulationRelation::get_name(int factor, int s) const{
    return g_fact_names[g_factors[factor][0]][g_state_registry->lookup_leaf_state(LeafStateID(s, factor))[g_factors[factor][0]]];
}
