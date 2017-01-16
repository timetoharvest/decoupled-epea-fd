#ifndef MIS_H
#define MIS_H

#include "globals.h"

#include <list>
#include <map>
#include <queue>
#include <set>
#include <stack>
#include <vector>


/* retrieve maximum independent sets
 *
 * find_mis_1:
 *      implements the algorithm from J.M.Robson as presented in
 *      Algorithms for Maximum Independent Sets
 *      published in the Journal of Algorithms 7, 425-440(1986)
 * find_mis_2:
 *      implements th algorithm from F. V. Fomin, F Grandoni, and D. Kratsch as presented in
 *      A Measure & Conquer Approach for the Analysis of Exact Algorithms
 *      published in the Journal of the ACM, Vol. 56, No. 5, Article 25
 */

enum MIS_ALGO {
    A1,
    A2,
    DFS
};


// NGraph, key = var, entry = set of neighbours
typedef std::map<int,std::set<int>> NGraph;
typedef std::pair<int,std::set<int>> NVertex;

// ConCom(Connected Components),
// index = connected component, entry = set of vars
typedef std::vector<std::set<int>> ConComp;
// maps var the respective connected
// component it is included in
typedef std::vector<int> CtoCCMap;

class MIS {
    NGraph graph;

private:
    void find_connected_components(NGraph, CtoCCMap *, ConComp *);
    NGraph remove_components(NGraph, int);
    NGraph remove_components(NGraph, std::set<int>);
    NGraph remove_components(NGraph, NGraph);
    std::set<int> find_mis_1(NGraph);
    std::set<int> find_mis_2(NGraph);
    std::vector<std::set<int>> find_mis_2_it(NGraph, int, int, int, int);
    std::set<int> ms_1(NGraph, std::set<int>);
    std::set<int> ms_2(NGraph, std::set<int>);
    std::set<int> brute_force_mis_DFS(NGraph, int x);
    void brute_force_mis_DFS(std::vector<bool>, size_t *, size_t, std::vector<std::vector<bool>> *, std::vector<std::vector<bool>> *, int);
public:
    MIS(NGraph g) : graph(g){}
    std::vector<std::set<int>> get_mis(enum MIS_ALGO, int x, int, int, int);
    void print_graph(NGraph);
    bool check_mis(std::vector<bool> mis);
    bool check_mis(std::set<int> mis);
};
#endif
