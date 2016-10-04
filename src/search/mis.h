#ifndef MIS_H
#define MIS_H


#include "globals.h"
#include <vector>
#include <set>
#include <map>
#include <list>

/* retrieve maximum independent sets
 * using the algorithm from J.M.Robson as presented in
 * Algorithms for Maximum Independent Sets
 * published in the Journal of Algorithms 7, 425-440(1986)
 */

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
    std::set<int> find_mis(NGraph);
    std::set<int> ms_1(NGraph, std::set<int>);
    std::set<int> ms_2(NGraph, std::set<int>);
public:
    MIS(NGraph g) : graph(g){}
    std::set<int> get_mis();
    void print_graph(NGraph);
};
#endif