#include "mis.h"
#include <iostream>
#include "assert.h"
#include "utilities.h"
#include "countdown_timer.h"

using namespace std;

set<int> MIS::get_mis(enum MIS_ALGO algo, int x, int arg_find_all, int arg_timeout) {
#ifdef DEBUG_FACTORING
    cout << "find mis for graph of size: " << graph.size() << endl;
    print_graph(graph);
#endif
    set<int> mis;
    switch (algo) {
        case A1: mis = find_mis_1(graph);
            break;
        //default: case A2: mis = find_mis_2(graph);
        default: case A2: mis = find_mis_2_it(graph,x, arg_find_all, arg_timeout);
            break;
        case DFS: mis = brute_force_mis_DFS(graph, x);
            break;
    }
#ifdef DEBUG_FACTORING
    if (!check_mis(mis)){
        cout << "did not create valid mis" << endl;
    }
#endif
    cout << "MIS size: " << mis.size() << " - ";
    for (set<int>::iterator it = mis.begin(); it != mis.end(); ++it){
        cout << *it << " ";
    }
    cout << endl;
    cout << "Graph size: " << graph.size() << endl;
    return mis;
}

set<int> MIS::brute_force_mis_DFS(NGraph g, int x){
    cout << "DFS" << endl;
    if (g.size() <= 1) {
        set<int> mis;
        if (g.size() == 1) {
            mis.insert(0);
        }
        return mis;
    }
    vector<vector<bool>> gv (g.size());

    for (NGraph::iterator it = g.begin(); it != g.end(); it++) {
        vector<bool> nv (g.size(), false);
        for (set<int>::iterator n = it->second.begin(); n != it->second.end(); n++){
            nv[*n] = true;
        }
        gv[it->first] = nv;
    }

    vector<bool> base (g.size(), false);
    vector<vector<bool>> maxindsets;
    size_t mis_size = 2;
    size_t cur_size = 0;

    brute_force_mis_DFS(base, &mis_size, cur_size, &maxindsets, &gv, -1);
    cout << "MIS found: " << maxindsets.size() << endl;
#ifdef DEBUG_FACTORING
    for (size_t i = 0; i < maxindsets.size(); i++) {
        cout << "MIS " << i << ": ";
        for (size_t v = 0; v < maxindsets[i].size(); v++) {
            if (maxindsets[i][v] == 1) {
                cout << v << " ";
            }
        }
        cout << endl;
    }
#endif
    cout << "Select x-th factoring, x: " << x << endl;

    set<int> mis;
    if (maxindsets.empty()) {
        cout << "No MIS found" << endl;
        return mis;
    }
    if (x >= (int)maxindsets.size()) {
        cout << "No more factoring found: 1" << endl;
        return mis;
    }

    cout << "No more factoring found: 0" << endl;
    assert(x >= 0 && x < (int)maxindsets.size());
    vector<bool> res = maxindsets[x];

    for (size_t i = 0; i < res.size(); i++){
        if (res[i]) {
            mis.insert(i);
        }
    }

    return mis;
}

void MIS::brute_force_mis_DFS(vector<bool> base, size_t * max, size_t cur_size, vector<vector<bool>> * maxindsets, vector<vector<bool>> * g, int first){
    bool mis = false;
    if (cur_size >= *max) {
        mis = check_mis(base);
    }
    if (mis) {
        if (cur_size > *max) {
            maxindsets->clear();
            *max = cur_size;
        }
        maxindsets->push_back(base);
    }
    for (size_t i = 0; i < base.size(); i++) {
        if (!base[i] && (int)i > first) {
            bool connected = false;
            for (size_t j = 0; j < base.size() && !connected; j++) {
                if (base[j] && (*g)[i][j]) {
                    connected = true;
                }
            }
            if (!connected ) {
                vector<bool> child (base);
                child[i] = true;
                first = i;
                brute_force_mis_DFS(child, max, cur_size + 1, maxindsets, g, first);
            }
        }
    }
}

