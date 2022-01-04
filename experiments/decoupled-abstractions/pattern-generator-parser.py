#! /usr/bin/env python

import math
import re

from lab.parser import Parser

parser = Parser()

parser.add_pattern('pattern_computation_time', 'computation time: (.+)s', required=False, type=float)
parser.add_pattern('pattern_num_variables', 'number of variables: (\d+)', required=False, type=int)

parser.parse()
