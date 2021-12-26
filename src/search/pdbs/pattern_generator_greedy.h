#ifndef PDBS_PATTERN_GENERATOR_GREEDY_H
#define PDBS_PATTERN_GENERATOR_GREEDY_H

#include "pattern_generator.h"

namespace options {
class Options;
}

namespace pdbs {
class PatternFilter;

class PatternGeneratorGreedy : public PatternGenerator {
    int max_states;
    std::shared_ptr<PatternFilter> pattern_filter;
public:
    explicit PatternGeneratorGreedy(const options::Options &opts);
    PatternGeneratorGreedy(int max_states, const std::shared_ptr<PatternFilter> &pattern_filter);
    virtual ~PatternGeneratorGreedy() = default;

    virtual PatternInformation generate(const std::shared_ptr<AbstractTask> &task) override;
};
}

#endif
