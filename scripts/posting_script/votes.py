import dbs

def convert_votes():
    golos_votes = dbs.golos_db['comment_vote_object']
    cyberway_messages = dbs.cyberway_db['posttable']

    id_count = 0

    for doc in golos_votes.find():
        post = cyberway_messages.find_one({"permlink": doc["permlink"]})
        
#        if not post:
#            continue

        vote = {
            "id" : id_count,
            "post_id" : post["id"],
            "voter" : doc["voter"],
            "persent" : 0,
            "weight" : doc["weight"],
            "time" : doc["last_update"],
            "rshares" : doc["rshares"],
            "count" : 0,
            "_SCOPE_" : post["_PAYER_"],
            "_PAYER_" : doc["author"],
            "_SIZE_" : 65
        }
        dbs.cyberway_db['votetable'].save(vote)

        id_count += 1

        break

    return True

