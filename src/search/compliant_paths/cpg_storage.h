#ifndef CPG_STORAGE
#define CPG_STORAGE

#include "../per_state_information.h"

class CompliantPathGraph;
class PruningOptions;

class CPGStorage {
    friend class StateRegistry; // only needed to permute initial state for decoupled orbit search
    
protected:
    
    PerStateInformation<uint64_t> hashes; // TODO: not sure if this is really the right place to store this

    virtual void register_at_compliant_path_graph() = 0;

    virtual CompliantPathGraph* get_mutable_cpg(const State &state) = 0;

public:

    virtual ~CPGStorage() = default;

    virtual const CompliantPathGraph* get_cpg(const State &state) const = 0;

    virtual uint64_t get_decoupled_state_hash(const State &state) = 0;

    virtual size_t size(const StateRegistry *registry) const = 0;


    static CPGStorage *storage;

    static void initialize(const PruningOptions &pruning_options);

};

template<class T>
class Storage : public CPGStorage {
    friend T; // to get non-const cpg

    PerStateInformation<T> cpgs;

    void store_cpg(const State &state, T &cpg) {
        cpgs[state] = std::move(cpg);
    }

protected:

    virtual void register_at_compliant_path_graph() override;

    virtual CompliantPathGraph* get_mutable_cpg(const State &state) override {
        return &cpgs[state];
    }

public:

    virtual const CompliantPathGraph* get_cpg(const State &state) const override {
        return &cpgs[state];
    }
    
    virtual uint64_t get_decoupled_state_hash(const State &state) override;

    virtual size_t size(const StateRegistry *registry) const override {
        return cpgs.size(registry);
    }

};


#endif