set<int> MIS::find_mis_1(NGraph g) {
    set<int> mis;

    if (g.size() <= 1) {
        if (g.size() == 0) {
            return mis;
        }
        mis.insert(g.begin()->first);
        return mis;
    }

    ConComp connected_components;
    CtoCCMap c_to_cc_map(g_variable_domain.size(),-1);
    find_connected_components(g, &c_to_cc_map, &connected_components);
    if (connected_components.size() > 1) {
        // find smallest component
        int min = 0;
        for (size_t c = 1; c < connected_components.size(); c++) {
            if (connected_components[c].size() < connected_components[min].size())
            {
                min = c;
            }
        }
        NGraph g_without_c = remove_components(g, connected_components[min]);
        set<int> mis_g_c = find_mis_1(g_without_c);
        set<int> mis_c;
        if (connected_components[min].size() <= 2) {
            mis_c.insert(*connected_components[min].begin());
        } else {
            mis_c = find_mis_1(remove_components(g, g_without_c));
        }
        mis_g_c.insert(mis_c.begin(), mis_c.end());
        return mis_g_c;
    }
    // find candidates for A
    size_t min = g.begin()->second.size();
    set<int> candidatesA;

    for (NGraph::iterator it = g.begin(); it != g.end(); ++it) {
        if (it->second.size() == min) {
            candidatesA.insert(it->first);
        } else if (it->second.size() < min) {
            candidatesA.clear();
            min = it->second.size();
            candidatesA.insert(it->first);
        }
    }
    size_t max = 0;
    int A;
    int B;

    for (set<int>::iterator a = candidatesA.begin(); a != candidatesA.end(); ++a) {
        for (set<int>::iterator b = g[*a].begin(); b != g[*a].end(); ++b) {
            if (g[*b].size() > max) {
                max = g[*b].size();
                B = *b;
                A = *a;
            }
        }
    }
    // case d(A) = 1
    if (g[A].size() == 1) {
        mis.insert(A);

        // g without a and a's neighbours
        NGraph g_red = remove_components(remove_components(g, g[A]), A);
        set<int> temp = find_mis_1(g_red);
        mis.insert(temp.begin(), temp.end());
        return mis;
    }

    // case d(A) = 2
    if (g[A].size() == 2) {
        // get A's other neighbour
        int B_2;
        for (set<int>::iterator b = g[A].begin(); b != g[A].end(); ++b) {
            if (B != *b) {
                B_2 = *b;
            }
        }
        if (g[B].find(B_2) != g[B].end()) {
            mis = find_mis_1(remove_components(remove_components(g, A), g[A]));
            mis.insert(A);
            return mis;
        }

        set<int> temp = g[B];
        temp.insert(B);
        temp.insert(g[B_2].begin(), g[B_2].end());
        temp.insert(B_2);
        set<int> temp1 = find_mis_1(remove_components(g, temp));

        // find secondary neighbours of a
        set<int> n2_a;
        for (set<int>::iterator na = g[A].begin(); na != g[A].end(); ++na) {
            n2_a.insert(g[*na].begin(), g[*na].end());
        }
        n2_a.erase(A);

        set<int> temp2 = ms_2(remove_components(remove_components(g, A), g[A]), n2_a);

        if (temp1.size() + 2 >= temp2.size() + 1) {
            temp1.insert(B);
            temp1.insert(B_2);
            return temp1;
        } else {
            temp2.insert(A);
            return temp2;
        }
    }
    // case d(A) = 3
    if (g[A].size() == 3) {
        set<int> temp1 = ms_2(remove_components(g, A), g[A]);
        set<int> temp2 = find_mis_1(remove_components(remove_components(g, A), g[A]));
        if (temp1.size() >= temp2.size() + 1) {
            return temp1;
        } else {
            temp2.insert(A);
            return temp2;
        }
    }
    // case A dominates B
    bool dominate = true;
    for (set<int>::iterator na = g[A].begin(); na != g[A].end(); ++na) {
        if (g[B].find(*na) == g[B].end() && *na != B) {
            dominate = false;
            break;
        }
    }
    if (dominate && g[A].size() != g[B].size()) {
        return find_mis_1(remove_components(g, B));
    }
    // otherwise
    set<int> temp1 = find_mis_1(remove_components(g, B));
    set<int> temp2 = find_mis_1(remove_components(remove_components(g, B), g[B]));
    if (temp1.size() >= temp2.size() + 1) {
        return temp1;
    } else {
        temp2.insert(B);
        return temp2;
    }
}

