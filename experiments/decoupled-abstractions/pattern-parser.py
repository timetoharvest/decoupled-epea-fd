#! /usr/bin/env python

import math
import re

from lab.parser import Parser

parser = Parser()

parser.add_pattern('num_affected_leaf_factors', '.*number of affected leaf factors: (\d+)', required=False, type=int)

parser.parse()
