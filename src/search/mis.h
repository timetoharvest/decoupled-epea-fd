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
 *      implements the algorithm from F. V. Fomin, F Grandoni, and D. Kratsch as presented in
 *      A Measure & Conquer Approach for the Analysis of Exact Algorithms
 *      published in the Journal of the ACM, Vol. 56, No. 5, Article 25
 */

// NGraph, key = var, entry = set of neighbours
typedef std::vector<std::set<int>> NGraph;

// ConCom(Connected Components),
// index = connected component, entry = set of vars
typedef std::vector<std::set<int>> ConComp;
// maps var the respective connected
// component it is included in
typedef std::vector<int> CtoCCMap;

class MIS {

private:
    void find_connected_components(const NGraph&, CtoCCMap *, ConComp *) const;
    NGraph remove_components(NGraph, int) const;
    NGraph remove_components(const NGraph&, const std::set<int>&) const;
    NGraph remove_components(const NGraph&, const NGraph&) const;
public:
    MIS() {}
    std::vector<std::set<int>> get_mis(const NGraph&, int, int, int, int) const;
    void print_graph(const NGraph&) const;
    bool check_mis(const NGraph&, const std::set<int> &mis) const;
};
#endif