std::set<int> MIS::ms_1(NGraph g, std::set<int> s) {
    int s1, s2;
    s1 = *s.begin();
    s2 = *(++s.begin());
    if (g[s1].size() > g[s2].size()) {
        int temp = s2;
        s2 = s1;
        s1 = temp;
    }

    if (g[s1].size() <= 1) {
        return find_mis_1(g);
    }
    // check for edge between s1 and s2
    if (g[s1].find(s2) != g[s1].end()) {
        if (g[s1].size() <= 3) {
            return find_mis_1(g);
        } else {
            set<int> temp1 = find_mis_1(remove_components(remove_components(g, s1), g[s1]));
            set<int> temp2 = find_mis_1(remove_components(remove_components(g, s2), g[s2]));
            if (temp1.size() >= temp2.size()) {
                temp1.insert(s1);
                return temp1;
            } else {
                temp2.insert(s2);
                return temp2;
            }
        }
    }
    // check whether s1 and s2 share neighbours
    set<int> intersection;
    for (set<int>::iterator it = g[s1].begin(); it != g[s1].end(); ++it) {
        if (g[s2].find(*it) != g[s2].end()) {
            intersection.insert(*it);
        }
    }
    if (!intersection.empty()) {
        return ms_1(remove_components(g, intersection), s);
    }
    // TODO in the original algorithm this check tests whether s2 has 2 neighbours
    // however, then we can't assume that s1 has two neighbours E and F (might be more)
    if (g[s1].size() == 2) {
        // independent sets to be considered contain s1 or (s2, E and F)
        int E = *(g[s1].begin());
        int F = *(++g[s1].begin());

        // do E and F share an edge?
        if (g[E].find(F) != g[E].end()) {
            set<int> temp = find_mis_1(remove_components(remove_components(g, s1), g[s1]));
            temp.insert(s1);
            return temp;
        }
        // are the neighbours of E and F (except s1) neighbours of s2?
        set<int> temp = g[E];
        temp.insert(g[F].begin(), g[F].end());
        temp.erase(s1);

        bool subset = true;
        for (set<int>::iterator it = temp.begin(); it != temp.end(); ++it) {
            if (g[s2].find(*it) == g[s2].end()) {
                subset = false;
                break;
            }
        }
        if (subset && !(temp.size() == g[s2].size())) {
            set<int> temp = g[s1];
            temp.insert(s1);
            temp.insert(s2);
            temp.insert(g[s2].begin(), g[s2].end());
            set<int> mis = find_mis_1(remove_components(g, temp));
            mis.insert(s2);
            mis.insert(E);
            mis.insert(F);
            return mis;

        }
        set<int> temp1 = find_mis_1(remove_components(remove_components(g, s1), g[s1]));

        temp = g[E];
        temp.insert(E);
        temp.insert(F);
        temp.insert(g[F].begin(), g[F].end());
        temp.insert(s2);
        temp.insert(g[s2].begin(), g[s2].end());
        set<int> temp2 = find_mis_1(remove_components(g, temp));
        if (temp1.size() + 1 >= temp2.size() + 3) {
            temp1.insert(s1);
            return temp1;
        } else {
            temp2.insert(s2);
            temp2.insert(E);
            temp2.insert(F);
            return temp2;
        }
    }

    // independent set contains s2 or (s1 and two elements of N(s2))
    set<int> temp1 = find_mis_1(remove_components(remove_components(g, s2), g[s2]));
    set<int> temp2 = ms_2(remove_components(remove_components(remove_components(g, s1), s2), g[s1]), g[s2]);
    if (temp1.size() >= temp2.size()) {
        temp1.insert(s2);
        return temp1;
    } else {
        temp2.insert(s1);
        return temp2;
    }
}

