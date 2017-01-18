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
typedef std::vector<std::set<int>> NGraph;
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
    void find_connected_components(const NGraph&, CtoCCMap *, ConComp *) const;
    NGraph remove_components(NGraph, int) const;
    NGraph remove_components(const NGraph&, const std::set<int>&) const;
    NGraph remove_components(const NGraph&, const NGraph&) const;
    std::set<int> find_mis_1(NGraph) const;
    std::set<int> find_mis_2(NGraph) const;
    std::vector<std::set<int>> find_mis_2_it(NGraph, int, int, int, int) const;
    std::set<int> ms_1(NGraph, std::set<int>) const;
    std::set<int> ms_2(NGraph, std::set<int>) const;
    std::set<int> brute_force_mis_DFS(NGraph, int x) const;
    void brute_force_mis_DFS(std::vector<bool>, size_t *, size_t, std::vector<std::vector<bool>> *, std::vector<std::vector<bool>> *, int) const;
public:
    MIS(const NGraph &g) : graph(g){}
    std::vector<std::set<int>> get_mis(enum MIS_ALGO, int x, int, int, int) const;
    void print_graph(const NGraph&) const;
    bool check_mis(const std::vector<bool> &mis) const;
    bool check_mis(const std::set<int> &mis) const;
};
#endif
