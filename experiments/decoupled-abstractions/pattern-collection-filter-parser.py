#! /usr/bin/env python

import math
import re

from lab.parser import Parser

parser = Parser()

parser.add_pattern('average_num_affected_leaf_factors', 'average number of affected leaf factors: (\d+)', required=False, type=int)
parser.add_pattern('maximum_num_affected_leaf_factors', 'maximum number of affected leaf factors: (\d+)', required=False, type=int)

parser.parse()
