#ifndef PDBS_PATTERN_FILTER_H
#define PDBS_PATTERN_FILTER_H

#include "types.h"

namespace pdbs {
class PatternFilter {
public:
    virtual ~PatternFilter() = default;

    virtual bool is_allowed(
        const Pattern &pattern, int var) = 0;
    virtual void filter_variables(
        const Pattern &pattern, std::vector<int> &variables) = 0;
};
}

#endif
