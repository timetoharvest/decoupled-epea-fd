#ifndef POR_METHOD_H
#define POR_METHOD_H

#include <vector>

class Operator;
class State;

class PORMethod {
public:
    virtual ~PORMethod() = default;
    virtual void prune_operators(const State &state,
                                 std::vector<const Operator *> &ops) = 0;
    virtual void print_statistics() const = 0;
};

#endif
