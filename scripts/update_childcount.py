#!/usr/bin/env python3
import utils
import dbs
import bson
import sys, traceback
import pymongo
from config import *
from pymongo import UpdateOne

class PublishConverter:
    def __init__(self, publish_db, cache_period = 10000):
        self.childcount_dict = {}
        self.update_list = []
        self.publish_db = publish_db
        self.messages = self.publish_db["message"]
        self.cache_period = cache_period

    def count_childcount(self):
        self.childcount_dict = {}
        cursor = self.messages.find({}, {"childcount":1,"id":1,"_SERVICE_.scope":1,"parent_id":1,"parentacc":1})
        total = cursor.count()
        processed = 0
        for doc in cursor:
            key = (doc["_SERVICE_"]["scope"], int(doc["id"].to_decimal()))
            if not key in self.childcount_dict:
                self.childcount_dict[key] = [0, int(doc["childcount"].to_decimal())]
            else:
                self.childcount_dict[key][1] = int(doc["childcount"].to_decimal())
            if doc["parentacc"]:
                parent = (doc["parentacc"], int(doc["parent_id"].to_decimal()))
                if not parent in self.childcount_dict:
                    self.childcount_dict[parent] = [0, 0]
                else:
                    self.childcount_dict[parent][0] += 1
            processed += 1
            if processed % self.cache_period == 0:
                utils.printProgressBar(processed, total, prefix = 'Progress:', suffix = 'Complete', length = 50)
        utils.printProgressBar(processed, total, prefix = 'Progress:', suffix = 'Complete', length = 50)
        print("\n")
                
    def write_childcount(self):
        self.messages.bulk_write(self.update_list)
        self.update_list = []

    def update_childcount(self):
        total = len(self.childcount_dict)
        updated = 0
        processed = 0
        for key, data in self.childcount_dict.items():
            processed += 1
            if data[0] != data[1]:
                update_filter = {
                    "$and":[{"id":utils.UInt64(key[1])}, {"_SERVICE_.scope":key[0]}]
                }
                update_obj = {
                    "$set":{"childcount":utils.UInt64(data[0])}
                }
                self.update_list.append(UpdateOne(update_filter, update_obj))
            if len(self.update_list) == 10000:
                updated += len(self.update_list)
                self.write_childcount()
            if processed % self.cache_period == 0:
                utils.printProgressBar(processed, total, prefix = 'Progress:', suffix = 'Complete', length = 50)
        if len(self.update_list):
            updated += len(self.update_list)
            self.write_childcount()
        utils.printProgressBar(processed, total, prefix = 'Progress:', suffix = 'Complete', length = 50)
        print("\nUpdate childcount: %d/%d = %.2f%%" % (updated, len(self.childcount_dict), 100.*updated/len(self.childcount_dict)))

converter = PublishConverter(dbs.cyberway_gls_publish_db)
converter.count_childcount()
converter.update_childcount()
