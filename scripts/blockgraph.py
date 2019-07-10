#!/usr/bin/python3

# Script to draw diagram of blocklog
# Prepare dot-file with blocklog diagram contains forks, irriversible blocks and
# last block. For drawing diagram used information from Event-Engine (AcceptBlock
# and CommitBlock message).
#
# Typical usage:
# `cat notifier.log | ./blockgraph.py >block.dot && dot -o block.png -Tpng block.dot`

import json
import fileinput

blocks = {}
first_blocks = []
last_block_id = None


def process(msg):
    if msg['msg_type'] == 'AcceptBlock':
        process_accept_block(msg)
    elif msg['msg_type'] == 'CommitBlock':
        process_commit_block(msg)
        
def process_commit_block(msg):
    if msg['id'] in blocks:
        block = blocks[msg['id']]
        block['commit'] = True

def process_accept_block(msg):
    global last_block_id
    trxs = ''
    for trx in msg['trxs']:
        trxs += '%s, ' % trx['id'][0:6]

    block = {
        'id': msg['id'],
        'previous': msg['previous'],
        'block_num': msg['block_num'],
        'hash': msg['id'][8:16],
        'prev': msg['previous'][8:16],
        'trxs': trxs,
        'next': [],
        'commit': False,
        'print': False,
        'link': None,
    }

    if msg['previous'] in blocks:
        prev = blocks[msg['previous']]
        prev['next'].append(block)
    else:
        first_blocks.append(block)

    blocks[msg['id']] = block
    last_block_id = block['id']



for line in fileinput.input():
    try:
        msg = json.loads(line)
    except:
        print("Error on line: '%s'"%line)
        exit(1)
    
    process(msg)


def process_graph(block):
    link = block['prev']
    block['print'] = True
    block['link'] = block['prev']

    next_count = block['next'].__len__()
    if next_count == 1:
        link = block['hash']
        commit = block['commit']
        while next_count == 1:
            blk = block['next'][0]
            if blk['commit'] != commit:
                break
            block = blk
            next_count = block['next'].__len__()

        block['print'] = True
        block['link'] = link

    for blk in block['next']:
        process_graph(blk)

for blk in first_blocks:
    process_graph(blk)

print("digraph graphname {\n")
for block in blocks.values():
    if block['print']:
        props = {
            'shape':'box',
            'label':'%s\\n%s' % (block['block_num'],block['hash']),
            'penwidth': '3' if last_block_id == block['id'] else '1',
            'color': 'blue' if block['commit'] else 'black',
        }
        print('\t"%s" [%s];' % (block['hash'], ','.join(['%s="%s"' % kv for kv in props.items()])))
        print('\t"%s" -> "%s"%s;' % (block['link'], block['hash'], ' [style=dotted]' if block['link'] != block['prev'] else ''))
print("}")
