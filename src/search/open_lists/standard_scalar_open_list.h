#ifndef OPEN_LISTS_STANDARD_SCALAR_OPEN_LIST_H
#define OPEN_LISTS_STANDARD_SCALAR_OPEN_LIST_H

#include "open_list.h"
#include "../option_parser.h"
#include "../scalar_evaluator.h"

#include <deque>
#include <map>
#include <vector>
#include <utility>

class ScalarEvaluator;

template<class Entry>
class StandardScalarOpenList : public OpenList<Entry> {
    typedef std::deque<Entry> Bucket;

    std::map<int, Bucket> buckets;
    int size;

    std::shared_ptr<ScalarEvaluator> evaluator;
    int last_evaluated_value;
    int last_preferred;
    bool dead_end;
    bool dead_end_reliable;
protected:
    std::shared_ptr<Evaluator> get_evaluator() {return evaluator; }

public:
    StandardScalarOpenList(const options::Options &opts);
    StandardScalarOpenList(std::shared_ptr<ScalarEvaluator> eval,
                           bool preferred_only);
    ~StandardScalarOpenList();

    int insert(const Entry &entry);
    Entry remove_min(std::vector<int> *key = 0);
    bool empty() const;
    void clear();

    void evaluate(int g, bool preferred);
    bool is_dead_end() const;
    bool dead_end_is_reliable() const;
    void get_involved_heuristics(std::set<std::shared_ptr<Heuristic>> &hset);

    static std::shared_ptr<OpenList<Entry>> _parse(options::OptionParser &parser);
};

#include "standard_scalar_open_list.cc"

// HACK! Need a better strategy of dealing with templates, also in the Makefile.

#endif
