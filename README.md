To get the decoupled-search code clone the repository and run "hg up decoupled-search".

The following heuristics/search engines are adapted for decoupled search:

blind, hmax, add, lmcut, ff
astar, eager_greedy, lazy_wastar, lazy_greedy

To run decoupled search, add the command line option --decoupling "profile(options)"

where profile can be any of {fork, ifork, xshape}

available options: 
search_type = {fra, [sda], asda, sat} where the first three correspond to different types of optimal search
maximize_mobility = {true, [false]} maximize leaf-mobility
max_leaf_size of type int; 0 => no bound on leaf size; default is 2^32. where leaf size is the product of domain sizes