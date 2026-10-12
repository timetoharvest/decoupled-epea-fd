#ifndef OPERATOR_SELECTION_FUNCTION_H
#define OPERATOR_SELECTION_FUNCTION_H

#include <vector>

class GlobalState;
class Operator;

namespace pdbs {
class PatternDatabase;
}

class DirectPDBOSF {
    const pdbs::PatternDatabase &pdb;

    std::vector<int> pattern_position;

public:
    explicit DirectPDBOSF(const pdbs::PatternDatabase &pdb);

    int predict_successor_h(const GlobalState &state,
                            const Operator &op) const;

    int table_value(const GlobalState &state) const;
};

#endif