std::set<int> MIS::ms_2(NGraph g, std::set<int> s) {
    set<int> mis;
    if (s.size() <= 1) {
        return mis;
    }

    if (s.size() == 2) {
        int s1, s2;
        s1 = *s.begin();
        s2 = *(++s.begin());

        // do s1 and s2 share an edge?
        if (g[s1].find(s2) != g[s1].end()) {
            return mis;
        } else {
            set<int> temp = g[s1];
            temp.insert(s1);
            temp.insert(s2);
            temp.insert(g[s2].begin(), g[s2].end());
            set<int> mis = find_mis_1(remove_components(g, temp));
            mis.insert(s2);
            mis.insert(s1);
            return mis;
        }
    }

    if (s.size() == 3) {
        // sort elements in s (ascending degree)

        list<int> sorted_s;
        sorted_s.push_back(*s.begin());
        s.erase(s.begin());
        for (set<int>::iterator it = s.begin(); it != s.end(); ++it) {
            if (*it <= sorted_s.front()) {
                sorted_s.push_front(*it);
            } else if (*it >= sorted_s.back()) {
                sorted_s.push_back(*it);
            } else {
                for (list<int>::iterator s_it = ++sorted_s.begin(); s_it != sorted_s.end(); ++s_it) {
                    if (*it <= *s_it) {
                        sorted_s.insert(s_it, *it);
                        break;
                    }
                }
            }
        }

        // first case: d(s1) = 0
        int s1 = *sorted_s.begin();
        if (g[s1].size() == 0) {
            s.erase(s1);
            set<int> mis = ms_1(remove_components(g, s1), s);
            mis.insert(s1);
            return mis;
        }

        // second case: edge(s1,s2) and edge(s2,s3) and edge(s1,s3)
        int s2 = *(sorted_s.begin()++);
        int s3 = *((sorted_s.begin()++)++);
        bool edge_s1_s2 = (g[s1].find(s2) != g[s1].end());
        bool edge_s1_s3 = (g[s1].find(s3) != g[s1].end());
        bool edge_s2_s3 = (g[s2].find(s3) != g[s2].end());
        if (edge_s1_s2 && edge_s1_s3 && edge_s2_s3) {
            set<int> mis;
            return mis;
        }
        // third case: edge(si,sj) and (si,sk)(j<>k)
        /* Note: order of degree of i,j,k irrelevant, we need to select two of them.
         * if there is a vertex that connects two others, than those two are our choice
         */
        if (edge_s1_s2 && edge_s1_s3) {
            set<int> temp = g[s2];
            temp.insert(g[s3].begin(), g[s3].end());
            temp.insert(s2);
            temp.insert(s3);
            set<int> mis = find_mis_1(remove_components(g, temp));
            mis.insert(s2);
            mis.insert(s3);
        }
        if (edge_s1_s2 && edge_s2_s3) {
            set<int> temp = g[s1];
            temp.insert(g[s3].begin(), g[s3].end());
            temp.insert(s1);
            temp.insert(s3);
            set<int> mis = find_mis_1(remove_components(g, temp));
            mis.insert(s1);
            mis.insert(s3);
        }
        if (edge_s1_s3 && edge_s2_s3) {
            set<int> temp = g[s2];
            temp.insert(g[s1].begin(), g[s1].end());
            temp.insert(s2);
            temp.insert(s1);
            set<int> mis = find_mis_1(remove_components(g, temp));
            mis.insert(s2);
            mis.insert(s1);
        }

        // forth case: edge(si,sj)
        if (edge_s1_s2) {
            set<int> temp;
            temp.insert(s1);
            temp.insert(s2);
            set<int> mis = ms_1(remove_components(remove_components(g, s3), g[s3]), temp);
        }
        if (edge_s1_s3) {
            set<int> temp;
            temp.insert(s1);
            temp.insert(s3);
            set<int> mis = ms_1(remove_components(remove_components(g, s2), g[s2]), temp);
        }
        if (edge_s2_s3) {
            set<int> temp;
            temp.insert(s3);
            temp.insert(s2);
            set<int> mis = ms_1(remove_components(remove_components(g, s1), g[s1]), temp);
        }

        // fifth case: vertex v in N(si) intersect N(sj)(j<>k)
        // check whether there is a shared neighbour between the nodes in S
        for (set<int>::iterator it = g[s1].begin(); it != g[s1].end(); ++it) {
            if (g[s2].find(*it) != g[s2].end()) {
                return ms_2(remove_components(g, *it), s);
            }
            if (g[s3].find(*it) != g[s3].end()) {
                return ms_2(remove_components(g, *it), s);
            }
        }
        for (set<int>::iterator it = g[s2].begin(); it != g[s2].end(); ++it) {
            if (g[s3].find(*it) != g[s3].end()) {
                return ms_2(remove_components(g, *it), s);
            }
        }

        // sixth case: d(s1) = 1
        if (g[s1].size() == 1) {
            s.erase(s1);
            set<int> mis = ms_1(remove_components(remove_components(g, g[s1]), s1), s);
            mis.insert(s1);
            return mis;
        }

        // otherwise
        s.erase(s1);
        set<int> temp1 = ms_1(remove_components(remove_components(g, g[s1]), s1), s);
        set<int> temp = g[s2];
        temp.insert(s2);
        temp.insert(s3);
        temp.insert(g[s3].begin(), g[s3].end());
        temp.insert(s1);

        set<int> temp2 = ms_2(remove_components(g, temp), g[s1]);

        if (temp1.size() + 1 >= temp2.size()) {
            temp1.insert(s1);
            return temp1;
        } else {
            return temp2;
        }
    }

    if (s.size() == 4) {
        for (NGraph::iterator it = g.begin(); it != g.end(); ++it) {
            if ((*it).second.size() <= 3) {
                return find_mis_1(g);
            }
        }
        int s1 = 0;
        size_t min = g[0].size();
        for (set<int>::iterator it = s.begin(); it != s.end(); ++it) {
            // find vertex with smallest degree
            if (g[*it].size() < min) {
                min = g[*it].size();
                s1 = *it;
            }
        }
        set<int> temp1 = find_mis_1(remove_components(remove_components(g, s1), g[s1]));
        s.erase(s1);
        set<int> temp2 = ms_2(remove_components(g, s1), s);
        if (temp1.size() + 1 >= temp2.size()) {
            temp1.insert(s1);
            return temp1;
        } else {
            return temp2;
        }
    }
    return find_mis_1(g);
}

