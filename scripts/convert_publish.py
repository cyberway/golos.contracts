#!/usr/bin/env python3
from collections import defaultdict
import json
import utils
import dbs
import bson
import sys, traceback
from datetime import datetime
from datetime import timedelta
from decimal import Decimal
from bson.decimal128 import Decimal128
from sshtunnel import SSHTunnelForwarder
from pymongo import MongoClient
from config import *

def create_tags(metadata_tags):
    tags = []
    for tag in metadata_tags or []:
        tag_obj = {
            "tag_name": tag
        }
        tags.append(tag_obj)

    return tags

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
            ('content', self.publish_db['contenttable'], None, None)
        ])
        self.cache_period = cache_period

    def fill_exists_accs(self):
        cw_accounts = self.cyberway_db['account']
        for doc in cw_accounts.find():
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
                    "sumcuratorsw": self.mssgs_curatorsw[(doc["author"], cur_mssg_id.bid)]
                }
            
                isClosedMessage = True
                expiretion = timedelta(minutes = 30)
                date_close = datetime.strptime("2106-02-07T06:28:15", '%Y-%m-%dT%H:%M:%S').isoformat()
                if (doc["cashout_time"].isoformat() != date_close and doc["cashout_time"].isoformat() > datetime.now().isoformat()):
                    rshares_sum += cur_rshares_raw
                    pool["state"]["msgs"] += 1
                    isClosedMessage = False

                message = {
                    "id": cur_mssg_id,
                    "date": doc["last_update"],
                    "parentacc": doc["parent_author"],
                    "parent_id": utils.convert_hash(doc["parent_permlink"]),
                    "tokenprop": utils.get_prop_raw(doc["percent_steem_dollars"] / 2),
                    "beneficiaries": "",
                    "rewardweight": utils.get_prop_raw(doc["reward_weight"]),
                    "state": messagestate,
                    "childcount": doc["children"],
                    "closed": isClosedMessage,
                    "level": doc["depth"],
                    "_SCOPE_": doc["author"],
                    "_PAYER_": "gls.publish",
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
                    "_PAYER_": "gls.publish",
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
        
        pool["state"]["rshares"] = Decimal128(Decimal(rshares_sum))  # TODO: how should we store int128_t?
        pool["state"]["rsharesfn"] = Decimal128(Decimal(rshares_sum))# TODO: --//--//--//--//--//--//--//--
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
                    "time" : doc["last_update"],
                    "count" : 0,
                    "curatorsw": doc["weight"] / 2,
                    "_SCOPE_" : doc["author"],
                    "_PAYER_" : "gls.publish",
                    "_SIZE_" : 50
                }

                self.publish_tables.vote.append(vote)

                self.mssgs_curatorsw[(doc["author"], cur_mssg_id.bid)] += vote["curatorsw"]

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




