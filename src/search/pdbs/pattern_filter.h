#ifndef PDBS_PATTERN_FILTER_H
#define PDBS_PATTERN_FILTER_H

#include "types.h"

namespace pdbs {
class PatternFilter {
public:
    virtual ~PatternFilter() = default;

    virtual bool is_pattern_ok(const Pattern &pattern) const = 0;
    virtual bool can_add_var_to_pattern(
        const Pattern &pattern, int var) const = 0;
    virtual bool can_merge_patterns(
        const Pattern &pattern1, const Pattern &pattern2) const = 0;
};
}

#endif
