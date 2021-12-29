#ifndef PDBS_PATTERN_FILTER_ONE_LEAF_PATTERNS_H
#define PDBS_PATTERN_FILTER_ONE_LEAF_PATTERNS_H

#include "pattern_filter.h"

namespace pdbs {
class PatternFilterOneLeafPatterns : public PatternFilter {
public:
    PatternFilterOneLeafPatterns() = default;
    virtual ~PatternFilterOneLeafPatterns() = default;

    virtual bool is_pattern_ok(const Pattern &pattern) const override;
    virtual bool can_add_var_to_pattern(
        const Pattern &pattern, int var) const override;
    virtual bool can_merge_patterns(
        const Pattern &pattern1, const Pattern &pattern2) const override;
};
}

#endif