set<int> MIS::find_mis_2(NGraph g) {
    if (g.size() <= 1) {
        set<int> found_mis;
        found_mis.insert((*g.begin()).first);
        return found_mis;
    }
    ConComp connected_components;
    CtoCCMap c_to_cc_map(g_variable_domain.size(),-1);
    find_connected_components(g, &c_to_cc_map, &connected_components);
    if (connected_components.size() > 1) {
        set<int> cc = connected_components[0];
        NGraph g_without_c = remove_components(g, cc);
        NGraph c = remove_components(g, g_without_c);;
        set<int> mis_c = find_mis_2(c);
        set<int> mis_g_c = find_mis_2(g_without_c);
        mis_c.insert(mis_g_c.begin(), mis_g_c.end());

        return mis_c;
    }
    // find w and v such that N(w) sub_eq N(v), discard v
    int v1;
    bool found = false;
    for (NGraph::iterator it = g.begin(); it != g.end() && !found; it++) {
        for (set<int>::iterator n = g[it->first].begin(); n != g[it->first].end(); n++)  {
            if ( *n < it->first) {
                continue;
            }
            if (g[it->first].size() >= g[*n].size()) {
                bool subset = true;
                for (set<int>::iterator nn = g[*n].begin(); nn != g[*n].end(); nn++){
                    if (g[it->first].find(*nn) == g[it->first].end() && *nn != it->first) {
                        subset = false;
                        break;
                    }
                }
                if (subset) {
                    found = true;
                    v1 = it->first;
                    break;
                }

            }
            else if (g[it->first].size() < g[*n].size()) {
                bool subset = true;
                for (set<int>::iterator in = g[it->first].begin(); in != g[it->first].end(); in++){
                    if (g[*n].find(*in) == g[*n].end() && *in != *n) {
                        subset = false;
                        break;
                    }
                }
                if (subset) {
                    found = true;
                    v1 = *n;
                    break;
                }
            }
        }
    }
    if(found) {
        NGraph g_without_v = remove_components(g, v1);
        set<int> found_mis = find_mis_2(g_without_v);
        return found_mis;
    }

    //find foldable vertex v (d(v) = 2)
    found = false;
    int v2;
    for (NGraph::iterator it = g.begin(); it != g.end() && !found; it++) {
        if (it->second.size() == 2) {
            v2 = it->first;
            found = true;
            break;

        }
    }
    if (found) {
        int i1 = *(g[v2].begin());
        int i2 = *(g[v2].begin()++);
        if (g[i1].find(i2) != g[i1].end()) {
            // folding basically merges the two neighbouring nodes
            set<int> u (g[i1]);
            u.insert(g[i2].begin(), g[i2].end());
            NGraph gf = remove_components(remove_components(g, v2), i2);
            gf[i1].clear();
            gf[i1].insert(u.begin(), u.end());
            set<int> mis = find_mis_2(gf);
            mis.insert(v2);
            return mis;
        } else {
            NGraph g_without_Nv = remove_components(remove_components(g, v2), g[v2]);
            set<int> mis = find_mis_2(g_without_Nv);
            mis.insert(v2);
            return mis;
        }
    }
    // select v of maximum degree, which minimizes |E(N(v))|
    set<size_t> max_deg;
    size_t max = 0;

    for (NGraph::iterator it = g.begin(); it != g.end(); it++) {
        if (it->second.size() > max) {
            max_deg.clear();
            max = it->second.size();
        }
        if (it->second.size() == max) {
            max_deg.insert(it->first);
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
            int cur_e = 0;
            for (set<int>::iterator n = g[*i].begin(); n != g[*i].end(); n++) {
                visited.insert(*n);
                for (set<int>::iterator nn = g[*n].begin(); nn != g[*n].end(); nn++) {
                    // don't count edges twice
                    if (visited.find(*nn) == visited.end() && *nn != (int)*i) {
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
    // mirror of v: vertex u in N(N(v)) such that N(v)\N(u) is a possibly empty clique
    // clique: every two distinct vertices are adjacent


    // find mirrors of v
    set<int> mv;
    set<int> visited;
    for (set<int>::iterator n = g[v3].begin(); n != g[v3].end(); n++) {
        for (set<int>::iterator u = g[*n].begin(); u != g[*n].end(); u++) {
            if (visited.find(*u) == visited.end() && *u != v3) {
                visited.insert(*u);
                // c = N(v)\N(u)
                set<int> c (g[v3]);
                for (set<int>::iterator it = g[*u].begin(); it != g[*u].end(); it ++){
                  c.erase(*it);
                }
                //c.erase(g[*u].begin(), g[*u].end());

                for (set<int>::iterator cc = c.begin(); cc != c.begin(); cc++) {
                }
                // test whether c is a clique (every two distinct vertices are adjacent)
                // (every element in c must be a neighbour of every other element in c)
                bool clique = true;
                if (c.size() > 1) {
                    set<int> cn(c);
                    for (set<int>::iterator cc = c.begin(); cc != c.begin() && clique; cc++) {
                        cn.erase(*cc);
                        for (set<int>::iterator cnn = cn.begin(); cnn != cn.end(); cnn++) {
                            if (g[*cc].find(*cnn) == g[*cc].end()) {
                                clique = false;
                                break;
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
    set<int> mis1 = find_mis_2(remove_components(remove_components(g, v3), mv));
    set<int> mis2 = find_mis_2(remove_components(remove_components(g, g[v3]), v3));
    mis2.insert(v3);

    if (mis1.size() >= mis2.size()) {
        return mis1;
    } else {
        return mis2;
    }
}

set<int> MIS::find_mis_2_it(NGraph g, int x, int arg_find_all, int arg_timeout) {

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
            if (g.find(tag) == g.end() && all_mis[mis].find(tag) == all_mis[mis].end()) {
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

        if (g.size() <= 1) {
            if (g.size() == 1) {
                all_mis[mis].insert((*g.begin()).first);
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
                int v1;
                int w = -1;
                bool found = false;
                bool equal = false;
                for (NGraph::iterator it = g.begin(); it != g.end() && !found; it++) {
                    for (set<int>::iterator n = g[it->first].begin(); n != g[it->first].end(); n++) {
                        if (g[it->first].size() >= g[*n].size()) {
                            bool subset = true;
                            for (set<int>::iterator nn = g[*n].begin(); nn != g[*n].end(); nn++) {
                                if (g[it->first].find(*nn) == g[it->first].end() && *nn != it->first) {
                                    subset = false;
                                    break;
                                }
                            }
                            if (subset) {
                                if (g[it->first].size() == g[*n].size()) {
                                    equal = true;
                                }
                                w = *n;
                                found = true;
                                v1 = it->first;
                                break;
                            }

                        } else if (g[it->first].size() < g[*n].size()) {
                            bool subset = true;
                            for (set<int>::iterator in = g[it->first].begin(); in != g[it->first].end(); in++) {
                                if (g[*n].find(*in) == g[*n].end() && *in != *n) {
                                    subset = false;
                                    break;
                                }
                            }
                            if (subset) {
                                found = true;
                                w = it->first;
                                v1 = *n;
                                break;
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
                    //find foldable vertex v (d(v) = 2)
                    found = false;
                    
                    int v2;
                    for (NGraph::iterator it = g.begin(); it != g.end() && !found; it++) {
                        if (it->second.size() == 2) {
                            v2 = it->first;
                            found = true;
                            break;

                        }
                    }
                    if (found) {
                        set<int>::iterator it = g[v2].begin();
                        int u1 = *(it);
                        int u2 = *(++it);
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
                            cout <<  "###############   INVALID FOLDING   ###############" << endl;
                            exit_with(EXIT_CRITICAL_ERROR);
                        }
                    } else {
                        // select v of maximum degree, which minimizes |E(N(v))|
                        set<size_t> max_deg;
                        size_t max = 0;

                        for (NGraph::iterator it = g.begin(); it != g.end(); it++) {
                            if (it->second.size() > max) {
                                max_deg.clear();
                                max = it->second.size();
                            }
                            if (it->second.size() == max) {
                                max_deg.insert(it->first);
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
    cout << "MIS found: " << all_mis.size() << endl;
#ifdef DEBUG_FACTORING
    for (size_t i = 0; i < all_mis.size(); i++) {
        cout << "MIS " << i << ": ";
        for (const int v:all_mis[i]) {
            cout << v <<  " ";
        }
        cout << endl;
    }
#endif
    cout << "Select x-th factoring, x: " << x << endl;
    set<int> mis;
    if (all_mis.empty()) {
        cout << "No MIS found" << endl;
        return mis;
    }
    if (x >= (int) all_mis.size()) {
        cout << "No more factoring found: 1" << endl;
        return mis;
    }

    cout << "No more factoring found: 0" << endl;
    assert(x >= 0 && x < (int) all_mis.size());

    return all_mis[x];
}

bool MIS::check_mis(vector<bool> mis) {
    for (size_t i = 0; i <  mis.size(); i++) {
        if (mis[i]) {
            for (set<int>::iterator n = graph[i].begin(); n != graph[i].end(); ++n) {
                if (mis[*n]) {
                    return false;
                }
            }
        }
    }
    return true;
}

bool MIS::check_mis(set<int> mis) {
    for (set<int>::iterator i = mis.begin(); i != mis.end(); i++) {
        for (set<int>::iterator n = graph[*i].begin(); n != graph[*i].end(); ++n) {
            if (mis.find(*n) != mis.end()) {
                return false;
            }
        }
    }
    return true;
}

NGraph MIS::remove_components(NGraph g, int c) {
    set<int> neighbours = g[c];
    g.erase(c);
    // remove var from neighbours' neighbours
    for (set<int>::iterator it = neighbours.begin(); it != neighbours.end(); ++it) {
        g[*it].erase(c);
    }
    return g;
}

NGraph MIS::remove_components(NGraph g, std::set<int> cs) {
    for (set<int>::iterator it = cs.begin(); it != cs.end(); ++it) {
        g = remove_components(g, *it);
    }
    return g;
}

NGraph MIS::remove_components(NGraph g, NGraph cs) {
    for (NGraph::iterator it = cs.begin(); it != cs.end(); ++it) {
        g = remove_components(g, it->first);
    }
    return g;
}


void MIS::find_connected_components(NGraph g, CtoCCMap *c_to_cc, ConComp *cc) {
    stack<int> open_c;
    size_t seen = 0;
    while (seen < g.size()) {
        if (open_c.empty()) {
            NVertex c = *g.begin();
            bool found = true;
            for (NVertex it:g) {
                if ((*c_to_cc)[it.first] == -1) {
                    c = it;
                    found = true;
                }
            }
            if (!found) {
                //cout << "no unseen node found" << endl;
                return;
            }
            open_c.push(c.first);
            set<int> i;
            (*c_to_cc)[c.first] = cc->size();
            i.insert(c.first);
            cc->push_back(i);
        } else {
            int c = open_c.top();
            open_c.pop();
            seen++;
            set<int> neighbours = g[c];
            for (int n:neighbours) {
                if ((*c_to_cc)[n] == -1) {
                    int comp = (*c_to_cc)[c];
                    (*c_to_cc)[n] = comp;
                    (*cc)[comp].insert(n);
                    open_c.push(n);
                }
            }
        }
    }
    return;
}

void MIS::print_graph(NGraph g) {
    for (NGraph::iterator it = g.begin(); it != g.end(); ++it) {
        cout << "Node: " << it->first << " - connected to: ";
        for(set<int>::iterator s = it->second.begin(); s != it->second.end(); ++s) {
            cout << *s << " ";
        }
        cout << endl;
    }
    cout << endl;
}