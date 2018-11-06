#!/usr/bin/env python3

import posts
import votes

def print_success():
    print('SUCCESS!')

print('posts converting: ')
if posts.convert_posts():
    print_success()

print('votes converting: ')
if votes.convert_votes():
    print_success()
