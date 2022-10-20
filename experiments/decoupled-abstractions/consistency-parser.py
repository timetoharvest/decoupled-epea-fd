#! /usr/bin/env python

import math

from lab.parser import Parser

parser = Parser()

def check_inconsistent_heuristic(content, props):
    inconsistent_heuristic = 'WARNING: heuristic is inconsistent' in content.splitlines()
    props['inconsistent_heuristic'] = inconsistent_heuristic

parser.add_function(check_inconsistent_heuristic)

parser.parse()
