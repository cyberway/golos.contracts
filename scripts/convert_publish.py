#!/usr/bin/env python3
from collections import defaultdict
import json
import utils
import dbs
import re
import ast
import bson
import subprocess
import sys, traceback
from datetime import datetime
from datetime import timedelta
from decimal import Decimal
from bson.decimal128 import Decimal128
from pymongo import MongoClient
from config import *

expiretion = timedelta(minutes = 30)

def create_tags(metadata_tags):
    tags = []
    if (isinstance(metadata_tags, int)):
        tag_obj = { "tag": str(metadata_tags) }
        tags.append(tag_obj)
    if(isinstance(metadata_tags, list)):
        for tag in metadata_tags or []:
            tag_obj = { "tag": tag }
            tags.append(tag_obj)
    return tags

def create_trx(author, id_message):
    trx = ""
    try:
        command = "cleos push action gls.publish closemssg '{\"account\":\""+author+"\", \"permlink\":\""+str(id_message)+"\"}' -p gls.publish -d --return-packed"
        result = ast.literal_eval(re.sub("^\s+|\n|\r|\s+$", '', subprocess.check_output(command, stderr=subprocess.STDOUT, shell=True).decode("utf-8")))
        trx = result["packed_trx"]
    except Exception as e:
        print(e.args)
        print(traceback.format_exc())
    return trx;

