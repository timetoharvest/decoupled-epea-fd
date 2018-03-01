#include "successor_generator.h"

#include "successor_generator_factory.h"
#include "successor_generator_internals.h"

#include "../state.h"

using namespace std;

namespace successor_generator {
SuccessorGenerator::SuccessorGenerator()
    : root(SuccessorGeneratorFactory().create()) {
}

SuccessorGenerator::~SuccessorGenerator() = default;

void SuccessorGenerator::generate_applicable_ops(
    const State &state, vector<OperatorID> &applicable_ops) const {
    root->generate_applicable_ops(state, applicable_ops);
}
}
