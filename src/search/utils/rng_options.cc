#include "rng_options.h"

#include "rng.h"

#include "../option_parser.h"

using namespace std;

namespace utils {
void add_rng_options(OptionParser &parser) {
    parser.add_option<int>(
        "random_seed",
        "Set to -1 (default) to use the global random number generator. "
        "Set to any other value to use a local random number generator with "
        "the given seed.",
        "-1");
}

RandomNumberGenerator* parse_rng_from_options(
    const Options &options) {
    int seed = options.get<int>("random_seed");
    if (seed == -1) {
        // Use an arbitrary default seed.
        RandomNumberGenerator *rng = new RandomNumberGenerator(2011);
        return rng;
    } else {
        return new RandomNumberGenerator(seed);
    }
}
}