class PublishConverter:
    def __init__(self, golos_db, publish_db, cyberway_db, cache_period = 10000):
        self.golos_db = golos_db
        self.publish_db = publish_db
        self.cyberway_db = cyberway_db
        self.exists_accs = set()
        self.mssgs_curatorsw = defaultdict(int)
        self.publish_tables = dbs.Tables([
            ('vote',    self.publish_db['votetable'],    None, None),
            ('message', self.publish_db['messagetable'], None, None),
            ('content', self.publish_db['contenttable'], None, None),
            ("gtransaction", self.cyberway_db['gtransaction'], None, None)
        ])
        self.cache_period = cache_period

    def fill_exists_accs(self):
        cw_accounts = self.cyberway_db['account']
        for doc in cw_accounts.find({},{'name':1}):
            self.exists_accs.add(doc["name"])
        print("accs num = ", len(self.exists_accs))
        for a in self.exists_accs:
            print(a)

    def convert_posts(self, query = {}):
        print("convert_posts")
        golos_gpo = self.golos_db['dynamic_global_property_object'].find()[0]
        golos_posts = self.golos_db['comment_object']
        
        reward_pools = self.publish_db['rewardpools']
        pool = reward_pools.find()[0] #we have to create it before converting
        pool["state"]["funds"]["amount"] = utils.get_golos_asset_amount(golos_gpo["total_reward_fund_steem_value"])
        pool["state"]["funds"]["decs"] = BALANCE_PRECISION
        pool["state"]["funds"]["sym"] = BALANCE_SYMBOL
        
        pool["state"]["msgs"] = 0
        rshares_sum = 0
        
        cursor = golos_posts.find(query)
        length = cursor.count()
        print("total messages = ", length)
        utils.printProgressBar(0, length, prefix = 'Progress:', suffix = 'Complete', length = 50)

        added = 0
        passed = -1        
        for doc in cursor:
            passed += 1
            try:
                if doc["removed"] or (not (doc["author"] in self.exists_accs)):
                    continue
                
                cur_mssg_id = utils.convert_hash(doc["permlink"])
                cur_rshares_raw = utils.get_fixp_raw(doc["net_rshares"])
                messagestate = {
                    "absshares": utils.get_fixp_raw(doc["abs_rshares"]),
                    "netshares": cur_rshares_raw,
                    "voteshares": utils.get_fixp_raw(doc["vote_rshares"]),
                    "sumcuratorsw": self.mssgs_curatorsw[(doc["author"], cur_mssg_id)]
                }
            
                isClosedMessage = True
                date_close = datetime.strptime("2106-02-07T06:28:15", '%Y-%m-%dT%H:%M:%S').isoformat()
                if (doc["cashout_time"].isoformat() != date_close and doc["cashout_time"].isoformat() > datetime.now().isoformat()):
                    rshares_sum += cur_rshares_raw
                    pool["state"]["msgs"] += 1
                    isClosedMessage = False
                    
                    delay_trx = {
                      "trx_id": "",
                      "sender": "gls.publish",
                      "sender_id": hex(utils.convert_hash(doc["permlink"]) << 64 | utils.string_to_name(doc["author"])),
                      "payer": "gls.publish",
                      "delay_until" : doc["cashout_time"].isoformat(), 
                      "expiration" :  (doc["cashout_time"] + expiretion).isoformat(), 
                      "published" :   doc["created"].isoformat(), 
                      "packed_trx" : create_trx(doc["author"], utils.convert_hash(doc["permlink"])), 
                      "_SCOPE_" : "",
                      "_PAYER_" : "",
                      "_SIZE_" : 50
                    }
                    self.publish_tables.gtransaction.append(delay_trx)
                
                orphan_comment = (len(doc["parent_author"]) > 0) and (not (doc["parent_author"] in self.exists_accs))

                message = {
                    "id": cur_mssg_id,
                    "date": int(doc["last_update"].timestamp()) * 1000000,
                    "parentacc": "" if orphan_comment else doc["parent_author"],
                    "parent_id": 0  if (orphan_comment or (not len(doc["parent_permlink"]) > 0)) else utils.convert_hash(doc["parent_permlink"]),
                    "tokenprop": utils.get_prop_raw(doc["percent_steem_dollars"] / 2),
                    "beneficiaries": [],
                    "rewardweight": utils.get_prop_raw(doc["reward_weight"]),
                    "state": messagestate,
                    "childcount": doc["children"],
                    "closed": isClosedMessage,
                    "level": 0 if orphan_comment else doc["depth"], # this value will be incorrect for comment to orphan comment
                                                                    # but we only use level for comments nesting limits, 
                                                                    # so it seems that this is not a problem
                    "_SCOPE_": doc["author"],
                    "_PAYER_": doc["author"],
                    "_SIZE_": 50
                }
                self.publish_tables.message.append(message)
                
                tags = []
                if (isinstance(doc["json_metadata"], dict)):
                    if ("tags" in doc["json_metadata"]):
                        tags = create_tags(doc["json_metadata"]["tags"])

                if(isinstance(doc["json_metadata"], str)):                
                    try:
                        if (doc["json_metadata"]):
                            json_str = doc["json_metadata"]
                            if ((json_str.find("\"") == 0) and (json_str.rfind("\"") == len(json_str)-1)):
                                json_str = json_str[1: len(json_str)-1]
                            dict_metadata = json.loads(json_str)                    
                            if (dict_metadata["tags"]):
                                tags = create_tags(dict_metadata["tags"])
                    except Exception:
                        tags= []
                
                content = {
                    "id": cur_mssg_id,
                    "headermssg": doc["title"],
                    "bodymssg": doc["body"],
                    "languagemssg": "",
                    "tags": tags,
                    "jsonmetadata": doc["json_metadata"],
                    "_SCOPE_": doc["author"],
                    "_PAYER_": doc["author"],
                    "_SIZE_": 50
                }
                self.publish_tables.content.append(content)
                
                if added % self.cache_period == 0:
                    print("messages converted -- ", added)
                    self.publish_tables.writeCache()
                added += 1
                utils.printProgressBar(passed + 1, length, prefix = 'Progress:', suffix = 'Complete', length = 50)
            except Exception as e:
                print(doc)
                print(e.args)
                print(traceback.format_exc())
        try:
            self.publish_tables.writeCache()
        except Exception as e:
            print(e.args)
            print(traceback.format_exc())
        
        shares_sum_str = utils.uint_to_hex_str(max(rshares_sum, 0), 32) #negative sum is almost impossible here
        pool["state"]["rshares"] = shares_sum_str
        pool["state"]["rsharesfn"] = shares_sum_str
        reward_pools.save(pool)
        print("...done")
        
    def convert_votes(self, query = {}):
        print("convert_votes")
        golos_votes = self.golos_db['comment_vote_object']
        cyberway_messages = self.publish_db['messagetable']

        cursor = golos_votes.find(query)
        length = cursor.count()
        print("total votes = ", length)
        length = golos_votes.count()
        utils.printProgressBar(0, length, prefix = 'Progress:', suffix = 'Complete', length = 50)

        added = 0
        passed = -1
        for doc in cursor:
            passed += 1
            try:
                cur_mssg_id = utils.convert_hash(doc["permlink"])
                    
                if (not (doc["author"] in self.exists_accs)) or (not (doc["voter"] in self.exists_accs)):
                    continue

                vote = {
                    "id" : added,
                    "message_id" : cur_mssg_id,
                    "voter" : doc["voter"],
                    "weight" : doc["vote_percent"],
                    "time" : int(doc["last_update"].timestamp()) * 1000000,
                    "count" : doc["num_changes"],
                    "curatorsw": doc["weight"] / 2,
                    "_SCOPE_" : doc["author"],
                    "_PAYER_" : doc["author"],
                    "_SIZE_" : 50
                }

                self.publish_tables.vote.append(vote)

                self.mssgs_curatorsw[(doc["author"], cur_mssg_id)] += vote["curatorsw"]

                if added % self.cache_period == 0:
                    print("votes converted -- ", added)
                    self.publish_tables.writeCache()
                added += 1
                utils.printProgressBar(passed + 1, length, prefix = 'Progress:', suffix = 'Complete', length = 50)
            except Exception as e:
                print(doc)
                print(e.args)
                print(traceback.format_exc())
        try:
            self.publish_tables.writeCache()
        except Exception as e:
            print(e.args)
            print(traceback.format_exc())
        print('...done')
    
    def run(self, query = {}):
        self.fill_exists_accs()
        self.convert_votes(query)
        self.convert_posts(query)

#PublishConverter(dbs.golos_db, dbs.cyberway_gls_publish_db, dbs.cyberway_db).run({"author" : "goloscore"})
PublishConverter(dbs.golos_db, dbs.cyberway_gls_publish_db, dbs.cyberway_db).run()   



