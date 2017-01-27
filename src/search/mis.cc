#include "mis.h"

#include "countdown_timer.h"
#include "utilities.h"

#include "assert.h"
#include <iostream>
#include <algorithm>


using namespace std;

vector<set<int>> MIS::get_mis(const NGraph& g, int x, int arg_find_all, int arg_timeout, int arg_multiple_mis) const {

#ifdef DEBUG_FACTORING
    cout << "find mis for graph of size: " << g.size() << endl;
    print_graph(g);
#endif
    vector<set<int>> all_mis;
    set<int> first_mis;
    all_mis.push_back(first_mis);

    list<pair<pair<NGraph,int>,int>> stack;
    stack.push_front(make_pair(make_pair(g,-1),0));

    CountdownTimer timer(arg_timeout);
    bool timeout = arg_timeout != 0;

    while (!stack.empty() && (!timeout || !timer.is_expired())) {
        pair<NGraph,int> g_t = stack.front().first;
        NGraph g = g_t.first;
        int tag = g_t.second;

        int mis = stack.front().second;
        stack.pop_front();

        if (tag != -1) {
            // if tagged element is not in graph anymore, and neither included in the MIS,
            // ignore stack element and  remove all stack elements with same MIS
            if (g[tag].empty() && all_mis[mis].find(tag) == all_mis[mis].end()) {
                list<pair<pair<NGraph,int>,int>>::iterator elem = stack.begin();
                while (elem != stack.end()) {
                    if (elem->second == mis) {
                        elem = stack.erase(elem);
                    } else {
                        ++elem;
                    }
                }
                continue;
            }
        }

        int only_elem = -1;
        size_t g_size = 0;
        for (size_t i = 0; i < g.size(); ++i){
            if (!g[i].empty()){
                if (g_size == 0){
                    only_elem = i;
                }
                g_size++;
            }
        }
        if (g_size <= 1) {
            if (g_size == 1) {
                all_mis[mis].insert(only_elem);
            }
        } else {
            ConComp connected_components;
            CtoCCMap c_to_cc_map(g_variable_domain.size(),-1);
            find_connected_components(g, &c_to_cc_map, &connected_components);
            if (connected_components.size() > 1) {
                set<int> cc = connected_components[0];
                NGraph g_without_c = remove_components(g, cc);
                NGraph c = remove_components(g, g_without_c);
                stack.push_front(make_pair(make_pair(g_without_c,tag), mis));
                stack.push_front(make_pair(make_pair(c,tag), mis));
            } else {
                // find w and v such that N(w) sub_eq N(v), discard v
                int v1 = -1;
                int w = -1;
                bool found = false;
                bool equal = false;
                for (size_t i = 0; i < g.size() && !found; ++i) {
                    if (!g[i].empty()) {
                        for (size_t n:g[i]) {
                            if (n != i) {
                                if (g[i].size() >= g[n].size()) {
                                    bool subset = true;
                                    for (set<int>::iterator nn = g[n].begin(); nn != g[n].end(); nn++) {
                                        if (g[i].find(*nn) == g[i].end() && *nn != (int) i) {
                                            subset = false;
                                            break;
                                        }
                                    }
                                    if (subset) {
                                        if (g[i].size() == g[n].size()) {
                                            equal = true;
                                        }
                                        w = n;
                                        found = true;
                                        v1 = i;
                                        break;
                                    }

                                } else if (g[i].size() < g[n].size()) {
                                    bool subset = true;
                                    for (size_t v:g[i]) {
                                        if (g[n].find(v) == g[n].end() && v != n) {
                                            subset = false;
                                            break;
                                        }
                                    }
                                    if (subset) {
                                        found = true;
                                        w = i;
                                        v1 = n;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
                if (found) {
                    if (equal && arg_find_all == 1) {
                        assert(w != -1);
                        NGraph g_without_w = remove_components(g, w);
                        set<int> copy(all_mis[mis]);
                        all_mis.push_back(copy);
                        list<pair<pair<NGraph,int>,int>> tmp_stack(stack);

                        NGraph g_without_v = remove_components(g, v1);
                        stack.push_front(make_pair(make_pair(g_without_v, tag), mis));

                        // copy stack "history" such that it is applied to both choices
                        for (const pair<pair<NGraph,int>,int> elem:tmp_stack) {
                            if (elem.second == mis) {
                                stack.push_front(make_pair(make_pair(elem.first.first, v1), all_mis.size()-1));
                            }
                            else if (elem.second == (int)all_mis.size()-1) {
                                break;
                            }
                        }
                        stack.push_front(make_pair(make_pair(g_without_w,v1), all_mis.size()-1));
                    } else {
                        NGraph g_without_v = remove_components(g, v1);
                        stack.push_front(make_pair(make_pair(g_without_v, tag), mis));
                    }
                } else {
                    //find foldable vertex v (d(v) = 2) [ = 3 as every node is its own neighbour]
                    found = false;
                    
                    int v2;
                    for (size_t i = 0; i < g.size(); ++i) {
                        if (g[i].size() == 3) {
                            v2 = i;
                            found = true;
                            break;

                        }
                    }
                    if (found) {
                        pair<int,int> u(-1,-1);
                        for (int i : g[v2]){
                            if (i != v2) {
                                if (u.first == -1) {
                                    u.first = i;
                                } else {
                                    u.second = i;
                                }
                            }
                        }
                        int u1 = u.first;
                        int u2 = u.second;
                        if (g[u1].find(u2) == g[u1].end()) {
                            // case 1: select v; remove v, u1, u2
                            NGraph gf = remove_components(remove_components(g, g[v2]), v2);
                            set<int> copy(all_mis[mis]);
                            all_mis[mis].insert(v2);
                            list<pair<pair<NGraph,int>,int>> tmp_stack(stack);
                            stack.push_front(make_pair(make_pair(gf,tag), mis));

                            // case 2: select u1, u2; remove v, u1, u2, N(u1), N(u2)
                            set<int> remove = g[u1];
                            remove.insert(g[u2].begin(), g[u2].end());
                            remove.insert(u1);
                            remove.insert(u2);
                            NGraph gf2 = remove_components(g, remove);
                            copy.insert(u1);
                            copy.insert(u2);
                            all_mis.push_back(copy);
                            // copy stack "history" such that it is applied to both choices
                            for (const pair<pair<NGraph,int>,int> elem:tmp_stack) {
                                if (elem.second == mis) {
                                    stack.push_front(make_pair(make_pair(elem.first.first, tag), all_mis.size()-1));
                                }
                                else if (elem.second == (int)all_mis.size()-1) {
                                    break;
                                }
                            }
                            stack.push_front(make_pair(make_pair(gf2,tag), all_mis.size()-1));

                        } else {
                            // should never happen, as this case should have been resolved by dominance first
                            print_graph(g);
                            cout <<  "###############   INVALID FOLDING   ###############" << endl;
                            exit_with(EXIT_CRITICAL_ERROR);
                        }
                    } else {
                        // select v of maximum degree, which minimizes |E(N(v))|
                        set<size_t> max_deg;
                        size_t max = 0;

                        for (size_t i = 0; i < g.size(); ++i) {
                            if (!g[i].empty()) {
                                if (g[i].size() > max) {
                                    max_deg.clear();
                                    max = g[i].size();
                                }
                                if (g[i].size() == max) {
                                    max_deg.insert(i);
                                }
                            }
                        }

                        int v3;
                        if (max_deg.size() == 1) {
                            v3 = *max_deg.begin();
                        } else { // find edges of graph induces by N(v)
                            // N(v) = g[i]
                            // N(N(v)) in g[i]
                            int e = 0;
                            v3 = *max_deg.begin();
                            set<int> visited;
                            for (set<size_t>::iterator i = max_deg.begin(); i != max_deg.end(); i++) {
                                visited.clear();
                                int cur_e = 0;
                                for (set<int>::iterator n = g[*i].begin(); n != g[*i].end(); n++) {
                                    visited.insert(*n);
                                    for (set<int>::iterator nn = g[*n].begin(); nn != g[*n].end(); nn++) {
                                        // don't count edges twice
                                        if (visited.find(*nn) == visited.end() && *nn != (int) *i) {
                                            // is N(N(v)) in N(v)
                                            if (g[*i].find(*nn) != g[*i].end()) {
                                                cur_e++;
                                            }
                                        }
                                    }
                                }
                                if (cur_e > e) {
                                    v3 = *i;
                                    e = cur_e;
                                }
                            }
                        }

                        //return max{mis(G - {v} - M{v}, 1 + mis(G - N[v]}
                        // M(v), set of mirrors of v
                        // mirror of v: vertex u in N^2(v) such that N(v)\N(u) is a possibly empty clique
                        // clique: every two distinct vertices are adjacent
                        // N^2(v): neighbours at distance 2 of v


                        // find mirrors of v
                        set<int> mv;
                        set<int> visited;
                        for (set<int>::iterator n = g[v3].begin(); n != g[v3].end(); n++) {
                            for (set<int>::iterator u = g[*n].begin(); u != g[*n].end(); u++) {
                                if (visited.find(*u) == visited.end() && *u != v3 && g[v3].find(*u) == g[v3].end()) {
                                    visited.insert(*u);
                                    // c = N(v)\N(u)
                                    set<int> c(g[v3]);
                                    for (set<int>::iterator it = g[*u].begin(); it != g[*u].end(); it++) {
                                        c.erase(*it);
                                    }
                                    // test whether c is a clique (every two distinct vertices are adjacent)
                                    // (every element in c must be a neighbour of every other element in c)
                                    bool clique = true;
                                    if (c.size() > 1) {
                                        for (set<int>::iterator cc = c.begin(); cc != c.end() && clique; ++cc) {
                                            if (g[*cc].size() < c.size()-1) {
                                                clique = false;
                                                break;
                                            } else {
                                                for (int ccn: c) {
                                                    if (ccn != *cc && g[*cc].find(ccn) == g[*cc].end()) {
                                                        clique = false;
                                                        break;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    if (clique) {
                                        mv.insert(*u);
                                    }
                                }
                            }
                        }
                        set<int> copy(all_mis[mis]);
                        copy.insert(v3);
                        all_mis.push_back(copy);

                        stack.push_front(make_pair(make_pair(remove_components(remove_components(g, v3), mv),tag), mis));
                        stack.push_front(make_pair(make_pair(remove_components(remove_components(g, g[v3]), v3),tag), all_mis.size()-1));
                    }
                }
            }
        }
    }
    vector<set<int>> temp;
    size_t max = all_mis[0].size();
    temp.push_back(all_mis[0]);
    for (size_t i = 1; i < all_mis.size(); i++){
        if (all_mis[i].size() >= max){
            if (all_mis[i].size() > max) {
                temp.clear();
                max = all_mis[i].size();
            }
            temp.push_back(all_mis[i]);
        }
    }
    all_mis = temp;
    if (max != 0) {
        cout << "MIS found: " << all_mis.size() << endl;
    } else {
        cout << "MIS found: 0" << endl;
    }
#ifdef DEBUG_FACTORING
    for (size_t i = 0; i < all_mis.size(); i++) {
        cout << "MIS " << i << ": ";
        for (const int v:all_mis[i]) {
            cout << v <<  " ";
        }
        cout << endl;
    }
#endif
    vector<set<int>> res_mis;
    if (!all_mis.empty()) {
        if (arg_multiple_mis > 1) {
            set<int> candidates;
            while (candidates.size() < min((size_t)arg_multiple_mis,all_mis.size())) {
                candidates.insert(rand() % all_mis.size());
            }
            for (int c:candidates){
                res_mis.push_back(all_mis[c]);
            }
        } else {
            cout << "Select x-th factoring, x: " << x << endl;

            if (x >= (int) all_mis.size()) {
                cout << "Factoring x found: 0" << endl;
            }

            cout << "Factoring x found: 1" << endl;
            assert(x >= 0 && x < (int) all_mis.size());
            res_mis.push_back(all_mis[x]);
        }
    }
#ifdef DEBUG_FACTORING
    for (set<int> m:res_mis) {
        if (!check_mis(g, m)) {
            cout << "did not create valid mis" << endl;
        }
    }
#endif
    if (!res_mis.empty()) {
        cout << "MIS size: " << res_mis[0].size() << endl;
    } else {
        cout << "MIS size: " << 0 << endl;
    }
#ifdef DEBUG_FACTORING
    int i = 0;
    for (set<int> m:res_mis) {
        cout << "MIS " << i << " : ";
        for (set<int>::iterator it = m.begin(); it != m.end(); ++it) {
            cout << *it << " ";
        }
        cout << endl;
    }
#endif
    cout << "Graph size: " << g.size() << endl;
    return res_mis;
}

bool MIS::check_mis(const NGraph &g, const set<int> &mis) const{
    for (set<int>::iterator i = mis.begin(); i != mis.end(); i++) {
        for (set<int>::iterator n = g.at(*i).begin(); n != g.at(*i).end(); ++n) {
            if (*i != *n && mis.find(*n) != mis.end()) {
                return false;
            }
        }
    }
    return true;
}

NGraph MIS::remove_components(NGraph g, int c) const {
    // remove var from neighbours' neighbours
    if (!g[c].empty()) {
        for (int neighbour : g[c]) {
            if (neighbour != c) {
                g[neighbour].erase(c);
            }
        }
        g[c].clear();
    }
    return g;
}

NGraph MIS::remove_components(const NGraph &g, const set<int> &cs) const {
    NGraph n_g(g.size(), set<int>());
    for (size_t i = 0; i < g.size(); ++i) {
        if (!g[i].empty()) {
            if (cs.find(i) == cs.end()) {
                n_g[i] = g[i];
                for (int v:cs) {
                    if (n_g[i].find(v) != n_g[i].end()) {
                        n_g[i].erase(v);
                    }
                }
            }
        }
    }
    return n_g;
}

NGraph MIS::remove_components(const NGraph &g, const NGraph &cs) const {
    NGraph n_g(g.size(), set<int>());
    for (size_t i = 0; i < g.size(); ++i) {
        set_difference(g[i].cbegin(), g[i].cend(), cs[i].cbegin(), cs[i].cend(), std::inserter(n_g[i], n_g[i].begin()));
    }
    return n_g;
}


void MIS::find_connected_components(const NGraph &g, CtoCCMap *c_to_cc, ConComp *cc) const {
    stack<int> open_c;
    size_t seen = 0;
    int m = 0;
    while (seen < g.size()) {
        if (open_c.empty()) {
            for (size_t i = m; i < g.size(); ++i) {
                if (!g[i].empty()) {
                    if ((*c_to_cc)[i] == -1) {
                        m = i;
                        open_c.push(m);
                        set<int> i;
                        (*c_to_cc)[m] = cc->size();
                        i.insert(m);
                        cc->push_back(i);
                        break;
                    }
                }
                else {
                    ++seen;
                }
            }

        } else {
            int c = open_c.top();
            open_c.pop();
            seen++;
            for (int n : g[c]) {
                if (c != n) {
                    if ((*c_to_cc)[n] == -1) {
                        int comp = (*c_to_cc)[c];
                        (*c_to_cc)[n] = comp;
                        (*cc)[comp].insert(n);
                        open_c.push(n);
                    }
                }
            }
        }
    }
    return;
}

void MIS::print_graph(const NGraph &g) const {
    for (size_t i = 0; i < g.size(); ++i) {
        if (!g[i].empty()) {
            cout << "Node: " << i << " - connected to: ";
            for (int s:g[i]) {
                cout << s << " ";
            }
            cout << endl;
        }
    }
    cout << endl;
}
