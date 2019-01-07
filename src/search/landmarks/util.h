#ifndef LANDMARKS_UTIL_H
#define LANDMARKS_UTIL_H

#include "../utils/hash.h"

#include <vector>

struct Condition;
class Operator;
class LandmarkNode;

bool _possibly_fires(const std::vector<Condition> &conditions,
                     const std::vector<std::vector<int> > &lvl_var);

utils::HashMap<int, int> _intersect(const utils::HashMap<int, int> &a,
                                    const utils::HashMap<int, int> &b);

bool _possibly_reaches_lm(const Operator &o,
                          const std::vector<std::vector<int> > &lvl_var,
                          const LandmarkNode *lmp);

#endif
