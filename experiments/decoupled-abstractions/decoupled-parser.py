#! /usr/bin/env python

import math
import re

from lab.parser import Parser

parser = Parser()

parser.add_pattern('average_member_states', 'Average number of member states \(generated decoupled states\): (.+)', required=False, type=float)
parser.add_pattern('number_leaf_factors', 'number leaf factors: (.+)', required=False, type=int)

parser.parse()
